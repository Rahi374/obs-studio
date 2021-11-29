/*
Copyright (C) 2021 by Paul Elder <paul.elder@ideasonboard.com>

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/
#include "libcamera-src.h"

#include <array>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <sys/mman.h>
#include <sys/types.h>
#include <unistd.h>

#include <libcamera/libcamera.h>

#include <util/threading.h>
#include <util/bmem.h>
#include <util/dstr.h>
#include <util/platform.h>
#include <obs-module.h>

#include "event_loop.h"


LibcameraSrc::LibcameraSrc(obs_source_t *source)
	: camera_(nullptr), config_(nullptr), isRunning_(false),
	  bufferAllocator_(nullptr), source_(source)
{
}

LibcameraSrc::~LibcameraSrc()
{
	if (camera_)
		releaseCamera();
}

const libcamera::Camera *LibcameraSrc::camera()
{
	return camera_.get();
}

void LibcameraSrc::assignCamera(std::shared_ptr<libcamera::Camera> &camera)
{
	camera_ = camera;

	if (camera_->acquire() < 0) {
		blog(LOG_ERROR, "failed to acquire camera");
		camera_->release();
		camera_.reset();
		camera_ = nullptr;
	}

	bufferAllocator_ = new libcamera::FrameBufferAllocator(camera_);
	camera_->requestCompleted.connect(this, &LibcameraSrc::requestComplete);

	blog(LOG_INFO, "acquired camera");
}

void LibcameraSrc::releaseCamera()
{
	if (isRunning_)
		stop();

	if (activated_)
		activated_ = false;

	delete bufferAllocator_;
	bufferAllocator_ = nullptr;

	camera_->release();
	camera_.reset();
	camera_ = nullptr;

	blog(LOG_INFO, "released camera");
}

void LibcameraSrc::restartWithNewCamera(std::shared_ptr<libcamera::Camera> &camera)
{
	blog(LOG_INFO, "restarting with new camera");

	bool wasRunning = isRunning_;

	releaseCamera();
	assignCamera(camera);

	if (wasRunning || activated_)
		start();
}

void LibcameraSrc::start()
{
	blog(LOG_INFO, "starting camera");

	activated_ = true;

	if (!camera_) {
		blog(LOG_INFO, "no camera set");
		return;
	}

	if (isRunning_) {
		blog(LOG_INFO, "already running");
		return;
	}

	if (!configure()) {
		blog(LOG_INFO, "failed to configure camera");
		return;
	}

	/* Allocate buffers */
	libcamera::Stream *stream = config_->at(0).stream();
	int ret = bufferAllocator_->allocate(stream);
	if (ret < 0) {
		blog(LOG_ERROR, "failed to allocate buffers for %s", camera_->id().c_str());
		return;
	}

	blog(LOG_INFO, "allocated buffers");

	/* Frame capture */
	const std::vector<std::unique_ptr<libcamera::FrameBuffer>> &buffers =
		bufferAllocator_->buffers(stream);
	for (unsigned int i = 0; i < buffers.size(); i++) {
		std::unique_ptr<libcamera::Request> request = camera_->createRequest();
		if (!request) {
			blog(LOG_ERROR, "failed to create request");
			return;
		}

		const std::unique_ptr<libcamera::FrameBuffer> &buffer = buffers[i];
		int ret = request->addBuffer(stream, buffer.get());
		if (ret < 0) {
			blog(LOG_ERROR, "failed to add buffer");
			return;
		}

		const libcamera::FrameBuffer::Plane &plane = buffer->planes()[0];
		int fd = plane.fd.fd();
		void *address = mmap(nullptr, plane.length, PROT_READ | PROT_WRITE,
				     MAP_SHARED, fd, plane.offset);
		if (address == MAP_FAILED)
			blog(LOG_ERROR, "failed to map buffer");
		{
			std::lock_guard<std::mutex> guard(buffersMutex_);
			mappedBuffers_.insert({buffer.get(),
					       static_cast<uint8_t *>(address)});
		}

		requests_.push_back(std::move(request));
	}

	blog(LOG_INFO, "done mapping buffers");

	ret = camera_->start();
	if (ret < 0) {
		blog(LOG_ERROR, "failed to start camera");
		return;
	}

	isRunning_ = true;

	for (std::unique_ptr<libcamera::Request> &request : requests_)
		camera_->queueRequest(request.get());

	blog(LOG_INFO, "done starting camera");
}

void LibcameraSrc::stop()
{
	activated_ = false;

	if (!camera_)
		return;

	if (!isRunning_)
		return;

	blog(LOG_INFO, "stopping camera");

	camera_->stop();

	blog(LOG_INFO, "camera stopped");

	requests_.clear();

	libcamera::Stream *stream = config_->at(0).stream();
	bufferAllocator_->free(stream);

	{
		std::lock_guard<std::mutex> guard(buffersMutex_);
		/* \todo munmap */
		mappedBuffers_.clear();
	}

}

void LibcameraSrc::processRequest(libcamera::Request *request)
{
	struct obs_source_frame out;
	memset(&out, 0, sizeof(out));

	libcamera::FrameBuffer *buffer = request->buffers().begin()->second;
	out.width = width_;
	out.height = height_;
	out.format = format_;
	out.timestamp = buffer->metadata().timestamp;
	out.linesize[0] = linesize_;
	out.flip = false;

	/* Hardcode to full range for now */
	out.full_range = true;
	float minColorRange[3] = { 0.0f, 0.0f, 0.0f };
	float maxColorRange[3] = { 1.0f, 1.0f, 1.0f };
	memcpy(out.color_range_min, minColorRange, sizeof(out.color_range_min));
	memcpy(out.color_range_max, maxColorRange, sizeof(out.color_range_max));

	/* Hardcode the color matrix to full range 709 */
	float matrix[16] = {
		1.000000f, 0.000000f, 1.581000f, -0.793600f,
		1.000000f, -0.188062f, -0.469967f, 0.330305f,
		1.000000f, 1.862906f, 0.000000f, -0.935106f,
		0.000000f, 0.000000f, 0.000000f, 1.000000f,
	};
	memcpy(out.color_matrix, matrix, sizeof(out.color_matrix));

	{
		std::lock_guard<std::mutex> guard(buffersMutex_);

		out.data[0] = mappedBuffers_.at(buffer);

		/* \todo Figure out how to do multiplane formats
		if (format_ == VIDEO_FORMAT_NV12) {
			out.linesize[1] = linesize_;
			out.data[1] =
				mappedBuffers_.at(std::next(request->buffers().begin())->second);
		}
		*/
	}

	obs_source_output_video(source_, &out);

	/* Re-queue the Request to the camera. */
	request->reuse(libcamera::Request::ReuseBuffers);
	camera_->queueRequest(request);
}

void LibcameraSrc::requestComplete(libcamera::Request *request)
{
	if (request->status() == libcamera::Request::RequestCancelled)
		return;

	processRequest(request);
}

bool LibcameraSrc::configure()
{
	/* I wish libobs would docment their video format codes */
	static const std::map<libcamera::PixelFormat,
			      std::pair<enum video_format, uint32_t>> formats = {
		{ libcamera::formats::YUYV, { VIDEO_FORMAT_YUY2, 2 } },
		{ libcamera::formats::YVYU, { VIDEO_FORMAT_YVYU, 2 } },
		{ libcamera::formats::UYVY, { VIDEO_FORMAT_UYVY, 2 } },
		/* Why does libobs have only these and not the other permutations? */
		{ libcamera::formats::RGBA8888, { VIDEO_FORMAT_RGBA, 4 } },
		{ libcamera::formats::BGRA8888, { VIDEO_FORMAT_BGRA, 4 } },
		{ libcamera::formats::BGRX8888, { VIDEO_FORMAT_BGRX, 4 } },
		{ libcamera::formats::BGR888, { VIDEO_FORMAT_BGR3, 3 } },
		//{ libcamera::formats::NV12, { VIDEO_FORMAT_NV12, 1 } },
		/* \todo Add I420, Y800, and I444 */
	};

	config_ = camera_->generateConfiguration( { libcamera::StreamRole::Viewfinder } );
	libcamera::StreamConfiguration &streamConfig = config_->at(0);

	bool found = false;
	for (const auto &pair : formats) {
		streamConfig.pixelFormat = pair.first;
		/* \todo Enable configuring resolution */
		streamConfig.size.width = 960;
		streamConfig.size.height = 540;
		if (config_->validate() != libcamera::CameraConfiguration::Invalid &&
		    streamConfig.pixelFormat == pair.first) {
			found = true;
			break;
		}
	}

	if (!found) {
		blog(LOG_ERROR, "failed to find compatible format");
		return false;
	}

	/* \todo proper error handling */
	if (camera_->configure(config_.get()) < 0) {
		blog(LOG_ERROR, "failed to configure camera with format: %s",
				streamConfig.toString().c_str());
		return false;
	}

	width_ = streamConfig.size.width;
	height_ = streamConfig.size.height;
	format_ = formats.at(streamConfig.pixelFormat).first;
	linesize_ = width_ * formats.at(streamConfig.pixelFormat).second;

	blog(LOG_INFO, "configured camera: %s", streamConfig.toString().c_str());

	return true;
}

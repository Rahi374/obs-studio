#pragma once

#include <map>
#include <memory>
#include <mutex>

#include <libcamera/libcamera.h>

#include <obs-module.h>

#define blog(level, msg, ...) blog(level, "libcamera-input: " msg, ##__VA_ARGS__)

class LibcameraSrc
{
public:
	LibcameraSrc(obs_source_t *source);
	~LibcameraSrc();

	const libcamera::Camera *camera();
	void assignCamera(std::shared_ptr<libcamera::Camera> &camera);
	void releaseCamera();
	void restartWithNewCamera(std::shared_ptr<libcamera::Camera> &camera);

	void start();
	void stop();

private:
	void processRequest(libcamera::Request *request);
	void requestComplete(libcamera::Request *request);

	bool configure();

	std::shared_ptr<libcamera::Camera> camera_;
	std::unique_ptr<libcamera::CameraConfiguration> config_;

	bool isRunning_;
	bool activated_;

	std::vector<std::unique_ptr<libcamera::Request>> requests_;

	std::mutex bufferLock_;
	libcamera::FrameBufferAllocator *bufferAllocator_;

	std::mutex buffersMutex_;
	std::map<libcamera::FrameBuffer *, uint8_t *> mappedBuffers_;

	/* obs */
	uint32_t width_;
	uint32_t height_;
	uint32_t linesize_;
	enum video_format format_;

	obs_source_t *source_;
};

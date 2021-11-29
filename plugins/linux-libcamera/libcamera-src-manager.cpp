#include "libcamera-src-manager.h"

#include <memory>

#include <libcamera/libcamera.h>

#include <obs-module.h>

#include "libcamera-src.h"

LibcameraSrcManager::LibcameraSrcManager()
	: cm_(nullptr), cookieCounter_(1)
{
}

LibcameraSrcManager::~LibcameraSrcManager()
{
	srcs_.clear();

	if (cm_) {
		cm_->stop();
		cm_ = nullptr;
	}
}

LibcameraSrcManager *LibcameraSrcManager::instance()
{
	static LibcameraSrcManager instance;
	return &instance;
}

void LibcameraSrcManager::init()
{
	if (cm_)
		return;

	cm_ = std::make_unique<libcamera::CameraManager>();
	cm_->start();
}

LibcameraSrc *LibcameraSrcManager::findSource(void *src)
{
	auto it = srcs_.find(src);
	if (it != srcs_.end())
		return it->second.get();

	return nullptr;
}

void *LibcameraSrcManager::create(obs_data_t *settings, obs_source_t *source)
{
	init();

	std::unique_ptr<LibcameraSrc> src = std::make_unique<LibcameraSrc>(source);

	/* \todo Handle other settings */
	std::string cameraName = bstrdup(obs_data_get_string(settings, "device_id"));
	std::shared_ptr<libcamera::Camera> camera = cm_->get(cameraName);
	if (!cameraName.empty())
		src->assignCamera(camera);

	void *ptr = reinterpret_cast<void *>(cookieCounter_++);
	srcs_.insert({ptr, std::move(src)});

	return ptr;
}

void LibcameraSrcManager::destroy(void *vptr)
{
	auto it = srcs_.find(vptr);
	if (it != srcs_.end())
		srcs_.erase(it);
}

void LibcameraSrcManager::enumerateCameras(obs_property_t *prop)
{
	obs_property_list_clear(prop);
	for (auto const &camera : cm_->cameras()) {
		/* \todo Get external camera model name */
		obs_property_list_add_string(prop, camera->id().c_str(),
					     camera->id().c_str());
	}
}

void LibcameraSrcManager::update(void *vptr, obs_data_t *settings)
{
	LibcameraSrc *src = findSource(vptr);
	if (!src)
		return;

	std::string newCameraName = bstrdup(obs_data_get_string(settings, "device_id"));

	const libcamera::Camera *oldCamera = src->camera();
	std::shared_ptr<libcamera::Camera> newCamera = cm_->get(newCameraName);

	/* The source doesn't have a camera yet, so assign it */
	if (!oldCamera) {
		if (!newCameraName.empty()) {
			src->assignCamera(newCamera);
			src->start();
		}

		return;
	}

	/* If the camera is changed, then the manager needs to reassign it */
	std::string oldCameraName = oldCamera->id();
	if (oldCameraName != newCameraName &&
	    newCamera != nullptr)
		src->restartWithNewCamera(newCamera);

	/*
	 * At the moment we don't have any other settings, so we don't have to
	 * do anything else here.
	 * \todo Update other settings
	 */
}

void LibcameraSrcManager::activate(void *vptr)
{
	LibcameraSrc *src = findSource(vptr);
	if (src)
		src->start();
}

void LibcameraSrcManager::deactivate(void *vptr)
{
	LibcameraSrc *src = findSource(vptr);
	if (src)
		src->stop();
}

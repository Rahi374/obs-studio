#pragma once

#include <map>
#include <memory>

#include <libcamera/libcamera.h>

#include <obs-module.h>

#include "libcamera-src.h"

class LibcameraSrcManager
{
public:
	static LibcameraSrcManager *instance();

	void *create(obs_data_t *settings, obs_source_t *source);
	void destroy(void *vptr);
	void enumerateCameras(obs_property_t *prop);
	void update(void *vptr, obs_data_t *settings);
	void activate(void *vptr);
	void deactivate(void *vptr);

private:
	LibcameraSrcManager();
	~LibcameraSrcManager();

	void init();
	LibcameraSrc *findSource(void *src);

	std::unique_ptr<libcamera::CameraManager> cm_;
	std::map<void *, std::unique_ptr<LibcameraSrc>> srcs_;

	size_t cookieCounter_;
};

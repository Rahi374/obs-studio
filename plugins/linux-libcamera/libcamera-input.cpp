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
#include <util/threading.h>
#include <util/bmem.h>
#include <util/dstr.h>
#include <util/platform.h>
#include <obs-module.h>

#include <libcamera/libcamera.h>

#include "libcamera-src.h"
#include "libcamera-src-manager.h"

extern "C" {

static const char *libcamera_getname(void *unused)
{
	UNUSED_PARAMETER(unused);
	return obs_module_text("libcameraInput");
}

static void *libcamera_create(obs_data_t *settings, obs_source_t *source)
{
	blog(LOG_INFO, "create!");

	return LibcameraSrcManager::instance()->create(settings, source);
}

static void libcamera_destroy(void *vptr)
{
	blog(LOG_INFO, "destroy!");

	LibcameraSrcManager::instance()->destroy(vptr);
}

static obs_properties_t *libcamera_properties(void *vptr)
{
	UNUSED_PARAMETER(vptr);

	obs_properties_t *props = obs_properties_create();

	obs_property_t *device_list = obs_properties_add_list(
		props, "device_id", obs_module_text("Device"),
		OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_STRING);

	LibcameraSrcManager::instance()->enumerateCameras(device_list);

	return props;
}

static void libcamera_update(void *vptr, obs_data_t *settings)
{
	blog(LOG_INFO, "update!");

	LibcameraSrcManager::instance()->update(vptr, settings);
}

static void libcamera_activate(void *vptr)
{
	blog(LOG_INFO, "activate!");

	LibcameraSrcManager::instance()->activate(vptr);
}

static void libcamera_deactivate(void *vptr)
{
	blog(LOG_INFO, "deactivate!");

	LibcameraSrcManager::instance()->deactivate(vptr);
}

struct obs_source_info libcamera_input = {
	.id = "libcamera_input",
	.type = OBS_SOURCE_TYPE_INPUT,
	.output_flags = OBS_SOURCE_ASYNC_VIDEO | OBS_SOURCE_DO_NOT_DUPLICATE,
	.get_name = libcamera_getname,
	.create = libcamera_create,
	.destroy = libcamera_destroy,
	.get_properties = libcamera_properties,
	.update = libcamera_update,
	.activate = libcamera_activate,
	.deactivate = libcamera_deactivate,
	.icon_type = OBS_ICON_TYPE_CAMERA,
};

}

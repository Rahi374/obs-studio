project(linux-libcamera)

if(DISABLE_LIBCAMERA)
	message(STATUS "libcamera plugin disabled")
	return()
endif()


if(ENABLE_LIBCAMERA)
	find_package(libcamera REQUIRED)
	find_package(Libevent REQUIRED)
else()
	find_package(libcamera)
	find_package(Libevent)
	if(NOT LIBCAMERA_FOUND)
		message(STATUS "libcamera not found, disabling libcamera plugin")
		return()
	endif()
	if(NOT LIBEVENT_FOUND)
		message(STATUS "libevent not found, disabling libcamera plugin")
		return()
	endif()
endif()

include_directories(
	SYSTEM "${CMAKE_SOURCE_DIR}/libobs"
	${LIBCAMERA_INCLUDE_DIRS}
)

set(linux-libcamera_SOURCES
	linux-libcamera.c
	libcamera-input.cpp
	libcamera-src.cpp
	libcamera-src-manager.cpp
	event_loop.cpp
)

add_library(linux-libcamera MODULE
	${linux-libcamera_SOURCES}
)
target_link_libraries(linux-libcamera
	libobs
	${LIBCAMERA_LIBRARIES}
	${LIBEVENT_LIBRARIES}
	${LIBEVENT_PTHREAD_LIBRARIES}
)
set_target_properties(linux-libcamera PROPERTIES FOLDER "plugins")

install_obs_plugin_with_data(linux-libcamera data)

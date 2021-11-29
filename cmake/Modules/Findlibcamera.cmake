# Once done these will be defined:
#
#  LIBCAMERA_FOUND
#  LIBCAMERA_INCLUDE_DIRS
#  LIBCAMERA_LIBRARIES

find_package(PkgConfig QUIET)
if (PKG_CONFIG_FOUND)
	pkg_check_modules(_LIBCAMERA QUIET libcamera)
endif()

find_path(LIBCAMERA_INCLUDE_DIR
	NAMES libcamera/libcamera.h
	HINTS
		${_LIBCAMERA_INCLUDE_DIRS}
	PATHS
		/usr/include /usr/local/include /opt/local/include)

find_library(LIBCAMERA_LIB
	NAMES libcamera.so
	HINTS
		${_LIBCAMERA_LIBRARY_DIRS}
	PATHS
		/usr/lib /usr/local/lib /opt/local/lib)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Libcamera DEFAULT_MSG LIBCAMERA_LIB LIBCAMERA_INCLUDE_DIR)
mark_as_advanced(LIBCAMERA_INCLUDE_DIR LIBCAMERA_LIB)

if(LIBCAMERA_FOUND)
	set(LIBCAMERA_INCLUDE_DIRS ${LIBCAMERA_INCLUDE_DIR})
	set(LIBCAMERA_LIBRARIES ${LIBCAMERA_LIB})
endif()

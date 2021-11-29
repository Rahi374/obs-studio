# Once done these will be defined:
#
#  LIBEVENT_FOUND
#  LIBEVENT_INCLUDE_DIRS
#  LIBEVENT_LIBRARIES

find_package(PkgConfig QUIET)
if (PKG_CONFIG_FOUND)
	pkg_check_modules(_LIBEVENT QUIET libevent_pthreads)
endif()

find_path(LIBEVENT_INCLUDE_DIR
	NAMES event2/event.h event2/thread.h
	HINTS
		${_LIBEVENT_INCLUDE_DIRS}
	PATHS
		/usr/include /usr/local/include /opt/local/include)

find_library(LIBEVENT_LIB
	NAMES libevent.so
	HINTS
		${_LIBEVENT_LIBRARY_DIRS}
	PATHS
		/usr/lib /usr/local/lib /opt/local/lib)

find_library(LIBEVENT_PTHREAD_LIB
	NAMES libevent_pthreads.so
	HINTS
		${_LIBEVENT_LIBRARY_DIRS}
	PATHS
		/usr/lib /usr/local/lib /opt/local/lib)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Libevent DEFAULT_MSG LIBEVENT_LIB LIBEVENT_PTHREAD_LIB LIBEVENT_INCLUDE_DIR)
mark_as_advanced(LIBEVENT_INCLUDE_DIR LIBEVENT_LIB LIBEVENT_PTHREAD_LIB)

if(LIBEVENT_FOUND)
	set(LIBEVENT_INCLUDE_DIRS ${LIBEVENT_INCLUDE_DIR})
	set(LIBEVENT_LIBRARIES ${LIBEVENT_LIB})
	set(LIBEVENT_PTHREAD_LIBRARIES ${LIBEVENT_PTHREAD_LIB})
endif()

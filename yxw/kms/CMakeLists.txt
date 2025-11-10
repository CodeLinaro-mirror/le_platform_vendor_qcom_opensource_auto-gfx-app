
add_library(yxw_kms  STATIC drv.c kmsdrv.h)

find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBDRM REQUIRED libdrm)

target_include_directories(yxw_kms PUBLIC
				${YXW}
				${LIBDRM_INCLUDE_DIRS}
	)
target_link_libraries (yxw_kms gbm ${LIBDRM_LIBRARIES})

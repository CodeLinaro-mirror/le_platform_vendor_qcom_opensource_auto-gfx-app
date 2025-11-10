
add_subdirectory(null)
add_subdirectory(kms)
add_subdirectory(wayland)

add_library(yxw SHARED yxw.c yxw.h)
target_link_libraries(yxw yxw_wayland yxw_nul yxw_kms)

set_target_properties(yxw PROPERTIES VERSION 1.0.0 SOVERSION 1)

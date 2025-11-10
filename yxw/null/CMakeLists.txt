
add_library(yxw_nul  STATIC drv.c)

target_include_directories(yxw_nul PUBLIC ${YXW})
target_link_libraries (yxw_nul gbm)

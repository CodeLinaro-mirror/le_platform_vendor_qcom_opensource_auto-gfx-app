
include(CheckIncludeFile)

add_executable(kpi simple.c)

check_include_file("bootkpi/logging.h" HAVE_BOOTKPI_H)
if(DEFINED HAVE_BOOTKPI_H AND HAVE_BOOTKPI_H EQUAL 1)
    target_compile_definitions(kpi PRIVATE HAVE_BOOTKPI_H=1)
endif()

target_link_libraries (kpi m yxw EGL GLESv2)


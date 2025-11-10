

find_program(WAYLAND_SCANNER NAMES wayland-scanner REQUIRED)

set(XDG_SHELL_XML "${CMAKE_CURRENT_SOURCE_DIR}/protocol/xdg-shell.xml")
set(GEN_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")
file(MAKE_DIRECTORY "${GEN_DIR}")

add_custom_command(
  OUTPUT
    ${GEN_DIR}/xdg-shell-protocol.h
    ${GEN_DIR}/xdg-shell-protocol.c
  COMMAND ${WAYLAND_SCANNER} client-header
          "${XDG_SHELL_XML}" "${GEN_DIR}/xdg-shell-protocol.h"
  COMMAND ${WAYLAND_SCANNER} private-code
          "${XDG_SHELL_XML}" "${GEN_DIR}/xdg-shell-protocol.c"
  DEPENDS "${XDG_SHELL_XML}"
  COMMENT "Generating xdg-shell protocol via ${WAYLAND_SCANNER}"
  VERBATIM
)

set (MYSRC
   ${GEN_DIR}/xdg-shell-protocol.h
   ${GEN_DIR}/xdg-shell-protocol.c
   display.c window.c drv.h drv.c
)

include_directories(AFTER ${GEN_DIR})

add_library(yxw_wayland STATIC ${MYSRC})
target_include_directories(yxw_wayland PUBLIC ${YXW})
target_link_libraries (yxw_wayland wayland-client wayland-cursor wayland-egl)


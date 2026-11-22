# Adapted from Selecta cmake/host_bake_tools.cmake at 679ff5fc (iconbake only; the font baker is not part of this package).
# Selecta will consume this repo, not the reverse.
# The Amiga cross build cannot run a tool it builds, so the icon baker is compiled with the host C compiler as a build step.
find_program(SEL_HOST_CC NAMES cc clang gcc REQUIRED)
function(sel_host_iconbake out_var)
  set(exe "${CMAKE_CURRENT_BINARY_DIR}/gui_iconbake_host")
  set(srcs "${PROJECT_SOURCE_DIR}/tools/gui_iconbake.c" "${PROJECT_SOURCE_DIR}/src/vendor/gui_icon_pack.c")
  add_custom_command(
    OUTPUT "${exe}"
    COMMAND "${SEL_HOST_CC}" -O1 -I "${PROJECT_SOURCE_DIR}/include" -o "${exe}" ${srcs} -lm
    DEPENDS ${srcs}
    COMMENT "Building the host icon baker")
  set(${out_var} "${exe}" PARENT_SCOPE)
endfunction()

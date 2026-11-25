# Host tests of the package. Adapted from Selecta tests/CMakeLists.txt at 679ff5fc (the mcc-* and mui-fontprobe cases that do not
# read Selecta's own UI sources); Selecta will consume this repo, not the reverse.
find_package(Python3 COMPONENTS Interpreter REQUIRED)
set(MCC_DIR "${PROJECT_SOURCE_DIR}")
# Amiga: the three tests that pull mui_aa_draw.o need the library bases the host program defines (sel_mui_GfxBase, MUIMasterBase) (see host_bases_stub.c).
function(mcc_host_bases_stub name)
  if(SEL_AMIGA)
    target_sources(${name} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/host_bases_stub.c")
  endif()
endfunction()
function(mcc_test_exe name)
  add_executable(${name} ${name}.c mcc_test_bases.c)
  target_link_libraries(${name} PRIVATE sel_mcc_static)
  target_compile_definitions(${name} PRIVATE "MCC_DIR=\"${MCC_DIR}\"")
endfunction()
function(mcc_test)
  add_test(NAME ${ARGV})
  list(GET ARGV 0 n)
  set_tests_properties(${n} PROPERTIES TIMEOUT 30 LABELS mui)
endfunction()

add_test(NAME mcc-include-check COMMAND "${CMAKE_COMMAND}" "-DMCC_DIR=${MCC_DIR}" "-DSDK_DIR=${MCC_DIR}/include/selecta"
  "-DROOTS=${MCC_CHECK_ROOTS}" -P "${PROJECT_SOURCE_DIR}/cmake/mcc_check_includes.cmake")
set_tests_properties(mcc-include-check PROPERTIES TIMEOUT 30)

add_executable(mui_fontprobe_test mui_fontprobe_test.c "${MCC_DIR}/src/map/mui_fontprobe.c")
target_include_directories(mui_fontprobe_test PRIVATE "${MCC_DIR}/src")
mcc_test(mui-fontprobe COMMAND mui_fontprobe_test)

mcc_test_exe(mui_truecolour_test)
mcc_test(mcc-truecolour-path COMMAND mui_truecolour_test)

# a ;-list must stay one argument of the test command
string(REPLACE ";" "\\;" _lib_src "${MCC_CLASS_LIB_SOURCES}")
string(REPLACE ";" "\\;" _static_src "${MCC_STATIC_SOURCES}")
mcc_test(mcc-classlibs-source COMMAND "${CMAKE_COMMAND}" "-DLIB_SOURCES=${_lib_src}" "-DSTATIC_SOURCES=${_static_src}"
  -P "${PROJECT_SOURCE_DIR}/cmake/mcc_check_classlibs.cmake")

# I1491a: the slim gate script (cmake/mcc_check_slim.cmake) on fixture nm and size output: one pass, three expected failures.
set(_slim "${PROJECT_SOURCE_DIR}/cmake/mcc_check_slim.cmake")
set(_slimdir "${CMAKE_CURRENT_SOURCE_DIR}/slim")
mcc_test(mcc-slim-ok COMMAND "${CMAKE_COMMAND}" "-DNM_TEXT=${_slimdir}/ok.nm" "-DSIZE_TEXT=${_slimdir}/ok.size" -P "${_slim}")
mcc_test(mcc-slim-bans-theme-parser COMMAND "${CMAKE_COMMAND}" "-DNM_TEXT=${_slimdir}/bad_parse.nm" "-DSIZE_TEXT=${_slimdir}/ok.size" -P "${_slim}")
mcc_test(mcc-slim-bans-stdio COMMAND "${CMAKE_COMMAND}" "-DNM_TEXT=${_slimdir}/bad_stdio.nm" "-DSIZE_TEXT=${_slimdir}/ok.size" -P "${_slim}")
mcc_test(mcc-slim-bans-draw COMMAND "${CMAKE_COMMAND}" "-DNM_TEXT=${_slimdir}/bad_draw.nm" "-DSIZE_TEXT=${_slimdir}/ok.size" -P "${_slim}")
mcc_test(mcc-slim-bans-sibling COMMAND "${CMAKE_COMMAND}" "-DNM_TEXT=${_slimdir}/bad_sibling.nm" "-DSIZE_TEXT=${_slimdir}/ok.size" "-DOWN=_aatabs_dispatch" -P "${_slim}")
mcc_test(mcc-slim-own-dispatch-ok COMMAND "${CMAKE_COMMAND}" "-DNM_TEXT=${_slimdir}/bad_sibling.nm" "-DSIZE_TEXT=${_slimdir}/ok.size" "-DOWN=_aabtn_dispatch" -P "${_slim}")
mcc_test(mcc-slim-size-limit COMMAND "${CMAKE_COMMAND}" "-DNM_TEXT=${_slimdir}/ok.nm" "-DSIZE_TEXT=${_slimdir}/big.size" -P "${_slim}")
set_tests_properties(mcc-slim-bans-theme-parser mcc-slim-bans-stdio mcc-slim-bans-draw mcc-slim-bans-sibling mcc-slim-size-limit PROPERTIES WILL_FAIL TRUE)
mcc_test_exe(mui_aasel_test)
mcc_test(mui-aasel-plan COMMAND mui_aasel_test)
mcc_test_exe(mui_aafield_test)
mcc_test(mui-aafield-fix COMMAND mui_aafield_test)
add_executable(mui_aafield_edit_test mui_aafield_edit_test.c "${MCC_DIR}/src/mui_text_lines.c")
target_include_directories(mui_aafield_edit_test PRIVATE "${MCC_DIR}/src")
mcc_test(mui-aafield-edit COMMAND mui_aafield_edit_test)
mcc_test_exe(mui_aascr_test)
mcc_test(mui-aascr-geom COMMAND mui_aascr_test)
add_executable(mui_text_lines_test mui_text_lines_test.c "${MCC_DIR}/src/mui_text_lines.c")
target_include_directories(mui_text_lines_test PRIVATE "${MCC_DIR}/src")
mcc_test(mui-text-lines COMMAND mui_text_lines_test)
add_executable(mui_ftxt_test mui_ftxt_test.c "${MCC_DIR}/src/mui_ftxt.c")
target_include_directories(mui_ftxt_test PRIVATE "${MCC_DIR}/src")
mcc_test(mui-ftxt COMMAND mui_ftxt_test)

mcc_test_exe(mcc_log_test)
foreach(t gate-off gate-on hook source)
  mcc_test(mcc-log-${t} COMMAND mcc_log_test ${t} "${CMAKE_CURRENT_BINARY_DIR}/mcc_log_${t}.txt")
endforeach()
mcc_test_exe(mcc_link_test)
mcc_test(mcc-link-no-map COMMAND mcc_link_test)
mcc_test_exe(mcc_font_list_test)
mcc_test(mcc-font-list-order COMMAND mcc_font_list_test order "${CMAKE_CURRENT_BINARY_DIR}/mcc_font_list_theme.txt")
mcc_test(mcc-font-list-source COMMAND mcc_font_list_test source)
mcc_test_exe(mcc_support_api_test)
mcc_host_bases_stub(mcc_support_api_test)
mcc_test(mcc-support-api COMMAND mcc_support_api_test)
add_executable(mcc_shared_store_test mcc_shared_store_test.c mcc_test_bases.c "${MCC_DIR}/src/map/up_store_client.c")
target_link_libraries(mcc_shared_store_test PRIVATE sel_mcc_static)
mcc_host_bases_stub(mcc_shared_store_test)
mcc_test(mcc-shared-store COMMAND mcc_shared_store_test)
mcc_test_exe(mcc_origin_test)
mcc_test(mcc-shared-origin COMMAND mcc_origin_test)
mcc_test_exe(mcc_lib_init_test)
mcc_host_bases_stub(mcc_lib_init_test)
mcc_test(mcc-lib-init-theme COMMAND mcc_lib_init_test "${MCC_DIR}/dist/themes/Light.theme")
mcc_test_exe(mcc_prefs_test)
mcc_test(mcc-prefs-writer COMMAND mcc_prefs_test writer)
mcc_test(mcc-prefs-mapping COMMAND mcc_prefs_test mapping)
mcc_test(mcc-prefs-themes COMMAND mcc_prefs_test themes "${MCC_DIR}/dist/themes")
mcc_test(mcc-theme-complete COMMAND mcc_prefs_test complete "${MCC_DIR}/dist/themes")
mcc_test(mcc-prefs-cmake COMMAND mcc_prefs_test cmake "${MCC_DIR}/CMakeLists.txt" "${MCC_PREFS_NAME}")

mcc_test(mcc-autodoc COMMAND "${Python3_EXECUTABLE}" -I "${PROJECT_SOURCE_DIR}/tools/mcc_autodoc.py" check)
mcc_test(mcc-dist-layout COMMAND "${CMAKE_COMMAND}" -DMODE=selftest "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/mcc_dist_selftest"
  "-DROOT=${PROJECT_SOURCE_DIR}" "-DCLASS_NAMES=${MCC_CLASS_NAMES}" -P "${PROJECT_SOURCE_DIR}/cmake/mcc_dist.cmake")
mcc_test(mcc-demo-headers-only COMMAND "${CMAKE_COMMAND}" "-DINCLUDES=${MCC_DEMO_INCLUDES}" "-DLIBS=${MCC_DEMO_LIBS}"
  "-DSOURCE=${MCC_DIR}/demo/updemo.c" "-DRELEASE_INC=${MCC_RELEASE_INC}" "-DSOURCE_ROOT=${PROJECT_SOURCE_DIR}"
  "-DBINARY_ROOT=${PROJECT_BINARY_DIR}" -P "${PROJECT_SOURCE_DIR}/cmake/mcc_check_demo.cmake")

if(NOT CMAKE_CROSSCOMPILING) # the fake system headers are host-only
# I1491a2: the library side of the draw layer on host, against the real draw headers with stand-in system headers (tests/fake) and a
# recording aa_* layer (design 2026-11-22-i1491a2-draw-behind-library.md section 6).
add_executable(mcc_draw_entry_test mcc_draw_entry_test.c "${MCC_DIR}/lib/support_draw.c")
target_include_directories(mcc_draw_entry_test BEFORE PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/fake")
target_include_directories(mcc_draw_entry_test PRIVATE "${MCC_DIR}/include" "${MCC_DIR}/src" "${MCC_DIR}/src/map")
mcc_test(mcc-draw-entry COMMAND mcc_draw_entry_test entry)
mcc_test(mcc-draw-ctx-size COMMAND mcc_draw_entry_test size)

# I1491a2 row R2: the class-library shims over the same fake headers.
add_executable(mcc_draw_shims_test mcc_draw_shims_test.c "${MCC_DIR}/lib/classlib_draw_shims.c")
target_include_directories(mcc_draw_shims_test BEFORE PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/fake")
target_include_directories(mcc_draw_shims_test PRIVATE "${MCC_DIR}/include" "${MCC_DIR}/src" "${MCC_DIR}/src/map")
mcc_test(mcc-draw-shims COMMAND mcc_draw_shims_test)
endif()
mcc_test_exe(mui_pop_test)
mcc_test(mui-pop-state COMMAND mui_pop_test)
mcc_test_exe(mui_popasl_test)
mcc_test(mui-popasl COMMAND mui_popasl_test)

mcc_test_exe(mui_numeric_scale_test)
mcc_test(mui-numeric-scale COMMAND mui_numeric_scale_test)

mcc_test_exe(mui_palette_test)
mcc_test(mui-palette COMMAND mui_palette_test)
mcc_test_exe(mui_cycle_test)
mcc_test(mui-cycle-empty COMMAND mui_cycle_test)

mcc_test_exe(mui_image_fit_test)
mcc_test(mui-image-fit COMMAND mui_image_fit_test)
mcc_test_exe(mui_dial_test)
mcc_test(mui-dial COMMAND mui_dial_test)

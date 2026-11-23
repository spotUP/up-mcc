# Host tests of the package. Adapted from Selecta tests/CMakeLists.txt at 679ff5fc (the mcc-* and mui-fontprobe cases that do not
# read Selecta's own UI sources); Selecta will consume this repo, not the reverse.
find_package(Python3 COMPONENTS Interpreter REQUIRED)
set(MCC_DIR "${PROJECT_SOURCE_DIR}")
function(mcc_test_exe name)
  add_executable(${name} ${name}.c)
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

mcc_test_exe(mui_aasel_test)
mcc_test(mui-aasel-plan COMMAND mui_aasel_test)

mcc_test_exe(mcc_log_test)
foreach(t gate-off gate-on source)
  mcc_test(mcc-log-${t} COMMAND mcc_log_test ${t} "${CMAKE_CURRENT_BINARY_DIR}/mcc_log_${t}.txt")
endforeach()
mcc_test_exe(mcc_link_test)
mcc_test(mcc-link-no-map COMMAND mcc_link_test)
mcc_test_exe(mcc_font_list_test)
mcc_test(mcc-font-list-order COMMAND mcc_font_list_test order "${CMAKE_CURRENT_BINARY_DIR}/mcc_font_list_theme.txt")
mcc_test(mcc-font-list-source COMMAND mcc_font_list_test source)
mcc_test_exe(mcc_support_api_test)
mcc_test(mcc-support-api COMMAND mcc_support_api_test)
add_executable(mcc_shared_store_test mcc_shared_store_test.c "${MCC_DIR}/src/map/up_store_client.c")
target_link_libraries(mcc_shared_store_test PRIVATE sel_mcc_static)
mcc_test(mcc-shared-store COMMAND mcc_shared_store_test)
mcc_test_exe(mcc_origin_test)
mcc_test(mcc-shared-origin COMMAND mcc_origin_test)
mcc_test_exe(mcc_lib_init_test)
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

mcc_test_exe(mui_pop_test)
mcc_test(mui-pop-state COMMAND mui_pop_test)

mcc_test_exe(mui_numeric_scale_test)
mcc_test(mui-numeric-scale COMMAND mui_numeric_scale_test)

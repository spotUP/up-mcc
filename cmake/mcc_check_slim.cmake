# I1491a (design 2026-11-22-up-mcc-full-component-set.md section 4): a class library stays slim. Two gates:
#  - nm gate: no _txt_theme_parse, _txt_theme_write or ___vfprintf_total_size (the theme parser and libnix stdio live in up-mcc.library only),
#    and none of the draw implementation names below (the draw layer lives in up-mcc.library only)
#  - size gate: the text section is below MAX_TEXT bytes (default 8192)
# On the build: cmake -DNM=<nm> -DSIZE=<size> -DBINARY=<.mcc> [-DMAX_TEXT=n] -P mcc_check_slim.cmake
# Self test (host ctest): cmake -DNM_TEXT=<file of nm output> -DSIZE_TEXT=<file of size output> [-DMAX_TEXT=n] -P mcc_check_slim.cmake
if(NOT DEFINED MAX_TEXT)
  set(MAX_TEXT 8192)
endif()
if(DEFINED BINARY)
  execute_process(COMMAND "${NM}" "${BINARY}" OUTPUT_VARIABLE nm_out RESULT_VARIABLE rc1)
  execute_process(COMMAND "${SIZE}" "${BINARY}" OUTPUT_VARIABLE size_out RESULT_VARIABLE rc2)
  if(NOT rc1 EQUAL 0 OR NOT rc2 EQUAL 0)
    message(FATAL_ERROR "mcc-slim: nm or size failed on ${BINARY}")
  endif()
  set(what "${BINARY}")
else()
  file(READ "${NM_TEXT}" nm_out)
  file(READ "${SIZE_TEXT}" size_out)
  set(what "${NM_TEXT}")
endif()
set(banned "")
# I1491a2: the draw layer lives in up-mcc.library. The library copy defines the real implementation; a class library only has the thin shims
# (lib/classlib_draw_shims.c), which define aa_setup, aa_text and the others, so the ban names the implementation only: the paint maths,
# the font probe, the retheme walk, the pen cache and the tooltip runner.
set(MCC_SLIM_BANNED_DRAW _mui_aa_paint_round_fill _mui_fontprobe_open _aa_retheme_all _aa_plain_textfont _aa_pen _tip_run)
# I1491a2 R3: a class reaches its siblings by name (MUI_NewObject("UP-Button.mcc")), so no class library carries another class's dispatcher.
# OWN names the dispatcher of the library's own class and is the one exception.
set(MCC_SLIM_BANNED_SIBLING _aabtn_dispatch _aalist_dispatch _aascroller_dispatch _aatext_dispatch)
if(DEFINED OWN)
  list(REMOVE_ITEM MCC_SLIM_BANNED_SIBLING ${OWN})
endif()
foreach(sym _txt_theme_parse _txt_theme_write ___vfprintf_total_size ${MCC_SLIM_BANNED_DRAW} ${MCC_SLIM_BANNED_SIBLING})
  if(nm_out MATCHES "[0-9a-fA-F]+ [A-Za-z] ${sym}\n")
    list(APPEND banned "${sym}")
  endif()
endforeach()
if(NOT size_out MATCHES "text[ \t]+data[ \t]+bss[^\n]*\n[ \t]*([0-9]+)")
  message(FATAL_ERROR "mcc-slim: no size line for ${what}")
endif()
set(text "${CMAKE_MATCH_1}")
if(banned)
  message(FATAL_ERROR "mcc-slim FAILED: ${what} defines banned symbols: ${banned}")
endif()
if(NOT text LESS MAX_TEXT)
  message(FATAL_ERROR "mcc-slim FAILED: ${what} text is ${text} bytes, limit ${MAX_TEXT}")
endif()
message(STATUS "mcc-slim OK: ${what}: text ${text} < ${MAX_TEXT}, no banned symbols")

# up-mcc

MUI custom class package for the Amiga: thirteen `UP-<Name>.mcc` classes (Button, Text, Panel, Tabs, Balance, Slider, Progress,
Cycle, Field, Scroller, List, Checkmark, Radio, Popstring, Poplist), the `UP-MCC.mcp` preferences class, the `up-mcc.library` support library, themes and a demo.
The same class sources also build as a static library (`sel_mcc_static`) that Selecta links. History comes from Selecta's
`plugins/shared/mcc`; Selecta will consume this repo, not the reverse.

## Build and test (host)

Needs CMake 3.20+, a C99 compiler, Python 3.

    cmake -S . -B build -G Ninja
    cmake --build build
    ctest --test-dir build

The host build compiles `sel_mcc_static` (theme store, tokens, icon table baked from Lucide at build time, support API) and runs
the `mcc-*` and `mui-fontprobe` tests. Tests that read Selecta's own MUI UI sources (`mui_contract_test`, `mui_aa_test`,
`mui_tagmap_test`) stay in Selecta.

## Build (Amiga)

    cmake -S . -B build/amiga -DSEL_AMIGA=ON -DCMAKE_TOOLCHAIN_FILE=cmake/amiga-68020.cmake

Needs bebbo amiga-gcc with the MUI 3.8 SDK and `sfdc` (see `cmake/amiga-68020.cmake`). Target `up_mcc_dist` stages `UP-MCC/`.

## Vendored from Selecta

`include/selecta/{uimodel,gui}`, `src/vendor`, `tools/gui_iconbake.c`, `tools/mcc_autodoc.py`, `cmake/`, `tests/` and `third_party/`
carry a header line naming the Selecta path and commit they were copied from.

## Licence

MIT for the package code (`LICENSE`). Third party: Lucide icons, ISC (`third_party/lucide/LICENSE`); Inter font, SIL OFL 1.1
(`third_party/fonts/LICENSE-OFL.txt`).

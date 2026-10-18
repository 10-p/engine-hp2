# tools/dev/layout — layout checks of the native classes against the shipped packages

The offline scripts read the `.u` packages and the generated headers; no build, no boot. They are not the same as the
similarly named scripts in `tools/dev/analysis/` (a different reader API).

- `upkg.py` — UE1 package (`.u`) reader; `layout.py` — script property layout (offsets, `PropertiesSize`).
- `hdr.py` — parses a generated `*Classes.h`; `model.py` — MSVC /Zp4 vs GCC/wasm layout model.
- `compare.py` — header vs package, member by member, under both models.
- `emit_asserts.py` — regenerates `Source/Engine/Src/EngineLayoutAsserts.cpp`. Run it after any change to
  `EngineClasses.h`; it writes `EngineLayoutAsserts.cpp` into the current directory and prints the assert count:
  `python3 tools/dev/layout/emit_asserts.py Source/Engine/Inc/EngineClasses.h <assets>/System/Core.u <assets>/System/Engine.u`
  Move the file to `Source/Engine/Src/` and put back its preamble, the `-Winvalid-offsetof` block and the
  `PLATFORM_64BITS` guard. The asserts are the 32-bit layout and must hold natively and with `-fpack-struct=4`.
- `forscope.py`, `undc.py` — the for-scope scanner and the Dreamcast-block remover used during the port.

`layout-sweep.gdb` checks a running engine instead: every script property of every native class (and every script
struct with a same-named C++ `F<Name>`) against the C++ member offset from the debug info, on 32-bit and 64-bit
builds alike. Run it in a run tree (`tools/dev/mkrun-hp2.sh`):

    cd <run-tree>/System && SDL_VIDEODRIVER=dummy gdb -batch -x <repo>/tools/dev/layout/layout-sweep.gdb --args ./Game -NULLRENDERER

A healthy tree prints `checked=<n> shifted=0` and `structs=<n> checked=<n> shifted=0`.

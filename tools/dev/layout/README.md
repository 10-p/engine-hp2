# tools/dev/layout — layout checks against the shipped packages (offline; no build, no boot)

From template-engine Appendix A (docs/porting/analysis/template-engine.md), extracted by the Engine port.
These are NOT the same scripts as the similarly named ones in `tools/dev/analysis/` (different reader API).

- `upkg.py` — UE1 package (`.u`) reader; `layout.py` — script property layout (offsets, `PropertiesSize`).
- `hdr.py` — parses a generated `*Classes.h`; `model.py` — MSVC /Zp4 vs GCC/wasm layout model.
- `compare.py` — header vs package, member by member, under both models.
- `emit_asserts.py` — regenerates `Source/Engine/Src/EngineLayoutAsserts.cpp`:
  `python3 tools/dev/layout/emit_asserts.py Source/Engine/Inc/EngineClasses.h Assets/System/Core.u Assets/System/Engine.u > Source/Engine/Src/EngineLayoutAsserts.cpp`
  (run it after any EngineClasses.h change; the asserts must hold natively AND with `-fpack-struct=4`).
- `forscope.py`, `undc.py` — the for-scope scanner and the Dreamcast-block remover used during the analysis.

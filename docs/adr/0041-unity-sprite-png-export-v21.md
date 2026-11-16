# ADR 0041 — bounded Sprite PNG export v21

- Date: 2026-11-03
- Status: Accepted
- TaskSpec: `pc2ps3-phase3-sprite-png-export-v21` v1

## Context

The v21 Sprite region reader, Texture2D decoders, RGBA cropper, PNG codec,
and offline atlas builder existed as separate components. The pipeline needed
a bounded local bridge to export one selected Sprite without exposing Unity
identifiers or writing game assets into the repository.

## Decision

Add `tools/unity_sprite_export.py` with explicit source root, SerializedFile,
Sprite ordinal, pixel row origin, and a new output directory. Resolve only a
local Texture2D reference. External streams require an explicit root under
the source root. Emit one ordinally named PNG and a schema 1 manifest with
relative paths, stage both files, then publish by renaming the directory.
Keep stdout limited to export count and dimensions; keep errors generic.

## Consequences

- `tools/build_atlas.py` can consume the generated manifest directly.
- The caller must choose `top-left` or `bottom-left`; this does not claim that
  the real Unity texture row orientation has been visually confirmed.
- The destination must not exist and must be outside the project and source
  roots. Existing files are never overwritten.
- A real Unity export remains an optional integration smoke test; synthetic
fixtures cover the exporter contract and are the recorded QA evidence. The
available local Sprite metadata references an external stream file that is
not present in the project, so the optional real-data smoke test is pending.

## Validation

Focused exporter tests: 5 passed. PC2PS3 regression: 269 passed, 7 skipped,
207 subtests. `py_compile` passed. Argus, Tifa, and Vincent approved the local
review/QA. The real Unity payload smoke test is pending because its referenced
external stream file is not present locally.

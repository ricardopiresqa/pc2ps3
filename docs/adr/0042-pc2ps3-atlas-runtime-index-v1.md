# ADR 0042 — PC2PS3 atlas runtime index v1

- Date: 2026-11-03
- Status: Accepted
- TaskSpec: `pc2ps3-phase3-atlas-runtime-index-v1` v1

## Context

The offline atlas builder emits a PNG and an audit JSON sidecar. The C++ runtime
does not have a JSON parser dependency, but it needs atlas dimensions, the image
filename, and sprite rectangles to render packaged PNG atlases.

## Decision

Emit an additional `<name>.p2sa` sidecar with a bounded binary v1 format:

- 8-byte magic `PC2SATL\0`;
- little-endian `u16 version=1`, `u32 width`, `u32 height`, `u32 sprite_count`,
  and `u16 image_filename_length`;
- UTF-8 relative image filename (`<name>.png`);
- for each sprite in validated placement order: `u16 id_length`, `u32 x`,
  `u32 y`, `u32 width`, `u32 height`, and the UTF-8 ID bytes.

The index is derived from the same validated layout as the PNG and JSON. Its
length, sprite count, strings, dimensions, and rectangles stay within the
builder's existing limits. It contains no source paths, hashes, pixels, or game
identifiers. If publication fails after one sidecar is renamed, the builder
restores the previous files or removes the new ones before returning an error.

## Consequences

- C++ can consume atlas metadata without a JSON library; the following card
  implements the bounded reader and TextureManager integration.
- PNG and JSON audit contracts remain unchanged.
- Any incompatible index layout requires a new version.

## Validation

Python fixtures verify the magic, version, little-endian fields, deterministic
record order, bounds, and absence of source paths or pixels. A publication
failure injected at the final rename verifies rollback of PNG, JSON, and index.
The C++ reader is validated in the next atlas runtime card.

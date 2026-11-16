# ADR 0012: host filesystem adapter

## Status

Accepted.

## Decision

Keep the core FileSystem contract unchanged and implement HostFileSystem in
the Windows platform layer. The adapter receives one existing directory as its
trusted root. Runtime paths are UTF-8, relative, forward-slash paths; absolute
paths, traversal components, backslashes, drive prefixes, and reparse points
are rejected.

Reads enforce the caller's byte limit before allocating and preserve the
output buffer on failure. Writes use a temporary file beside the destination,
flush it, then replace the destination. Removal operates on files only.
Missing parent directories are errors; the adapter does not create directories.
Files with multiple hard links are rejected so a path inside the root cannot
refer to shared file content outside it.

## Consequences

- TextureManager and SaveManager can use real host files without depending
  on Windows APIs in the core.
- The adapter is built only on Windows; the PS3 platform still needs a
  separately verified backend.
- Atomic replacement is limited to same-volume Windows rename semantics.
- Path checks reject existing reparse points and verify resolved handles stay
  under the root. They do not claim safety against a hostile process racing to
  replace directories while an operation is in progress.
- Tests use a temporary directory and do not touch game assets or user saves.

## Rollback

Remove the adapter source and CMake targets. Existing managers continue to use
the FileSystem interface and in-memory test doubles.

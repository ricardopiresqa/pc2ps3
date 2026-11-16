# ADR 0013: PC2PAK asset bundle

## Status

Accepted.

## Format

PC2PAK v1 is an uncompressed, read-only archive. Its 24-byte little-endian
header contains an 8-byte magic, version, flags, entry count, and index size.
Each index record contains a UTF-8 logical path, data offset, byte size, and
CRC-32/IEEE. Records are ordered by path; file bytes are concatenated after the
index with no gaps.

## Decision

The Python packer consumes a version 2 asset manifest, rechecks every source
file's size and SHA-256, computes the bundle CRC, and writes a temporary package
outside the source root before atomic replacement. It never extracts,
decompresses, executes, or modifies source assets.

BundleFileSystem validates the complete header and index on construction. It
rejects malformed paths, duplicates, gaps, overlaps, overflow, and entries
outside the file. read_all loads only the requested entry, enforces max_bytes,
checks CRC-32, and replaces the caller's output only on success. Writes and
removals are unsupported.

## Consequences

- The runtime reads assets directly from a package without a JSON parser or
  extraction step.
- Payloads are not compressed; PS3 I/O and memory behavior still need testing
  with the verified platform toolchain.
- CRC-32 detects accidental corruption, not malicious tampering or authenticity.
- The packer validates SHA-256 against the manifest before packaging; the
  runtime validates CRC when an entry is read.

## Rollback

Remove asset_pack.py, BundleFileSystem, the CMake targets, and this format ADR.
The existing asset manifest and loose-file backends remain unchanged.

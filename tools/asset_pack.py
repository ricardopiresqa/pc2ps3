#!/usr/bin/env python3
"""Build a deterministic, uncompressed PC2PAK from an asset manifest."""

from __future__ import annotations

import argparse
import binascii
import hashlib
import json
import os
import re
import stat
import struct
import tempfile
from pathlib import Path
from typing import BinaryIO, Dict, List, Tuple

import asset_manifest

MAGIC = b"PC2PAK1\0"
VERSION = 1
HEADER = struct.Struct("<8sHHIQ")
ENTRY = struct.Struct("<HHQQI")
MAX_ENTRIES = 100_000
MAX_PATH_BYTES = 512
MAX_INDEX_BYTES = 64 * 1024 * 1024
MAX_MANIFEST_BYTES = 64 * 1024 * 1024
HASH_CHUNK_BYTES = 1024 * 1024
SHA256_PATTERN = re.compile(r"^[0-9a-f]{64}$")


class PackError(ValueError):
    """Raised when a manifest or source set cannot be safely packaged."""


def _unique_json_object(pairs):
    """Reject duplicate keys instead of accepting the last value silently."""
    result = {}
    for key, value in pairs:
        if key in result:
            raise PackError(f"manifest contains duplicate JSON key: {key}")
        result[key] = value
    return result


def _stable_json(path: Path) -> dict:
    path = path.expanduser().absolute()
    asset_manifest._reject_linked_dependency_map_parents(path)
    try:
        before = path.lstat()
    except OSError as error:
        raise PackError(f"cannot inspect manifest {path}: {error}") from error
    if asset_manifest._is_reparse_or_link(path, before) or not stat.S_ISREG(
        before.st_mode
    ):
        raise PackError("manifest must be a regular file, not a link")
    if before.st_size > MAX_MANIFEST_BYTES:
        raise PackError("manifest exceeds 64 MiB")

    flags = os.O_RDONLY | getattr(os, "O_BINARY", 0)
    flags |= getattr(os, "O_NOFOLLOW", 0)
    try:
        descriptor = os.open(str(path), flags)
    except OSError as error:
        raise PackError(f"cannot open manifest {path}: {error}") from error
    try:
        opened = os.fstat(descriptor)
        if (
            not stat.S_ISREG(opened.st_mode)
            or asset_manifest._is_reparse_or_link(path, opened)
            or not asset_manifest._same_snapshot(before, opened)
        ):
            raise PackError("manifest changed or is not a regular file")
        with os.fdopen(descriptor, "rb", closefd=False) as stream:
            raw = stream.read(MAX_MANIFEST_BYTES + 1)
        after = os.fstat(descriptor)
        if len(raw) > MAX_MANIFEST_BYTES:
            raise PackError("manifest exceeds 64 MiB")
        if not asset_manifest._same_snapshot(opened, after):
            raise PackError("manifest changed while reading")
    except OSError as error:
        raise PackError(f"cannot read manifest {path}: {error}") from error
    finally:
        os.close(descriptor)

    try:
        document = json.loads(
            raw.decode("utf-8"), object_pairs_hook=_unique_json_object
        )
    except (UnicodeDecodeError, json.JSONDecodeError, RecursionError) as error:
        raise PackError(f"manifest is not valid UTF-8 JSON: {error}") from error
    if not isinstance(document, dict):
        raise PackError("manifest root must be an object")
    return document


def _canonical_path(value: object) -> str:
    try:
        path = asset_manifest._asset_path(value)
        encoded = path.encode("utf-8", errors="strict")
    except (asset_manifest.ManifestError, UnicodeError) as error:
        raise PackError(f"invalid asset path: {error}") from error
    if len(encoded) > MAX_PATH_BYTES:
        raise PackError("asset path exceeds 512 UTF-8 bytes")
    return path


def _manifest_files(document: dict, root: Path) -> List[dict]:
    required = {
        "schema_version",
        "root",
        "file_count",
        "skipped_count",
        "files",
        "skipped",
    }
    if set(document) != required:
        raise PackError("manifest has an unsupported shape")
    if document["schema_version"] != asset_manifest.SCHEMA_VERSION:
        raise PackError("unsupported manifest schema version")
    if document["root"] != str(root):
        raise PackError("manifest root does not match the supplied root")

    files = document["files"]
    if not isinstance(files, list) or len(files) > MAX_ENTRIES:
        raise PackError("manifest file list is invalid or exceeds the entry limit")
    if (
        isinstance(document["file_count"], bool)
        or document["file_count"] != len(files)
        or not isinstance(document["skipped"], list)
        or isinstance(document["skipped_count"], bool)
        or document["skipped_count"] != len(document["skipped"])
    ):
        raise PackError("manifest counts do not match their lists")

    records = []
    seen = set()
    for item in files:
        if not isinstance(item, dict):
            raise PackError("manifest file entry must be an object")
        if set(item) != {
            "path",
            "extension",
            "format_category",
            "size_bytes",
            "sha256",
            "depends_on",
        }:
            raise PackError("manifest file entry has an unsupported shape")
        path = _canonical_path(item["path"])
        if path in seen:
            raise PackError(f"duplicate asset path: {path}")
        seen.add(path)
        size = item["size_bytes"]
        if isinstance(size, bool) or not isinstance(size, int) or size < 0:
            raise PackError(f"invalid size for asset: {path}")
        digest = item["sha256"]
        if not isinstance(digest, str) or not SHA256_PATTERN.fullmatch(digest):
            raise PackError(f"invalid SHA-256 for asset: {path}")
        dependencies = item["depends_on"]
        if not isinstance(dependencies, list):
            raise PackError(f"invalid dependency list for asset: {path}")
        for dependency in dependencies:
            _canonical_path(dependency)
        records.append({"path": path, "size": size, "sha256": digest})

    records.sort(key=lambda record: record["path"].encode("utf-8"))
    index_size = sum(ENTRY.size + len(item["path"].encode("utf-8")) for item in records)
    if index_size > MAX_INDEX_BYTES:
        raise PackError("bundle index exceeds 64 MiB")
    return records


def _asset_path(root: Path, logical_path: str) -> Path:
    parts = logical_path.split("/")
    current = root
    for index, part in enumerate(parts):
        current = current / part
        try:
            metadata = current.lstat()
        except OSError as error:
            raise PackError(f"cannot inspect asset {logical_path}: {error}") from error
        if asset_manifest._is_reparse_or_link(current, metadata):
            raise PackError(f"asset path contains a link or reparse point: {logical_path}")
        if index + 1 < len(parts) and not stat.S_ISDIR(metadata.st_mode):
            raise PackError(f"asset parent is not a directory: {logical_path}")
        if index + 1 == len(parts) and not stat.S_ISREG(metadata.st_mode):
            raise PackError(f"asset is not a regular file: {logical_path}")
    return current


def _copy_verified_asset(
    root: Path,
    item: dict,
    payload: BinaryIO,
    payload_offset: int,
) -> dict:
    path = _asset_path(root, item["path"])
    try:
        before = path.lstat()
        flags = os.O_RDONLY | getattr(os, "O_BINARY", 0)
        flags |= getattr(os, "O_NOFOLLOW", 0)
        descriptor = os.open(str(path), flags)
    except OSError as error:
        raise PackError(f"cannot open asset {item['path']}: {error}") from error

    digest = hashlib.sha256()
    checksum = 0
    copied = 0
    try:
        opened = os.fstat(descriptor)
        if (
            not stat.S_ISREG(opened.st_mode)
            or asset_manifest._is_reparse_or_link(path, opened)
            or not asset_manifest._same_snapshot(before, opened)
        ):
            raise PackError(f"asset changed or is not regular: {item['path']}")
        with os.fdopen(descriptor, "rb", closefd=False) as stream:
            while True:
                chunk = stream.read(HASH_CHUNK_BYTES)
                if not chunk:
                    break
                copied += len(chunk)
                if copied > item["size"]:
                    raise PackError(f"asset grew after manifest creation: {item['path']}")
                digest.update(chunk)
                checksum = binascii.crc32(chunk, checksum)
                payload.write(chunk)
        after = os.fstat(descriptor)
        path_after = path.lstat()
        if (
            not asset_manifest._same_snapshot(opened, after)
            or not asset_manifest._same_snapshot(opened, path_after)
            or copied != item["size"]
            or digest.hexdigest() != item["sha256"]
        ):
            raise PackError(f"asset changed since manifest creation: {item['path']}")
    except OSError as error:
        raise PackError(f"cannot read asset {item['path']}: {error}") from error
    finally:
        os.close(descriptor)

    return {
        "path": item["path"],
        "path_bytes": item["path"].encode("utf-8"),
        "offset": payload_offset,
        "size": copied,
        "crc32": checksum & 0xFFFFFFFF,
    }


def _write_bytes(stream: BinaryIO, digest, data: bytes) -> None:
    stream.write(data)
    digest.update(data)


def build_pack(root_value: Path, manifest_value: Path, output_value: Path) -> dict:
    """Build a deterministic package and atomically replace the output."""
    root = asset_manifest._root_directory(Path(root_value))
    manifest_path = Path(manifest_value).expanduser().absolute()
    resolved_manifest = manifest_path.resolve(strict=True)
    try:
        resolved_manifest.relative_to(root)
    except ValueError:
        pass
    else:
        raise PackError("manifest must be outside the asset root")

    try:
        output = asset_manifest._output_path(Path(output_value), root)
    except asset_manifest.ManifestError as error:
        raise PackError(str(error)) from error
    if output.resolve(strict=False) == resolved_manifest:
        raise PackError("package output cannot replace the source manifest")
    document = _stable_json(manifest_path)
    records = _manifest_files(document, root)

    output.parent.mkdir(parents=True, exist_ok=True)
    payload_path = None
    package_path = None
    try:
        payload_descriptor, payload_name = tempfile.mkstemp(
            prefix=f".{output.name}.payload.", suffix=".tmp", dir=str(output.parent)
        )
        payload_path = Path(payload_name)
        packed_entries = []
        offset = 0
        with os.fdopen(payload_descriptor, "wb") as payload:
            for item in records:
                packed = _copy_verified_asset(root, item, payload, offset)
                packed_entries.append(packed)
                offset += packed["size"]
            payload.flush()
            os.fsync(payload.fileno())

        index_size = sum(
            ENTRY.size + len(item["path_bytes"]) for item in packed_entries
        )
        if index_size > MAX_INDEX_BYTES:
            raise PackError("bundle index exceeds 64 MiB")
        data_offset = HEADER.size + index_size

        package_descriptor, package_name = tempfile.mkstemp(
            prefix=f".{output.name}.", suffix=".tmp", dir=str(output.parent)
        )
        package_path = Path(package_name)
        package_digest = hashlib.sha256()
        with os.fdopen(package_descriptor, "wb") as package:
            _write_bytes(
                package,
                package_digest,
                HEADER.pack(MAGIC, VERSION, 0, len(packed_entries), index_size),
            )
            current_offset = data_offset
            for item in packed_entries:
                _write_bytes(
                    package,
                    package_digest,
                    ENTRY.pack(
                        len(item["path_bytes"]),
                        0,
                        current_offset,
                        item["size"],
                        item["crc32"],
                    ),
                )
                _write_bytes(package, package_digest, item["path_bytes"])
                current_offset += item["size"]

            with payload_path.open("rb") as payload:
                while True:
                    chunk = payload.read(HASH_CHUNK_BYTES)
                    if not chunk:
                        break
                    _write_bytes(package, package_digest, chunk)
            package.flush()
            os.fsync(package.fileno())

        try:
            asset_manifest._output_path(output, root)
        except asset_manifest.ManifestError as error:
            raise PackError(str(error)) from error
        os.replace(package_path, output)
        package_path = None
        return {
            "entry_count": len(packed_entries),
            "size_bytes": output.stat().st_size,
            "sha256": package_digest.hexdigest(),
        }
    except (OSError, OverflowError, struct.error) as error:
        if isinstance(error, PackError):
            raise
        raise PackError(f"cannot write package {output}: {error}") from error
    finally:
        for temporary in (package_path, payload_path):
            if temporary is not None:
                try:
                    temporary.unlink()
                except OSError:
                    pass


def main(argv=None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("root", type=Path, help="authorized asset directory")
    parser.add_argument("manifest", type=Path, help="version 2 asset manifest")
    parser.add_argument("-o", "--output", required=True, type=Path)
    args = parser.parse_args(argv)
    try:
        result = build_pack(args.root, args.manifest, args.output)
    except (PackError, asset_manifest.ManifestError, OSError) as error:
        parser.error(str(error))
    print(
        f"wrote {result['entry_count']} entries ({result['size_bytes']} bytes) "
        f"to {args.output} sha256={result['sha256']}"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

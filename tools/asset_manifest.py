#!/usr/bin/env python3
"""Inventory authorized assets without copying or parsing asset contents."""

import argparse
import hashlib
import heapq
import json
import os
import stat
import tempfile
from pathlib import Path
from typing import Iterable, Optional, Tuple


SCHEMA_VERSION = 2
DEPENDENCY_SCHEMA_VERSION = 1
MAX_DEPENDENCY_MAP_BYTES = 1024 * 1024
HASH_CHUNK_BYTES = 1024 * 1024
FILE_ATTRIBUTE_REPARSE_POINT = 0x400

FORMAT_EXTENSIONS = {
    "image": {".bmp", ".dds", ".gif", ".jpeg", ".jpg", ".png", ".tga", ".webp"},
    "audio": {".aiff", ".flac", ".mp3", ".ogg", ".wav", ".wem"},
    "video": {".avi", ".m4v", ".mov", ".mp4", ".webm"},
    "font": {".fnt", ".otf", ".ttf"},
    "archive": {".7z", ".bank", ".pak", ".rar", ".zip"},
    "executable": {".dll", ".elf", ".exe", ".rpx", ".self", ".sprx"},
}


class ManifestError(Exception):
    """Raised when a complete, trustworthy manifest cannot be produced."""


def _is_reparse_or_link(path: Path, metadata: os.stat_result) -> bool:
    attributes = getattr(metadata, "st_file_attributes", 0)
    return stat.S_ISLNK(metadata.st_mode) or bool(
        attributes & FILE_ATTRIBUTE_REPARSE_POINT
    )


def _reject_linked_dependency_map_parents(path: Path) -> None:
    candidate = Path(path).expanduser()
    absolute = candidate if candidate.is_absolute() else Path.cwd() / candidate
    current = Path(absolute.anchor)
    for part in absolute.parts[1:-1]:
        if part == "..":
            current = current.parent
            continue
        current = current / part
        try:
            metadata = current.lstat()
        except OSError as error:
            raise ManifestError(
                f"cannot inspect dependency map parent {current}: {error}"
            ) from error
        if _is_reparse_or_link(current, metadata):
            raise ManifestError(
                "dependency map path contains a symlink or reparse point"
            )
        if not stat.S_ISDIR(metadata.st_mode):
            raise ManifestError("dependency map parent must be a directory")


def _same_snapshot(before: os.stat_result, after: os.stat_result) -> bool:
    identity_matches = (
        before.st_ino == 0
        or after.st_ino == 0
        or (before.st_dev == after.st_dev and before.st_ino == after.st_ino)
    )
    return (
        identity_matches
        and before.st_size == after.st_size
        and getattr(before, "st_mtime_ns", int(before.st_mtime * 1e9))
        == getattr(after, "st_mtime_ns", int(after.st_mtime * 1e9))
    )


def _category(extension: str) -> str:
    for category, extensions in FORMAT_EXTENSIONS.items():
        if extension in extensions:
            return category
    return "unknown"


def _hash_regular_file(path: Path, expected: os.stat_result) -> Tuple[int, str]:
    flags = os.O_RDONLY | getattr(os, "O_BINARY", 0)
    flags |= getattr(os, "O_NOFOLLOW", 0)
    try:
        descriptor = os.open(path, flags)
    except OSError as error:
        raise ManifestError(f"cannot open {path}: {error}") from error

    try:
        opened = os.fstat(descriptor)
        if not stat.S_ISREG(opened.st_mode) or not _same_snapshot(expected, opened):
            raise ManifestError(f"file changed or is not regular: {path}")

        checksum = hashlib.sha256()
        with os.fdopen(descriptor, "rb", closefd=False) as stream:
            while True:
                chunk = stream.read(HASH_CHUNK_BYTES)
                if not chunk:
                    break
                checksum.update(chunk)
        after = os.fstat(descriptor)
        if not _same_snapshot(opened, after):
            raise ManifestError(f"file changed while hashing: {path}")
        return after.st_size, checksum.hexdigest()
    except OSError as error:
        raise ManifestError(f"cannot read {path}: {error}") from error
    finally:
        os.close(descriptor)


def _root_directory(value: Path) -> Path:
    candidate = value.expanduser().absolute()
    try:
        metadata = candidate.lstat()
    except OSError as error:
        raise ManifestError(f"cannot inspect root {candidate}: {error}") from error
    if _is_reparse_or_link(candidate, metadata):
        raise ManifestError(f"root must not be a symlink or reparse point: {candidate}")
    if not stat.S_ISDIR(metadata.st_mode):
        raise ManifestError(f"root is not a directory: {candidate}")
    try:
        return candidate.resolve(strict=True)
    except OSError as error:
        raise ManifestError(f"cannot resolve root {candidate}: {error}") from error


def _asset_path(value: object) -> str:
    if not isinstance(value, str) or not value or "\x00" in value:
        raise ManifestError("asset paths must be non-empty strings")
    if "\\" in value or ":" in value:
        raise ManifestError("asset paths must use relative POSIX separators")
    parts = value.split("/")
    if value.startswith("/") or any(part in ("", ".", "..") for part in parts):
        raise ManifestError("asset paths must be canonical relative POSIX paths")
    return value


def _normalize_exclusions(values: Optional[Iterable[str]]) -> Tuple[str, ...]:
    if values is None:
        return ()
    if isinstance(values, str):
        raise ManifestError("exclusions must be a list of canonical relative paths")
    normalized = {}
    for value in values:
        path = _asset_path(value)
        normalized[path.casefold()] = path
    return tuple(sorted(normalized.values(), key=lambda item: (item.casefold(), item)))


def _is_excluded(relative: str, exclusions: Tuple[str, ...]) -> bool:
    candidate = relative.casefold()
    return any(
        candidate == exclusion.casefold()
        or candidate.startswith(exclusion.casefold() + "/")
        for exclusion in exclusions
    )


def _read_dependency_document(path: Path) -> dict:
    try:
        before = path.lstat()
    except OSError as error:
        raise ManifestError(f"cannot inspect dependency map {path}: {error}") from error
    if _is_reparse_or_link(path, before) or not stat.S_ISREG(before.st_mode):
        raise ManifestError("dependency map must be a regular file, not a link")
    if before.st_size > MAX_DEPENDENCY_MAP_BYTES:
        raise ManifestError("dependency map exceeds 1 MiB")

    flags = os.O_RDONLY | getattr(os, "O_BINARY", 0) | getattr(os, "O_NOFOLLOW", 0)
    try:
        descriptor = os.open(str(path), flags)
    except OSError as error:
        raise ManifestError(f"cannot open dependency map {path}: {error}") from error
    try:
        opened = os.fstat(descriptor)
        if (
            _is_reparse_or_link(path, opened)
            or not stat.S_ISREG(opened.st_mode)
            or not _same_snapshot(before, opened)
        ):
            raise ManifestError("dependency map changed or is not a regular file")
        if opened.st_size > MAX_DEPENDENCY_MAP_BYTES:
            raise ManifestError("dependency map exceeds 1 MiB")
        with os.fdopen(descriptor, "rb", closefd=False) as stream:
            raw = stream.read(MAX_DEPENDENCY_MAP_BYTES + 1)
        after = os.fstat(descriptor)
        if not _same_snapshot(opened, after):
            raise ManifestError("dependency map changed while reading")
        if len(raw) > MAX_DEPENDENCY_MAP_BYTES:
            raise ManifestError("dependency map exceeds 1 MiB")
    except OSError as error:
        raise ManifestError(f"cannot read dependency map {path}: {error}") from error
    finally:
        os.close(descriptor)

    def unique_object(pairs):
        result = {}
        for key, value in pairs:
            if key in result:
                raise ManifestError(f"dependency map contains duplicate JSON key: {key}")
            result[key] = value
        return result

    try:
        document = json.loads(raw.decode("utf-8"), object_pairs_hook=unique_object)
    except (UnicodeDecodeError, json.JSONDecodeError, RecursionError) as error:
        raise ManifestError(f"dependency map is not valid UTF-8 JSON: {error}") from error
    if not isinstance(document, dict) or set(document) != {"schema_version", "dependencies"}:
        raise ManifestError("dependency map must contain only schema_version and dependencies")
    version = document["schema_version"]
    if isinstance(version, bool) or not isinstance(version, int) or version != DEPENDENCY_SCHEMA_VERSION:
        raise ManifestError("unsupported dependency map schema_version")
    if not isinstance(document["dependencies"], list):
        raise ManifestError("dependency map dependencies must be a list")
    return document


def _load_dependencies(value: Path, root: Path, known_paths: set) -> dict:
    path = Path(value).expanduser()
    _reject_linked_dependency_map_parents(path)
    path = path.absolute()
    try:
        metadata = path.lstat()
    except OSError as error:
        raise ManifestError(f"cannot inspect dependency map {path}: {error}") from error
    if _is_reparse_or_link(path, metadata) or not stat.S_ISREG(metadata.st_mode):
        raise ManifestError("dependency map must be a regular file, not a link")
    try:
        resolved = path.resolve(strict=True)
    except OSError as error:
        raise ManifestError(f"cannot resolve dependency map {path}: {error}") from error
    try:
        resolved.relative_to(root)
    except ValueError:
        pass
    else:
        raise ManifestError("dependency map must be outside the scanned root")
    document = _read_dependency_document(path)

    dependencies = {}
    for entry in document["dependencies"]:
        if not isinstance(entry, dict) or set(entry) != {"asset", "depends_on"}:
            raise ManifestError("each dependency entry must contain only asset and depends_on")
        asset = _asset_path(entry["asset"])
        if asset not in known_paths:
            raise ManifestError(f"dependency asset is not in the inventory: {asset}")
        if asset in dependencies:
            raise ManifestError(f"duplicate dependency declaration for asset: {asset}")
        targets = entry["depends_on"]
        if not isinstance(targets, list):
            raise ManifestError(f"depends_on must be a list for asset: {asset}")
        normalized = set()
        for target_value in targets:
            target = _asset_path(target_value)
            if target not in known_paths:
                raise ManifestError(f"dependency target is not in the inventory: {target}")
            if target == asset:
                raise ManifestError(f"asset cannot depend on itself: {asset}")
            if target in normalized:
                raise ManifestError(f"duplicate dependency target for asset: {asset}")
            normalized.add(target)
        dependencies[asset] = sorted(normalized)

    remaining = {path: len(dependencies.get(path, [])) for path in known_paths}
    dependents = {path: [] for path in known_paths}
    for asset, targets in dependencies.items():
        for target in targets:
            dependents[target].append(asset)
    for values in dependents.values():
        values.sort()
    ready = [path for path, count in remaining.items() if count == 0]
    heapq.heapify(ready)
    visited = 0
    while ready:
        target = heapq.heappop(ready)
        visited += 1
        for asset in dependents[target]:
            remaining[asset] -= 1
            if remaining[asset] == 0:
                heapq.heappush(ready, asset)
    if visited != len(known_paths):
        raise ManifestError("dependency map contains a cycle")
    return dependencies


def build_manifest(
    root: Path,
    dependency_map: Optional[Path] = None,
    exclusions: Optional[Iterable[str]] = None,
) -> dict:
    """Return deterministic file metadata, failing on incomplete scans."""
    root = _root_directory(Path(root))
    exclusions = _normalize_exclusions(exclusions)
    files = []
    skipped = []
    pending = [root]

    while pending:
        directory = pending.pop()
        try:
            directory_metadata = directory.lstat()
        except OSError as error:
            raise ManifestError(f"cannot inspect directory {directory}: {error}") from error
        if _is_reparse_or_link(directory, directory_metadata) or not stat.S_ISDIR(
            directory_metadata.st_mode
        ):
            raise ManifestError(f"directory changed or became a link: {directory}")
        try:
            with os.scandir(directory) as iterator:
                entries = sorted(iterator, key=lambda entry: entry.name)
        except OSError as error:
            raise ManifestError(f"cannot scan {directory}: {error}") from error

        for entry in entries:
            path = Path(entry.path)
            relative = path.relative_to(root).as_posix()
            if _is_excluded(relative, exclusions):
                continue
            try:
                metadata = path.lstat()
            except OSError as error:
                raise ManifestError(f"cannot inspect {path}: {error}") from error

            if _is_reparse_or_link(path, metadata):
                skipped.append({"path": relative, "reason": "link_or_reparse_point"})
                continue
            if stat.S_ISDIR(metadata.st_mode):
                pending.append(path)
                continue
            if not stat.S_ISREG(metadata.st_mode):
                skipped.append({"path": relative, "reason": "special_file"})
                continue

            size, sha256 = _hash_regular_file(path, metadata)
            extension = path.suffix.lower()
            files.append(
                {
                    "path": relative,
                    "extension": extension,
                    "format_category": _category(extension),
                    "size_bytes": size,
                    "sha256": sha256,
                }
            )

    files.sort(key=lambda item: item["path"])
    skipped.sort(key=lambda item: (item["path"], item["reason"]))
    known_paths = {item["path"] for item in files}
    dependencies = (
        _load_dependencies(dependency_map, root, known_paths)
        if dependency_map is not None
        else {}
    )
    for item in files:
        item["depends_on"] = dependencies.get(item["path"], [])
    return {
        "schema_version": SCHEMA_VERSION,
        "root": str(root),
        "file_count": len(files),
        "skipped_count": len(skipped),
        "files": files,
        "skipped": skipped,
    }


def _output_path(value: Path, root: Path) -> Path:
    output = value.expanduser().absolute()
    resolved = output.resolve(strict=False)
    try:
        resolved.relative_to(root)
    except ValueError:
        pass
    else:
        raise ManifestError("manifest output must be outside the scanned root")

    if output.exists() or output.is_symlink():
        try:
            metadata = output.lstat()
        except OSError as error:
            raise ManifestError(f"cannot inspect output {output}: {error}") from error
        if _is_reparse_or_link(output, metadata) or not stat.S_ISREG(metadata.st_mode):
            raise ManifestError(f"output must be a regular file, not a link: {output}")
    return output


def write_manifest(manifest: dict, output: Path) -> None:
    """Write JSON to a sibling temporary file and atomically replace output."""
    output.parent.mkdir(parents=True, exist_ok=True)
    temporary = None
    try:
        descriptor, name = tempfile.mkstemp(
            prefix=f".{output.name}.", suffix=".tmp", dir=str(output.parent)
        )
        temporary = Path(name)
        with os.fdopen(descriptor, "w", encoding="utf-8", newline="\n") as stream:
            json.dump(manifest, stream, ensure_ascii=True, indent=2, sort_keys=True)
            stream.write("\n")
            stream.flush()
            os.fsync(stream.fileno())
        os.replace(temporary, output)
        temporary = None
    except OSError as error:
        raise ManifestError(f"cannot write manifest {output}: {error}") from error
    finally:
        if temporary is not None:
            try:
                temporary.unlink()
            except OSError:
                pass


def main(argv=None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("root", type=Path, help="authorized asset directory")
    parser.add_argument(
        "--dependency-map",
        type=Path,
        help="explicit JSON dependency map stored outside the scanned root",
    )
    parser.add_argument(
        "--exclude",
        action="append",
        default=[],
        help="canonical relative path to exclude; directories exclude their subtree",
    )
    parser.add_argument(
        "-o", "--output", type=Path, default=Path("asset-manifest.json")
    )
    args = parser.parse_args(argv)

    try:
        root = _root_directory(args.root)
        manifest = build_manifest(root, args.dependency_map, args.exclude)
        output = _output_path(args.output, root)
        write_manifest(manifest, output)
    except (ManifestError, OSError) as error:
        parser.error(str(error))
    print(f"wrote {manifest['file_count']} entries to {output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

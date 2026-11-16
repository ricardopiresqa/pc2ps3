"""Build a local RGBA atlas and stable metadata from a bounded PNG manifest."""

import argparse
import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import re
import stat
import struct
import sys
import tempfile
import zlib

try:
    import asset_manifest
    import asset_png
    import atlas_layout
except ImportError:
    from tools import asset_manifest, asset_png, atlas_layout


MANIFEST_VERSION = 1
ATLAS_TOOL_VERSION = "1"
MAX_MANIFEST_BYTES = 1024 * 1024
MAX_SPRITES = 1024
MAX_SOURCE_PIXELS = asset_png.MAX_PIXELS
MAX_OUTPUT_PIXELS = asset_png.MAX_PIXELS
_OUTPUT_NAME = re.compile(r"^[A-Za-z0-9_-]{1,64}$")
ATLAS_INDEX_MAGIC = b"PC2SATL\0"
ATLAS_INDEX_VERSION = 1
MAX_ATLAS_INDEX_BYTES = 24 + 68 + MAX_SPRITES * (18 + atlas_layout.MAX_ID_BYTES)


class AtlasBuildError(ValueError):
    """Raised when the manifest, paths or assets violate the builder contract."""


def _is_link_or_reparse(path, metadata):
    return asset_manifest._is_reparse_or_link(path, metadata)


def _reject_linked_ancestors(path):
    chain = []
    current = Path(path).absolute()
    while True:
        chain.append(current)
        parent = current.parent
        if parent == current:
            break
        current = parent
    for candidate in reversed(chain):
        try:
            metadata = candidate.lstat()
        except OSError as error:
            raise AtlasBuildError("path contains an unavailable parent") from error
        if _is_link_or_reparse(candidate, metadata):
            raise AtlasBuildError("path contains a symlink or reparse point")


def _resolve_directory(value, label):
    path = Path(value).expanduser().absolute()
    _reject_linked_ancestors(path)
    try:
        metadata = path.lstat()
    except OSError as error:
        raise AtlasBuildError("{} directory is unavailable".format(label)) from error
    if _is_link_or_reparse(path, metadata) or not stat.S_ISDIR(metadata.st_mode):
        raise AtlasBuildError("{} must be a real directory".format(label))
    try:
        return path.resolve(strict=True)
    except OSError as error:
        raise AtlasBuildError("{} directory cannot be resolved".format(label)) from error


def _read_regular_file(path, max_bytes, label):
    try:
        before = path.lstat()
    except OSError as error:
        raise AtlasBuildError("{} is unavailable".format(label)) from error
    if _is_link_or_reparse(path, before) or not stat.S_ISREG(before.st_mode):
        raise AtlasBuildError("{} must be a regular file, not a link".format(label))
    if before.st_size > max_bytes:
        raise AtlasBuildError("{} exceeds byte limit".format(label))

    flags = os.O_RDONLY | getattr(os, "O_BINARY", 0) | getattr(os, "O_NOFOLLOW", 0)
    try:
        descriptor = os.open(str(path), flags)
    except OSError as error:
        raise AtlasBuildError("{} cannot be opened safely".format(label)) from error
    try:
        opened = os.fstat(descriptor)
        if (
            not stat.S_ISREG(opened.st_mode)
            or not asset_manifest._same_snapshot(before, opened)
        ):
            raise AtlasBuildError("{} changed while opening".format(label))
        with os.fdopen(descriptor, "rb", closefd=False) as stream:
            data = stream.read(max_bytes + 1)
        after = os.fstat(descriptor)
        if (
            len(data) > max_bytes
            or len(data) != opened.st_size
            or not asset_manifest._same_snapshot(opened, after)
        ):
            raise AtlasBuildError("{} changed or exceeds byte limit".format(label))
        return data
    except OSError as error:
        raise AtlasBuildError("{} could not be read".format(label)) from error
    finally:
        os.close(descriptor)


def _unique_json_object(pairs):
    result = {}
    for key, value in pairs:
        if key in result:
            raise AtlasBuildError("manifest contains a duplicate JSON key")
        result[key] = value
    return result


def _read_manifest(path):
    _reject_linked_ancestors(path)
    raw = _read_regular_file(path, MAX_MANIFEST_BYTES, "manifest")
    try:
        document = json.loads(raw.decode("utf-8"), object_pairs_hook=_unique_json_object)
    except AtlasBuildError:
        raise
    except (UnicodeDecodeError, json.JSONDecodeError, RecursionError, ValueError) as error:
        raise AtlasBuildError("manifest is not valid UTF-8 JSON") from error
    if not isinstance(document, dict) or set(document) != {"schema_version", "sprites"}:
        raise AtlasBuildError("manifest must contain only schema_version and sprites")
    if type(document["schema_version"]) is not int or document["schema_version"] != MANIFEST_VERSION:
        raise AtlasBuildError("unsupported atlas manifest version")
    sprites = document["sprites"]
    if not isinstance(sprites, list) or not sprites or len(sprites) > MAX_SPRITES:
        raise AtlasBuildError("manifest sprite count must be from 1 to {}".format(MAX_SPRITES))
    return raw, sprites


def _validate_id(value):
    if not isinstance(value, str) or not value.strip() or not value.isprintable():
        raise AtlasBuildError("sprite ID must be a non-empty printable string")
    try:
        encoded = value.encode("utf-8")
    except UnicodeEncodeError as error:
        raise AtlasBuildError("sprite ID is not valid UTF-8") from error
    if len(encoded) > atlas_layout.MAX_ID_BYTES:
        raise AtlasBuildError("sprite ID exceeds byte limit")


def _canonical_relative_path(value):
    try:
        canonical = asset_manifest._asset_path(value)
    except (asset_manifest.ManifestError, UnicodeError) as error:
        raise AtlasBuildError("sprite path is not canonical") from error
    if (
        canonical != value
        or "\\" in canonical
        or ":" in canonical
        or canonical.startswith("/")
        or any(part in ("", ".", "..") for part in canonical.split("/"))
    ):
        raise AtlasBuildError("sprite path must be a canonical relative POSIX path")
    return canonical


def _source_file(root, relative_path):
    parts = PurePosixPath(relative_path).parts
    current = root
    for index, part in enumerate(parts):
        current = current / part
        try:
            metadata = current.lstat()
        except OSError as error:
            raise AtlasBuildError("sprite path is unavailable: {}".format(relative_path)) from error
        if _is_link_or_reparse(current, metadata):
            raise AtlasBuildError("sprite path contains a symlink or reparse point")
        if index + 1 < len(parts) and not stat.S_ISDIR(metadata.st_mode):
            raise AtlasBuildError("sprite parent is not a directory")
        if index + 1 == len(parts) and not stat.S_ISREG(metadata.st_mode):
            raise AtlasBuildError("sprite must be a regular file")
    try:
        current.resolve(strict=True).relative_to(root)
    except (OSError, ValueError) as error:
        raise AtlasBuildError("sprite path escapes the source root") from error
    return current


def _validate_output_target(path):
    try:
        metadata = path.lstat()
    except FileNotFoundError:
        return
    except OSError as error:
        raise AtlasBuildError("output target is unavailable") from error
    if _is_link_or_reparse(path, metadata) or not stat.S_ISREG(metadata.st_mode):
        raise AtlasBuildError("output target must be a regular file, not a link")


def _stage_output(directory, data):
    descriptor, name = tempfile.mkstemp(prefix=".pc2ps3-atlas-", suffix=".tmp", dir=str(directory))
    temporary = Path(name)
    try:
        with os.fdopen(descriptor, "wb") as stream:
            stream.write(data)
            stream.flush()
            os.fsync(stream.fileno())
        return temporary
    except OSError:
        try:
            temporary.unlink()
        except OSError:
            pass
        raise


def _reserve_output_backup(directory):
    descriptor, name = tempfile.mkstemp(
        prefix=".pc2ps3-atlas-backup-", suffix=".tmp", dir=str(directory)
    )
    backup = Path(name)
    try:
        os.close(descriptor)
    except OSError:
        try:
            backup.unlink()
        except OSError:
            pass
        raise
    return backup


def _publish_outputs(
    output_dir,
    image_path,
    image_bytes,
    metadata_path,
    metadata_bytes,
    index_path,
    index_bytes,
):
    targets = (
        (image_path, image_bytes),
        (metadata_path, metadata_bytes),
        (index_path, index_bytes),
    )
    for target, _ in targets:
        _validate_output_target(target)

    staged = []
    backups = []
    backup_placeholders = []
    published = []
    try:
        staged_targets = []
        for target, data in targets:
            temporary = _stage_output(output_dir, data)
            staged.append(temporary)
            staged_targets.append((target, temporary))

        for target, _ in targets:
            try:
                target.lstat()
            except FileNotFoundError:
                continue
            backup = _reserve_output_backup(output_dir)
            backup_placeholders.append(backup)
            os.replace(str(target), str(backup))
            backup_placeholders.remove(backup)
            backups.append((target, backup))

        for target, temporary in staged_targets:
            os.replace(str(temporary), str(target))
            staged.remove(temporary)
            published.append(target)
    except BaseException as error:
        rollback_failed = False
        original_targets = {target for target, _ in backups}

        for target, backup in reversed(backups):
            try:
                os.replace(str(backup), str(target))
            except OSError:
                rollback_failed = True

        for target in reversed(published):
            if target in original_targets:
                continue
            try:
                target.unlink()
            except FileNotFoundError:
                pass
            except OSError:
                rollback_failed = True

        for temporary in staged + backup_placeholders:
            try:
                temporary.unlink()
            except FileNotFoundError:
                pass
            except OSError:
                rollback_failed = True

        if rollback_failed:
            raise AtlasBuildError(
                "atlas output publication failed and rollback was incomplete"
            ) from error
        raise

    for _, backup in backups:
        try:
            backup.unlink()
        except OSError:
            pass


def _encode_runtime_index(name, width, height, sprites):
    image_filename = (name + ".png").encode("ascii")
    if (
        not image_filename
        or len(image_filename) > 0xFFFF
        or not isinstance(width, int)
        or not isinstance(height, int)
        or width <= 0
        or height <= 0
        or len(sprites) < 1
        or len(sprites) > MAX_SPRITES
    ):
        raise AtlasBuildError("atlas runtime index metadata is invalid")

    output = bytearray(
        struct.pack(
            "<8sHIIIH",
            ATLAS_INDEX_MAGIC,
            ATLAS_INDEX_VERSION,
            width,
            height,
            len(sprites),
            len(image_filename),
        )
    )
    output.extend(image_filename)
    for sprite in sprites:
        identifier = sprite["id"].encode("utf-8")
        x, y = sprite["x"], sprite["y"]
        sprite_width, sprite_height = sprite["width"], sprite["height"]
        if (
            not identifier
            or len(identifier) > atlas_layout.MAX_ID_BYTES
            or x < 0
            or y < 0
            or sprite_width <= 0
            or sprite_height <= 0
            or x > width - sprite_width
            or y > height - sprite_height
        ):
            raise AtlasBuildError("atlas runtime index sprite is invalid")
        output.extend(
            struct.pack(
                "<HIIII", len(identifier), x, y, sprite_width, sprite_height
            )
        )
        output.extend(identifier)
        if len(output) > MAX_ATLAS_INDEX_BYTES:
            raise AtlasBuildError("atlas runtime index exceeds supported limit")
    return bytes(output)


def build_atlas(root, manifest_path, output_dir, name="atlas", max_width=2048, max_height=2048, padding=0):
    """Build an atlas and metadata sidecar; return their output paths."""
    if not isinstance(name, str) or not _OUTPUT_NAME.fullmatch(name):
        raise AtlasBuildError("output name must use 1-64 ASCII letters, digits, '_' or '-'")

    source_root = _resolve_directory(root, "source root")
    destination = _resolve_directory(output_dir, "output")
    try:
        destination.relative_to(source_root)
    except ValueError:
        pass
    else:
        raise AtlasBuildError("output directory must be outside the source root")

    manifest = Path(manifest_path).expanduser().absolute()
    raw_manifest, entries = _read_manifest(manifest)
    seen_ids = set()
    seen_paths = set()
    validated = []
    for entry in entries:
        if not isinstance(entry, dict) or set(entry) != {"id", "path"}:
            raise AtlasBuildError("each sprite must contain only id and path")
        identifier = entry["id"]
        _validate_id(identifier)
        relative_path = _canonical_relative_path(entry["path"])
        if identifier in seen_ids:
            raise AtlasBuildError("sprite IDs must be unique")
        path_key = relative_path.casefold()
        if path_key in seen_paths:
            raise AtlasBuildError("sprite paths must be unique")
        seen_ids.add(identifier)
        seen_paths.add(path_key)
        validated.append((identifier, relative_path))

    images = []
    total_source_pixels = 0
    for identifier, relative_path in validated:
        remaining_pixels = MAX_SOURCE_PIXELS - total_source_pixels
        if remaining_pixels <= 0:
            raise AtlasBuildError("source images exceed total pixel limit")
        source_path = _source_file(source_root, relative_path)
        encoded = _read_regular_file(source_path, asset_png.MAX_INPUT_BYTES, "sprite {}".format(identifier))
        try:
            image = asset_png.decode_png(encoded, max_pixels=remaining_pixels)
        except asset_png.PngError as error:
            raise AtlasBuildError("sprite {} is not a supported PNG: {}".format(identifier, error)) from error
        total_source_pixels += image.width * image.height
        images.append((identifier, image, hashlib.sha256(encoded).hexdigest()))

    try:
        layout = atlas_layout.pack_atlas(
            [atlas_layout.SpriteSize(identifier, image.width, image.height) for identifier, image, _ in images],
            max_width=max_width,
            max_height=max_height,
            padding=padding,
        )
    except atlas_layout.LayoutError as error:
        raise AtlasBuildError(str(error)) from error
    if layout.width <= 0 or layout.height <= 0 or layout.width * layout.height > MAX_OUTPUT_PIXELS:
        raise AtlasBuildError("atlas dimensions exceed output pixel limit")

    pixels = bytearray(layout.width * layout.height * 4)
    sprite_metadata = []
    for placement, (identifier, image, source_sha256) in zip(layout.placements, images):
        for row in range(image.height):
            source_start = row * image.width * 4
            destination_start = ((placement.y + row) * layout.width + placement.x) * 4
            row_bytes = image.width * 4
            pixels[destination_start:destination_start + row_bytes] = image.rgba[source_start:source_start + row_bytes]
        sprite_metadata.append({
            "id": identifier,
            "x": placement.x,
            "y": placement.y,
            "width": placement.width,
            "height": placement.height,
            "source_sha256": source_sha256,
        })

    try:
        image_bytes = asset_png.encode_png(asset_png.PngImage(layout.width, layout.height, bytes(pixels)))
    except asset_png.PngError as error:
        raise AtlasBuildError("atlas PNG could not be encoded") from error
    image_filename = name + ".png"
    document = {
        "schema_version": 1,
        "generator": {
            "name": "pc2ps3-atlas",
            "version": ATLAS_TOOL_VERSION,
            "python_version": "{}.{}.{}".format(*sys.version_info[:3]),
            "zlib_version": zlib.ZLIB_VERSION,
        },
        "manifest_sha256": hashlib.sha256(raw_manifest).hexdigest(),
        "image": {
            "file": image_filename,
            "width": layout.width,
            "height": layout.height,
            "sha256": hashlib.sha256(image_bytes).hexdigest(),
        },
        "sprites": sprite_metadata,
    }
    metadata_bytes = (json.dumps(document, ensure_ascii=False, sort_keys=True, separators=(",", ":")) + "\n").encode("utf-8")
    image_path = destination / image_filename
    metadata_path = destination / (name + ".json")
    index_path = destination / (name + ".p2sa")
    index_bytes = _encode_runtime_index(
        name, layout.width, layout.height, sprite_metadata
    )
    _publish_outputs(
        destination,
        image_path,
        image_bytes,
        metadata_path,
        metadata_bytes,
        index_path,
        index_bytes,
    )
    return image_path, metadata_path, layout


def main(argv=None):
    parser = argparse.ArgumentParser(description="Build a bounded RGBA sprite atlas from local PNG files.")
    parser.add_argument("root", help="source root containing the PNG files")
    parser.add_argument("manifest", help="version 1 JSON manifest with sprite IDs and relative paths")
    parser.add_argument("--output-dir", required=True, help="existing output directory outside the source root")
    parser.add_argument("--name", default="atlas", help="output basename using ASCII letters, digits, '_' or '-'")
    parser.add_argument("--max-width", type=int, default=2048, help="maximum atlas width (default: 2048)")
    parser.add_argument("--max-height", type=int, default=2048, help="maximum atlas height (default: 2048)")
    parser.add_argument("--padding", type=int, default=0, help="transparent gap between sprites and outer edge")
    args = parser.parse_args(argv)
    try:
        image_path, metadata_path, layout = build_atlas(
            args.root,
            args.manifest,
            args.output_dir,
            name=args.name,
            max_width=args.max_width,
            max_height=args.max_height,
            padding=args.padding,
        )
    except AtlasBuildError as error:
        print("atlas build: {}".format(error), file=sys.stderr)
        return 2
    except OSError as error:
        print("atlas build: I/O failure ({})".format(error.strerror or type(error).__name__), file=sys.stderr)
        return 3
    print("built {}x{} atlas with {} sprites: {} and {}".format(
        layout.width, layout.height, len(layout.placements), image_path.name, metadata_path.name
    ))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

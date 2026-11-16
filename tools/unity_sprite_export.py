"""Export one validated Unity Sprite to a generated PNG and atlas manifest."""

import argparse
import io
import json
from pathlib import Path
import shutil
import stat
import sys
import tempfile
from typing import BinaryIO, NamedTuple, Optional, Sequence

try:
    import asset_manifest
    import asset_png
    import build_atlas
    import unity_serialized_file
    import unityfs_data
    import unityfs_header
    from unity_texture_pixels import (
        crop_sprite_rgba,
        decode_loaded_texture2d_to_rgba,
        decode_texture2d_to_rgba,
    )
except ImportError:
    from tools import (
        asset_manifest,
        asset_png,
        build_atlas,
        unity_serialized_file,
        unityfs_data,
        unityfs_header,
    )
    from tools.unity_texture_pixels import (
        crop_sprite_rgba,
        decode_loaded_texture2d_to_rgba,
        decode_texture2d_to_rgba,
    )


MAX_EXPORTED_FILE_BYTES = 64 * 1024 * 1024
MAX_EXPORTED_SPRITES = 64
MAX_EXPORTED_BATCH_BYTES = 128 * 1024 * 1024
MAX_UNITYFS_BUNDLE_BYTES = 1024 * 1024 * 1024
PROJECT_ROOT = Path(__file__).resolve().parents[1]


class SpriteExportError(ValueError):
    """Raised when a Sprite cannot be exported within the bounded contract."""


class SpriteExportSummary(NamedTuple):
    width: int
    height: int


class SpriteBatchExportSummary(NamedTuple):
    exported: int
    total_png_bytes: int
    dimensions: tuple[SpriteExportSummary, ...]


def _is_within(path: Path, root: Path) -> bool:
    try:
        path.relative_to(root)
        return True
    except ValueError:
        return False


def _resolve_source_root(value: str) -> Path:
    path = Path(value).expanduser()
    if not path.is_absolute():
        raise SpriteExportError("source root must be absolute")
    try:
        return build_atlas._resolve_directory(path, "source root")
    except (OSError, build_atlas.AtlasBuildError):
        raise SpriteExportError("source root is invalid") from None


def _resolve_serialized_file(value: str, source_root: Path) -> Path:
    path = Path(value).expanduser()
    if not path.is_absolute():
        path = source_root / path
    path = path.absolute()
    try:
        build_atlas._reject_linked_ancestors(path)
        metadata = path.lstat()
    except OSError:
        raise SpriteExportError("SerializedFile is unavailable") from None
    if asset_manifest._is_reparse_or_link(path, metadata) or not stat.S_ISREG(metadata.st_mode):
        raise SpriteExportError("SerializedFile must be a regular file")
    if metadata.st_size > unity_serialized_file.MAX_SERIALIZED_FILE_BYTES:
        raise SpriteExportError("SerializedFile exceeds the supported limit")
    try:
        resolved = path.resolve(strict=True)
    except OSError:
        raise SpriteExportError("SerializedFile cannot be resolved") from None
    if not _is_within(resolved, source_root):
        raise SpriteExportError("SerializedFile is outside the source root")
    return resolved


def _resolve_bundle_file(value: str, source_root: Path) -> Path:
    path = Path(value).expanduser()
    if not path.is_absolute():
        path = source_root / path
    try:
        build_atlas._reject_linked_ancestors(path)
        metadata = path.lstat()
    except OSError:
        raise SpriteExportError("UnityFS bundle is unavailable") from None
    if (
        asset_manifest._is_reparse_or_link(path, metadata)
        or not stat.S_ISREG(metadata.st_mode)
        or metadata.st_size > MAX_UNITYFS_BUNDLE_BYTES
    ):
        raise SpriteExportError("UnityFS bundle is invalid or exceeds the input limit")
    try:
        resolved = path.resolve(strict=True)
    except OSError:
        raise SpriteExportError("UnityFS bundle is unavailable") from None
    if not _is_within(resolved, source_root):
        raise SpriteExportError("UnityFS bundle is outside the source root")
    return resolved


def _resolve_external_root(value: Optional[str], source_root: Path) -> Optional[Path]:
    if value is None:
        return None
    path = Path(value).expanduser()
    if not path.is_absolute():
        raise SpriteExportError("external data root must be absolute")
    try:
        resolved = build_atlas._resolve_directory(path, "external data root")
    except (OSError, build_atlas.AtlasBuildError):
        raise SpriteExportError("external data root is invalid") from None
    if not _is_within(resolved, source_root):
        raise SpriteExportError("external data root is outside the source root")
    return resolved


def _resolve_new_output_directory(value: str, source_root: Path) -> Path:
    path = Path(value).expanduser()
    if not path.is_absolute() or not path.name:
        raise SpriteExportError("output directory must be an absolute new path")
    try:
        parent = build_atlas._resolve_directory(path.parent, "output parent")
    except (OSError, build_atlas.AtlasBuildError):
        raise SpriteExportError("output parent is invalid") from None
    destination = parent / path.name
    try:
        destination.lstat()
    except FileNotFoundError:
        pass
    except OSError:
        raise SpriteExportError("output directory cannot be checked") from None
    else:
        raise SpriteExportError("output directory already exists")
    if _is_within(destination, source_root) or _is_within(destination, PROJECT_ROOT):
        raise SpriteExportError("output directory must be outside source and project roots")
    return destination


def _texture_for_sprite(stream, header, preamble, sprite, objects):
    (
        _,
        _,
        _,
        _,
        texture_file_id,
        texture_path_id,
    ) = unity_serialized_file._read_sprite_render_data_prefix(
        stream, header, preamble, sprite
    )
    if texture_file_id != 0 or texture_path_id == 0:
        raise SpriteExportError("Sprite requires a local Texture2D")
    texture = next(
        (
            item
            for item in objects
            if item.class_id == 28 and item.path_id == texture_path_id
        ),
        None,
    )
    if texture is None:
        raise SpriteExportError("Sprite Texture2D is unresolved")
    return texture, texture_path_id


def _parse_export_metadata(stream: BinaryIO, available_size: int) -> tuple:
    try:
        stream.seek(0)
        header = unity_serialized_file.parse_serialized_file_header(stream, available_size)
        metadata = unity_serialized_file.parse_serialized_file_metadata_v21(
            stream, available_size, allow_type_tree_disabled=True
        )
        preamble = metadata["preamble"]
        objects = metadata["objects"]
        sprites = tuple(item for item in objects if item.class_id == 213)
        return header, preamble, objects, sprites
    except SpriteExportError:
        raise
    except Exception:
        raise SpriteExportError("Sprite input is invalid or unsupported") from None


def _build_export_from_stream(
    stream: BinaryIO,
    available_size: int,
    sprite_index: int,
    source_origin: str,
    external_root: Optional[Path],
    parsed_source: Optional[tuple] = None,
):
    if isinstance(sprite_index, bool) or not isinstance(sprite_index, int) or sprite_index < 0:
        raise SpriteExportError("Sprite ordinal is invalid")
    try:
        stream.seek(0)
        if parsed_source is None:
            parsed_source = _parse_export_metadata(stream, available_size)
        header, preamble, objects, sprites = parsed_source
        if sprite_index >= len(sprites):
            raise SpriteExportError("Sprite ordinal is outside the file")
        sprite = sprites[sprite_index]
        texture, texture_path_id = _texture_for_sprite(
            stream, header, preamble, sprite, objects
        )
        texture_metadata = unity_serialized_file.read_texture2d_metadata_prefix(
            stream, header, preamble, texture
        )
        region = unity_serialized_file.read_sprite_region(
            stream,
            header,
            preamble,
            sprite,
            {texture_path_id: texture_metadata},
        )
        image = unity_serialized_file.read_texture2d_image_data(
            stream,
            header,
            preamble,
            texture,
            external_data_root=external_root,
        )
    except SpriteExportError:
        raise
    except Exception:
        raise SpriteExportError("Sprite input is invalid or unsupported") from None
    try:
        if image.storage == "inline":
            rgba = decode_texture2d_to_rgba(image)
        elif image.storage == "loaded":
            rgba = decode_loaded_texture2d_to_rgba(image)
        else:
            raise SpriteExportError("Sprite Texture2D payload is unavailable")
        cropped = crop_sprite_rgba(rgba, region, source_origin=source_origin)
        png = asset_png.encode_png(asset_png.PngImage(cropped.width, cropped.height, cropped.pixels))
    except SpriteExportError:
        raise
    except Exception:
        raise SpriteExportError("Sprite pixels are invalid or unsupported") from None
    if len(png) > MAX_EXPORTED_FILE_BYTES:
        raise SpriteExportError("PNG output exceeds the supported limit")
    return cropped.width, cropped.height, png


def _build_export(
    serialized_file: Path,
    sprite_index: int,
    source_origin: str,
    external_root: Optional[Path],
):
    try:
        with serialized_file.open("rb") as stream:
            available_size = serialized_file.stat().st_size
            return _build_export_from_stream(
                stream, available_size, sprite_index, source_origin, external_root
            )
    except SpriteExportError:
        raise
    except Exception:
        raise SpriteExportError("Sprite input is invalid or unsupported") from None


def _read_unityfs_node(bundle_path: Path, node_index: int) -> bytes:
    try:
        with bundle_path.open("rb") as stream:
            layout = unityfs_header.parse_data_block_layout(
                stream, bundle_path.stat().st_size
            )
            return unityfs_data.read_node_bytes(stream, layout, node_index)
    except Exception:
        raise SpriteExportError("UnityFS bundle or selected node is invalid") from None


def _build_export_from_node(
    node_bytes: bytes,
    sprite_index: int,
    source_origin: str,
    external_root: Optional[Path],
):
    if not isinstance(node_bytes, bytes) or len(node_bytes) > unityfs_data.MAX_NODE_OUTPUT_BYTES:
        raise SpriteExportError("UnityFS node exceeds the reader limit")
    return _build_export_from_stream(
        io.BytesIO(node_bytes), len(node_bytes), sprite_index, source_origin, external_root
    )


def _normalize_sprite_indices(sprite_indices: Sequence[int]) -> tuple[int, ...]:
    if not isinstance(sprite_indices, Sequence) or isinstance(sprite_indices, (str, bytes)):
        raise SpriteExportError("Sprite ordinals are invalid")
    indices = tuple(sprite_indices)
    if not indices or len(indices) > MAX_EXPORTED_SPRITES:
        raise SpriteExportError("Sprite ordinal count is outside supported limits")
    if any(isinstance(index, bool) or not isinstance(index, int) or index < 0 for index in indices):
        raise SpriteExportError("Sprite ordinal is invalid")
    if len(set(indices)) != len(indices):
        raise SpriteExportError("Sprite ordinals must be unique")
    return tuple(sorted(indices))


def _write_sprite_batch(
    serialized_file: Optional[str],
    *,
    source_root: str,
    sprite_indices: Sequence[int],
    source_origin: str,
    output_directory: str,
    external_data_root: Optional[str] = None,
    bundle: Optional[str] = None,
    node_index: Optional[int] = None,
) -> tuple[tuple[SpriteExportSummary, ...], int]:
    indices = _normalize_sprite_indices(sprite_indices)
    if (serialized_file is None) == (bundle is None):
        raise SpriteExportError("provide exactly one serialized source")
    if (bundle is None) != (node_index is None):
        raise SpriteExportError("UnityFS bundle requires one node index")
    staging: Optional[Path] = None
    source_stream: Optional[BinaryIO] = None
    try:
        root = _resolve_source_root(source_root)
        external_root = _resolve_external_root(external_data_root, root)
        destination = _resolve_new_output_directory(output_directory, root)
        if bundle is not None:
            bundle_path = _resolve_bundle_file(bundle, root)
            node_bytes = _read_unityfs_node(bundle_path, node_index)
            source_stream = io.BytesIO(node_bytes)
            available_size = len(node_bytes)
        else:
            source = _resolve_serialized_file(serialized_file, root)
            source_stream = source.open("rb")
            available_size = source.stat().st_size
        parsed_source = _parse_export_metadata(source_stream, available_size)
        build_export = lambda index: _build_export_from_stream(
            source_stream,
            available_size,
            index,
            source_origin,
            external_root,
            parsed_source,
        )
        staging = Path(tempfile.mkdtemp(prefix=".pc2ps3-sprites-", dir=str(destination.parent)))
        dimensions = []
        manifest_sprites = []
        total_png_bytes = 0
        for output_index, sprite_index in enumerate(indices):
            width, height, png = build_export(sprite_index)
            total_png_bytes += len(png)
            if total_png_bytes > MAX_EXPORTED_BATCH_BYTES:
                raise SpriteExportError("Sprite batch exceeds supported output limit")
            filename = "sprite_{:06d}.png".format(output_index)
            (staging / filename).write_bytes(png)
            manifest_sprites.append(
                {"id": "sprite-{:06d}".format(output_index), "path": filename}
            )
            dimensions.append(SpriteExportSummary(width, height))
        manifest = {"schema_version": 1, "sprites": manifest_sprites}
        manifest_bytes = json.dumps(
            manifest, ensure_ascii=True, sort_keys=True, separators=(",", ":")
        ).encode("utf-8")
        (staging / "manifest.json").write_bytes(manifest_bytes)
        staging.rename(destination)
        return tuple(dimensions), total_png_bytes
    except SpriteExportError:
        if staging is not None and staging.exists():
            shutil.rmtree(staging)
        raise
    except Exception:
        if staging is not None and staging.exists():
            shutil.rmtree(staging)
        raise SpriteExportError("Sprite batch export failed") from None
    finally:
        if source_stream is not None:
            source_stream.close()


def export_sprites(
    serialized_file: str,
    *,
    source_root: str,
    sprite_indices: Sequence[int],
    source_origin: str,
    output_directory: str,
    external_data_root: Optional[str] = None,
) -> SpriteBatchExportSummary:
    """Export selected Sprites from a standalone SerializedFile."""
    dimensions, total_png_bytes = _write_sprite_batch(
        serialized_file,
        source_root=source_root,
        sprite_indices=sprite_indices,
        source_origin=source_origin,
        output_directory=output_directory,
        external_data_root=external_data_root,
    )
    return SpriteBatchExportSummary(len(dimensions), total_png_bytes, dimensions)


def export_sprites_from_bundle(
    bundle: str,
    *,
    source_root: str,
    node_index: int,
    sprite_indices: Sequence[int],
    source_origin: str,
    output_directory: str,
    external_data_root: Optional[str] = None,
) -> SpriteBatchExportSummary:
    """Export selected Sprites from one bounded UnityFS node in memory."""
    dimensions, total_png_bytes = _write_sprite_batch(
        None,
        source_root=source_root,
        sprite_indices=sprite_indices,
        source_origin=source_origin,
        output_directory=output_directory,
        external_data_root=external_data_root,
        bundle=bundle,
        node_index=node_index,
    )
    return SpriteBatchExportSummary(len(dimensions), total_png_bytes, dimensions)


def export_sprite(
    serialized_file: str,
    *,
    source_root: str,
    sprite_index: int,
    source_origin: str,
    output_directory: str,
    external_data_root: Optional[str] = None,
) -> SpriteExportSummary:
    """Export one Sprite into a new, generated-name directory outside the repo."""
    dimensions, _ = _write_sprite_batch(
        serialized_file,
        source_root=source_root,
        sprite_indices=(sprite_index,),
        source_origin=source_origin,
        output_directory=output_directory,
        external_data_root=external_data_root,
    )
    return dimensions[0]


def main(argv: Optional[Sequence[str]] = None) -> int:
    parser = argparse.ArgumentParser(
        prog="sprite-export",
        description="Export selected Unity Sprites to a bounded PNG manifest.",
    )
    parser.add_argument("--source-root", required=True, help="absolute root containing local game assets")
    parser.add_argument("--serialized-file", help="standalone SerializedFile v21 path")
    parser.add_argument("--bundle", help="UnityFS bundle path under the source root")
    parser.add_argument("--node-index", type=int, help="zero-based node ordinal for --bundle")
    parser.add_argument(
        "--sprite-index",
        action="append",
        type=int,
        required=True,
        help="zero-based Sprite ordinal; repeat to export a deterministic batch",
    )
    parser.add_argument("--source-origin", required=True, choices=("top-left", "bottom-left"))
    parser.add_argument("--output-dir", required=True, help="new absolute destination outside source and project roots")
    parser.add_argument("--external-data-root", help="optional absolute root for validated external streams")
    if argv and argv[0] == "sprite-export":
        argv = argv[1:]
    args = parser.parse_args(argv)
    if (args.serialized_file is None) == (args.bundle is None):
        parser.error("provide exactly one of --serialized-file or --bundle")
    if args.bundle is not None and args.node_index is None:
        parser.error("--bundle requires --node-index")
    if args.bundle is None and args.node_index is not None:
        parser.error("--node-index requires --bundle")
    if args.node_index is not None and args.node_index < 0:
        parser.error("--node-index must be nonnegative")
    try:
        common = {
            "source_root": args.source_root,
            "sprite_indices": args.sprite_index,
            "source_origin": args.source_origin,
            "output_directory": args.output_dir,
            "external_data_root": args.external_data_root,
        }
        if args.bundle is not None:
            result = export_sprites_from_bundle(
                args.bundle, node_index=args.node_index, **common
            )
        else:
            result = export_sprites(args.serialized_file, **common)
    except SpriteExportError:
        sys.stderr.write("sprite export failed\n")
        return 2
    if result.exported == 1:
        width, height = result.dimensions[0]
        payload = {"exported": 1, "width": width, "height": height}
    else:
        payload = {
            "exported": result.exported,
            "total_png_bytes": result.total_png_bytes,
        }
    sys.stdout.write(json.dumps(payload, sort_keys=True, separators=(",", ":")) + "\n")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

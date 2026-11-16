#!/usr/bin/env python3
"""Export selected UnityFS nodes without trusting paths stored in the bundle."""

import argparse
import os
import stat
import sys
import tempfile
from pathlib import Path

from unityfs_data import MAX_NODE_OUTPUT_BYTES, read_node_bytes
from unityfs_header import UnityFSHeaderError, parse_data_block_layout

MAX_SELECTED_NODES = 64
MAX_TOTAL_OUTPUT_BYTES = MAX_NODE_OUTPUT_BYTES


def _has_reparse_point(path):
    try:
        info = path.lstat()
    except FileNotFoundError:
        return False
    reparse_flag = getattr(stat, "FILE_ATTRIBUTE_REPARSE_POINT", 0x400)
    return stat.S_ISLNK(info.st_mode) or bool(getattr(info, "st_file_attributes", 0) & reparse_flag)


def _validate_output_directory(path):
    path = Path(os.path.abspath(os.fspath(path)))
    for candidate in (path,) + tuple(path.parents):
        if _has_reparse_point(candidate):
            raise ValueError("output directory contains a symbolic link or reparse point")
    if path.exists() and not path.is_dir():
        raise ValueError("output path is not a directory")
    return path


def _path_exists(path):
    try:
        path.lstat()
        return True
    except FileNotFoundError:
        return False


def _publish_one(temporary, target):
    if os.name == "nt":
        # Windows rename is atomic and fails when the destination already exists.
        os.rename(os.fspath(temporary), os.fspath(target))
    else:
        os.link(os.fspath(temporary), os.fspath(target))


def _publish_atomically(output_dir, payloads):
    staged = []
    published = []
    try:
        for node_index, payload in payloads:
            target = output_dir / "node_{:06d}.bin".format(node_index)
            if _path_exists(target):
                raise FileExistsError("output already exists: {}".format(target.name))
            descriptor, temporary_name = tempfile.mkstemp(
                prefix=".pc2ps3-unityfs-", suffix=".tmp", dir=os.fspath(output_dir)
            )
            temporary = Path(temporary_name)
            staged.append((temporary, target))
            with os.fdopen(descriptor, "wb") as output:
                output.write(payload)
                output.flush()
                os.fsync(output.fileno())

        for temporary, target in staged:
            if _path_exists(target):
                raise FileExistsError("output already exists: {}".format(target.name))
            info = temporary.stat()
            _publish_one(temporary, target)
            published.append((target, info.st_dev, info.st_ino))
            if _path_exists(temporary):
                temporary.unlink()

        return [target for _, target in staged]
    except BaseException:
        for target, device, inode in reversed(published):
            try:
                info = target.lstat()
                if info.st_dev == device and info.st_ino == inode:
                    target.unlink()
            except OSError:
                pass
        raise
    finally:
        for temporary, _ in staged:
            try:
                temporary.unlink()
            except OSError:
                pass


def _extract(bundle_path, output_path, node_indices):
    if not node_indices:
        raise ValueError("select at least one node index")
    if len(node_indices) > MAX_SELECTED_NODES:
        raise ValueError("selection exceeds the {}-node limit".format(MAX_SELECTED_NODES))
    if any(index < 0 for index in node_indices):
        raise ValueError("node indexes must be non-negative")
    if len(set(node_indices)) != len(node_indices):
        raise ValueError("node indexes must be unique")

    bundle_path = Path(bundle_path)
    if not bundle_path.is_file():
        raise ValueError("bundle path is not a regular file")
    output_dir = _validate_output_directory(Path(output_path))

    with bundle_path.open("rb") as stream:
        layout = parse_data_block_layout(stream, bundle_path.stat().st_size)
        nodes = layout["nodes"]
        if any(index >= len(nodes) for index in node_indices):
            raise UnityFSHeaderError("UnityFS node index is outside the directory")
        sizes = [nodes[index]["size"] for index in node_indices]
        if sum(sizes) > MAX_TOTAL_OUTPUT_BYTES:
            raise UnityFSHeaderError("selected nodes exceed the output limit")

        targets = [output_dir / "node_{:06d}.bin".format(index) for index in node_indices]
        if any(_path_exists(target) for target in targets):
            raise FileExistsError("an output file already exists")

        payloads = []
        for index, expected_size in zip(node_indices, sizes):
            payload = read_node_bytes(stream, layout, index)
            if len(payload) != expected_size:
                raise UnityFSHeaderError("UnityFS node output size does not match metadata")
            payloads.append((index, payload))

    output_dir.mkdir(parents=True, exist_ok=True)
    output_dir = _validate_output_directory(output_dir)
    return _publish_atomically(output_dir, payloads)


def _argument_parser():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--bundle", required=True, type=Path, help="UnityFS bundle to read")
    parser.add_argument("--output-dir", required=True, type=Path, help="separate directory for node files")
    parser.add_argument(
        "--node-index", action="append", required=True, type=int, metavar="INDEX",
        help="node index to export; repeat for more nodes (maximum 64)",
    )
    return parser


def main(argv=None):
    args = _argument_parser().parse_args(argv)
    try:
        paths = _extract(args.bundle, args.output_dir, args.node_index)
    except (OSError, ValueError, UnityFSHeaderError) as error:
        print("error: {}".format(error), file=sys.stderr)
        return 1
    for path in paths:
        print(path.name)
    return 0


if __name__ == "__main__":
    sys.exit(main())

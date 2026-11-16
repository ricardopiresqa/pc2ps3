#!/usr/bin/env python3
"""Read bounded UnityFS node payloads using the validated data-block layout."""

from typing import BinaryIO, Dict

import unityfs_header


MAX_NODE_OUTPUT_BYTES = 256 * 1024 * 1024
MAX_NODE_BLOCK_BYTES = unityfs_header.MAX_LZ4_BLOCK_BYTES


def _read_exact(stream: BinaryIO, count: int) -> bytes:
    if count < 0 or count > MAX_NODE_BLOCK_BYTES:
        raise unityfs_header.UnityFSHeaderError("UnityFS compressed block exceeds reader limit")
    output = bytearray()
    while len(output) < count:
        try:
            chunk = stream.read(count - len(output))
        except (OSError, ValueError) as error:
            raise unityfs_header.UnityFSHeaderError(
                "cannot read UnityFS data block: {}".format(error)
            )
        if not chunk:
            raise unityfs_header.UnityFSHeaderError("truncated UnityFS data block")
        if len(chunk) > count - len(output):
            raise unityfs_header.UnityFSHeaderError("stream returned too many UnityFS bytes")
        output.extend(chunk)
    return bytes(output)


def _nonnegative_int(value, label: str) -> int:
    if isinstance(value, bool) or not isinstance(value, int) or value < 0:
        raise unityfs_header.UnityFSHeaderError("invalid UnityFS {}".format(label))
    return value


def read_node_bytes(
    stream: BinaryIO,
    layout: Dict[str, object],
    node_index: int,
    max_output_bytes: int = MAX_NODE_OUTPUT_BYTES,
) -> bytes:
    """Read one directory node without loading unrelated blocks or returning partial data."""
    if isinstance(node_index, bool) or not isinstance(node_index, int):
        raise unityfs_header.UnityFSHeaderError("UnityFS node index must be an integer")
    if (
        isinstance(max_output_bytes, bool)
        or not isinstance(max_output_bytes, int)
        or max_output_bytes < 0
        or max_output_bytes > MAX_NODE_OUTPUT_BYTES
    ):
        raise unityfs_header.UnityFSHeaderError("UnityFS node output limit is invalid")

    try:
        nodes = layout["nodes"]
        blocks = layout["blocks"]
        data_end = _nonnegative_int(layout["data_end"], "data end")
        data_offset = _nonnegative_int(layout["data_offset"], "data offset")
    except (KeyError, TypeError) as error:
        raise unityfs_header.UnityFSHeaderError("invalid UnityFS data layout: {}".format(error))
    if not isinstance(nodes, list) or not isinstance(blocks, list):
        raise unityfs_header.UnityFSHeaderError("invalid UnityFS node or block table")
    if len(nodes) > unityfs_header.MAX_NODE_COUNT or len(blocks) > unityfs_header.MAX_BLOCK_COUNT:
        raise unityfs_header.UnityFSHeaderError("UnityFS node or block count exceeds reader limit")
    if node_index < 0 or node_index >= len(nodes):
        raise unityfs_header.UnityFSHeaderError("UnityFS node index is outside the directory")

    node = nodes[node_index]
    try:
        node_offset = _nonnegative_int(node["offset"], "node offset")
        node_size = _nonnegative_int(node["size"], "node size")
    except (KeyError, TypeError) as error:
        raise unityfs_header.UnityFSHeaderError("invalid UnityFS node record: {}".format(error))
    if node_size > max_output_bytes:
        raise unityfs_header.UnityFSHeaderError("UnityFS node exceeds output limit")

    if data_offset > data_end:
        raise unityfs_header.UnityFSHeaderError("UnityFS data range is inverted")

    logical_total = 0
    previous_file_end = data_offset
    checked_blocks = []
    for block in blocks:
        try:
            file_offset = _nonnegative_int(block["file_offset"], "block file offset")
            logical_offset = _nonnegative_int(block["uncompressed_offset"], "block logical offset")
            compressed_size = _nonnegative_int(block["compressed_size"], "compressed block size")
            uncompressed_size = _nonnegative_int(block["uncompressed_size"], "uncompressed block size")
        except (KeyError, TypeError) as error:
            raise unityfs_header.UnityFSHeaderError("invalid UnityFS block record: {}".format(error))
        if logical_offset != logical_total:
            raise unityfs_header.UnityFSHeaderError("UnityFS logical blocks are not contiguous")
        if file_offset < previous_file_end or file_offset > data_end:
            raise unityfs_header.UnityFSHeaderError("UnityFS physical block ranges overlap or exceed data")
        if compressed_size > data_end - file_offset:
            raise unityfs_header.UnityFSHeaderError("UnityFS physical block exceeds data region")
        checked_blocks.append((block, file_offset, logical_offset, compressed_size, uncompressed_size))
        previous_file_end = file_offset + compressed_size
        logical_total += uncompressed_size

    node_end = node_offset + node_size
    if node_end > logical_total:
        raise unityfs_header.UnityFSHeaderError("UnityFS node range exceeds logical data")
    if node_size == 0:
        return b""

    output = bytearray()
    for block, file_offset, logical_offset, compressed_size, uncompressed_size in checked_blocks:
        block_end = logical_offset + uncompressed_size
        overlap_start = max(node_offset, logical_offset)
        overlap_end = min(node_end, block_end)
        if overlap_start >= overlap_end:
            continue
        if uncompressed_size > MAX_NODE_BLOCK_BYTES or compressed_size > MAX_NODE_BLOCK_BYTES:
            raise unityfs_header.UnityFSHeaderError("UnityFS block exceeds reader limit")
        compression = block.get("compression")
        if compression not in ("none", "lz4", "lz4hc"):
            raise unityfs_header.UnityFSHeaderError(
                "unsupported UnityFS data-block compression: {}".format(compression)
            )
        if compression == "none":
            if compressed_size != uncompressed_size:
                raise unityfs_header.UnityFSHeaderError(
                    "uncompressed UnityFS block sizes do not match"
                )

        try:
            stream.seek(file_offset)
        except (AttributeError, OSError, ValueError) as error:
            raise unityfs_header.UnityFSHeaderError(
                "cannot seek to UnityFS data block: {}".format(error)
            )
        compressed = _read_exact(stream, compressed_size)
        if compression == "none":
            decoded = compressed
        else:
            decoded = unityfs_header.decompress_lz4_block(
                compressed, uncompressed_size, MAX_NODE_BLOCK_BYTES
            )
        if len(decoded) != uncompressed_size:
            raise unityfs_header.UnityFSHeaderError("UnityFS block output size does not match")

        start = overlap_start - logical_offset
        end = overlap_end - logical_offset
        output.extend(decoded[start:end])
        if len(output) > node_size or len(output) > max_output_bytes:
            raise unityfs_header.UnityFSHeaderError("UnityFS node output exceeds declared size")

    if len(output) != node_size:
        raise unityfs_header.UnityFSHeaderError("UnityFS node output size does not match")
    return bytes(output)

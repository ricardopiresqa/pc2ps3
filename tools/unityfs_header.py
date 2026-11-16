#!/usr/bin/env python3
"""Read only the fixed UnityFS bundle header; never unpack its payload."""

import argparse
import json
import os
import re
import stat
import struct
from pathlib import Path
from typing import BinaryIO, Dict


class UnityFSHeaderError(ValueError):
    """Raised when a UnityFS header is malformed or outside supported limits."""


FORMAT_VERSION = 6
MAX_STRING_BYTES = 256
MAX_UNCOMPRESSED_INFO_BYTES = 128 * 1024 * 1024
MAX_LZ4_BLOCK_BYTES = 128 * 1024 * 1024
MAX_DIRECTORY_INFO_BYTES = 16 * 1024 * 1024
MAX_COMPRESSED_INFO_BYTES = 16 * 1024 * 1024
MAX_BLOCK_COUNT = 65536
MAX_NODE_COUNT = 100000
MAX_NODE_PATH_BYTES = 4096
COMPRESSION_MASK = 0x3F
KNOWN_FLAG_MASK = 0x3FF
COMPRESSION_NAMES = {0: "none", 1: "lzma", 2: "lz4", 3: "lz4hc"}
BLOCK_INFO_AT_END = 0x80
BLOCK_INFO_NEED_PADDING_AT_START = 0x200
SUPPORTED_ARCHIVE_FLAGS = COMPRESSION_MASK | 0x40 | BLOCK_INFO_AT_END | 0x200
SUPPORTED_STORAGE_BLOCK_FLAGS = COMPRESSION_MASK | 0x40


def decompress_lz4_block(
    block: bytes,
    expected_size: int,
    max_output_size: int = MAX_UNCOMPRESSED_INFO_BYTES,
) -> bytes:
    """Decode one raw LZ4 block within explicit input and output limits."""
    if not isinstance(block, bytes):
        raise UnityFSHeaderError("LZ4 block must be bytes")
    if len(block) > MAX_LZ4_BLOCK_BYTES:
        raise UnityFSHeaderError("LZ4 block exceeds input size limit")
    if isinstance(max_output_size, bool) or not isinstance(max_output_size, int):
        raise UnityFSHeaderError("LZ4 output limit must be an integer")
    if max_output_size < 0 or max_output_size > MAX_UNCOMPRESSED_INFO_BYTES:
        raise UnityFSHeaderError("LZ4 output limit is outside supported range")
    if isinstance(expected_size, bool) or not isinstance(expected_size, int):
        raise UnityFSHeaderError("LZ4 expected size must be an integer")
    if expected_size < 0 or expected_size > max_output_size:
        raise UnityFSHeaderError("LZ4 expected size exceeds output limit")

    source = memoryview(block)
    output = bytearray()
    position = 0

    def read_extended_length(length: int, available: int, label: str) -> int:
        nonlocal position
        while True:
            if position >= len(source):
                raise UnityFSHeaderError("truncated LZ4 {} length".format(label))
            extension = source[position]
            position += 1
            if length > available - extension:
                raise UnityFSHeaderError("LZ4 {} exceeds output size".format(label))
            length += extension
            if extension != 255:
                return length

    while position < len(source):
        token = source[position]
        position += 1

        literal_length = token >> 4
        if literal_length == 15:
            literal_length = read_extended_length(
                literal_length, expected_size - len(output), "literal run"
            )
        if literal_length > expected_size - len(output):
            raise UnityFSHeaderError("LZ4 literal run exceeds output size")
        literal_end = position + literal_length
        if literal_end > len(source):
            raise UnityFSHeaderError("truncated LZ4 literal run")
        output.extend(source[position:literal_end])
        position = literal_end

        if position == len(source):
            break
        if len(source) - position < 2:
            raise UnityFSHeaderError("truncated LZ4 match offset")
        offset = source[position] | (source[position + 1] << 8)
        position += 2
        if offset == 0:
            raise UnityFSHeaderError("LZ4 match offset cannot be zero")
        if offset > len(output):
            raise UnityFSHeaderError("LZ4 match offset exceeds produced output")

        match_nibble = token & 0x0F
        match_extra = match_nibble
        if match_nibble == 15:
            match_extra = read_extended_length(
                match_nibble,
                expected_size - len(output) - 4,
                "match run",
            )
        match_length = match_extra + 4
        if match_length > expected_size - len(output):
            raise UnityFSHeaderError("LZ4 match run exceeds output size")
        for _ in range(match_length):
            output.append(output[-offset])

    if len(output) != expected_size:
        raise UnityFSHeaderError("LZ4 output size does not match expected size")
    return bytes(output)


def _read_exact(stream: BinaryIO, count: int, label: str) -> bytes:
    data = stream.read(count)
    if len(data) != count:
        raise UnityFSHeaderError("truncated {}".format(label))
    return data


def _read_cstring(stream: BinaryIO, label: str) -> str:
    value = bytearray()
    for _ in range(MAX_STRING_BYTES):
        character = stream.read(1)
        if not character:
            raise UnityFSHeaderError("unterminated {} string".format(label))
        if character == b"\0":
            try:
                return value.decode("utf-8")
            except UnicodeDecodeError as error:
                raise UnityFSHeaderError("{} string is not valid UTF-8: {}".format(label, error))
        value.extend(character)
    raise UnityFSHeaderError("{} string exceeds the {} byte limit".format(label, MAX_STRING_BYTES))


def parse_header(stream: BinaryIO, file_size: int) -> Dict[str, object]:
    """Parse fields incrementally from a seekable stream with a known file size."""
    if isinstance(file_size, bool) or not isinstance(file_size, int) or file_size <= 0:
        raise UnityFSHeaderError("file size must be a positive integer")
    signature = _read_cstring(stream, "signature")
    if signature != "UnityFS":
        raise UnityFSHeaderError("unsupported bundle signature: {}".format(signature))
    version = struct.unpack(">I", _read_exact(stream, 4, "format version"))[0]
    if version != FORMAT_VERSION:
        raise UnityFSHeaderError("unsupported UnityFS format version: {}".format(version))
    player_version = _read_cstring(stream, "player version")
    engine_version = _read_cstring(stream, "engine version")
    total_size = struct.unpack(">Q", _read_exact(stream, 8, "total size"))[0]
    compressed_info_size, uncompressed_info_size, flags = struct.unpack(
        ">III", _read_exact(stream, 12, "block-info sizes and flags")
    )
    header_size = stream.tell()

    if total_size != file_size:
        raise UnityFSHeaderError("declared total size does not match file size")
    if total_size < header_size:
        raise UnityFSHeaderError("declared total size is smaller than the header")
    if compressed_info_size == 0 or uncompressed_info_size == 0:
        raise UnityFSHeaderError("block-info sizes must be nonzero")
    if compressed_info_size > total_size - header_size:
        raise UnityFSHeaderError("compressed block-info size exceeds remaining bundle bytes")
    if uncompressed_info_size > MAX_UNCOMPRESSED_INFO_BYTES:
        raise UnityFSHeaderError("uncompressed block-info size exceeds the reader limit")
    if flags & ~KNOWN_FLAG_MASK:
        raise UnityFSHeaderError("bundle flags contain unsupported bits")
    compression = flags & COMPRESSION_MASK
    if compression not in COMPRESSION_NAMES:
        raise UnityFSHeaderError("bundle flags specify an unknown compression type")

    return {
        "signature": signature,
        "format_version": version,
        "player_version": player_version,
        "engine_version": engine_version,
        "total_size": total_size,
        "compressed_block_info_size": compressed_info_size,
        "uncompressed_block_info_size": uncompressed_info_size,
        "flags": flags,
        "compression": COMPRESSION_NAMES[compression],
        "header_size": header_size,
    }


def _parse_block_info(block_info: bytes) -> Dict[str, object]:
    if len(block_info) < 20:
        raise UnityFSHeaderError("truncated UnityFS block-info header")
    checksum = block_info[:16]
    position = 16
    block_count = struct.unpack_from(">i", block_info, position)[0]
    position += 4
    if block_count < 0 or block_count > MAX_BLOCK_COUNT:
        raise UnityFSHeaderError("UnityFS block count is outside supported range")
    if block_count > (len(block_info) - position) // 10:
        raise UnityFSHeaderError("truncated UnityFS block table")

    blocks = []
    compressed_total = 0
    uncompressed_total = 0
    max_signed_size = (1 << 63) - 1
    for _ in range(block_count):
        uncompressed_size, compressed_size, flags = struct.unpack_from(">IIH", block_info, position)
        position += 10
        if flags & ~SUPPORTED_STORAGE_BLOCK_FLAGS:
            raise UnityFSHeaderError("UnityFS storage block has unsupported flags")
        if (flags & COMPRESSION_MASK) not in COMPRESSION_NAMES:
            raise UnityFSHeaderError("UnityFS storage block uses unsupported compression")
        if (flags & COMPRESSION_MASK) == 0 and compressed_size != uncompressed_size:
            raise UnityFSHeaderError("uncompressed UnityFS block sizes do not match")
        if uncompressed_size > max_signed_size - uncompressed_total:
            raise UnityFSHeaderError("UnityFS uncompressed block-size total overflows")
        compressed_total += compressed_size
        uncompressed_total += uncompressed_size
        blocks.append({
            "uncompressed_size": uncompressed_size,
            "compressed_size": compressed_size,
            "flags": flags,
        })

    if len(block_info) - position < 4:
        raise UnityFSHeaderError("truncated UnityFS node count")
    node_count = struct.unpack_from(">i", block_info, position)[0]
    position += 4
    if node_count < 0 or node_count > MAX_NODE_COUNT:
        raise UnityFSHeaderError("UnityFS node count is outside supported range")
    if node_count > (len(block_info) - position) // 21:
        raise UnityFSHeaderError("truncated UnityFS node table")

    nodes = []
    seen_paths = set()
    for _ in range(node_count):
        offset, size, flags = struct.unpack_from(">qqI", block_info, position)
        position += 20
        if (
            offset < 0
            or size < 0
            or offset > uncompressed_total
            or size > uncompressed_total - offset
        ):
            raise UnityFSHeaderError("UnityFS node range is outside uncompressed blocks")
        path_end = block_info.find(
            b"\0", position, min(len(block_info), position + MAX_NODE_PATH_BYTES + 1)
        )
        if path_end < 0:
            if len(block_info) - position > MAX_NODE_PATH_BYTES:
                raise UnityFSHeaderError("UnityFS node path exceeds supported length")
            raise UnityFSHeaderError("unterminated UnityFS node path")
        if path_end == position:
            raise UnityFSHeaderError("UnityFS node path cannot be empty")
        if path_end - position > MAX_NODE_PATH_BYTES:
            raise UnityFSHeaderError("UnityFS node path exceeds supported length")
        try:
            path = block_info[position:path_end].decode("utf-8")
        except UnicodeDecodeError as error:
            raise UnityFSHeaderError("UnityFS node path is not valid UTF-8: {}".format(error))
        parts = path.split("/")
        if (
            path.startswith("/")
            or "\\" in path
            or (len(path) >= 2 and path[0].isalpha() and path[1] == ":")
            or any(part in ("", ".", "..") for part in parts)
        ):
            raise UnityFSHeaderError("UnityFS node path is not canonical relative POSIX path")
        if path in seen_paths:
            raise UnityFSHeaderError("duplicate UnityFS node path")
        seen_paths.add(path)
        nodes.append({"offset": offset, "size": size, "flags": flags, "path": path})
        position = path_end + 1

    if position != len(block_info):
        raise UnityFSHeaderError("unexpected trailing bytes in UnityFS block-info")
    return {
        "md5": checksum.hex(),
        "blocks": blocks,
        "nodes": nodes,
        "compressed_block_size_total": compressed_total,
        "uncompressed_block_size_total": uncompressed_total,
    }


def parse_directory_info(stream: BinaryIO, file_size: int) -> Dict[str, object]:
    """Read and validate only UnityFS v6 block-info metadata, never asset blocks."""
    try:
        stream.seek(0)
    except (AttributeError, OSError, ValueError) as error:
        raise UnityFSHeaderError("UnityFS block-info requires a seekable stream: {}".format(error))
    header = parse_header(stream, file_size)
    flags = int(header["flags"])
    if flags & ~SUPPORTED_ARCHIVE_FLAGS:
        raise UnityFSHeaderError("UnityFS archive contains unsupported flags")
    compression = flags & COMPRESSION_MASK
    if compression not in (0, 2, 3):
        raise UnityFSHeaderError("unsupported UnityFS block-info compression")

    compressed_size = int(header["compressed_block_info_size"])
    uncompressed_size = int(header["uncompressed_block_info_size"])
    if compressed_size > MAX_COMPRESSED_INFO_BYTES:
        raise UnityFSHeaderError("compressed UnityFS block-info exceeds reader limit")
    if uncompressed_size > MAX_DIRECTORY_INFO_BYTES:
        raise UnityFSHeaderError("uncompressed UnityFS block-info exceeds parser limit")
    header_size = int(header["header_size"])
    total_size = int(header["total_size"])
    if flags & BLOCK_INFO_AT_END:
        block_info_offset = total_size - compressed_size
        data_range_size = block_info_offset - header_size
    else:
        block_info_offset = header_size
        data_range_size = total_size - header_size - compressed_size
    if block_info_offset < header_size or data_range_size < 0:
        raise UnityFSHeaderError("UnityFS block-info range is inconsistent")
    try:
        stream.seek(block_info_offset)
    except (AttributeError, OSError, ValueError) as error:
        raise UnityFSHeaderError("cannot seek to UnityFS block-info: {}".format(error))
    compressed_info = _read_exact(stream, compressed_size, "compressed block-info")
    if compression == 0:
        if compressed_size != uncompressed_size:
            raise UnityFSHeaderError("uncompressed block-info sizes do not match")
        block_info = compressed_info
    else:
        block_info = decompress_lz4_block(
            compressed_info, uncompressed_size, MAX_DIRECTORY_INFO_BYTES
        )

    parsed = _parse_block_info(block_info)
    if parsed["compressed_block_size_total"] > data_range_size:
        raise UnityFSHeaderError("UnityFS block sizes exceed the asset-data range")
    parsed.update(header)
    parsed["block_info_offset"] = block_info_offset
    parsed["block_info_compressed_size"] = compressed_size
    return parsed


def _engine_uses_block_padding(engine_version: str) -> bool:
    match = re.match(r"^(\d+)\.(\d+)\.(\d+)", engine_version)
    if match is None:
        return False
    version = tuple(int(part) for part in match.groups())
    thresholds = {
        (2019, 4): 15,
        (2020, 3): 34,
        (2021, 3): 2,
        (2022, 1): 1,
    }
    major, minor, patch = version
    threshold = thresholds.get((major, minor))
    if threshold is not None:
        return patch >= threshold
    return (major == 2022 and minor > 1) or major > 2022


def parse_data_block_layout(stream: BinaryIO, file_size: int) -> Dict[str, object]:
    """Return physical and logical block offsets without reading asset payloads."""
    parsed = parse_directory_info(stream, file_size)
    flags = int(parsed["flags"])
    header_size = int(parsed["header_size"])
    total_size = int(parsed["total_size"])
    block_info_offset = int(parsed["block_info_offset"])
    block_info_size = int(parsed["block_info_compressed_size"])
    block_info_at_end = bool(flags & BLOCK_INFO_AT_END)

    data_offset = header_size if block_info_at_end else header_size + block_info_size
    if flags & BLOCK_INFO_NEED_PADDING_AT_START:
        if block_info_at_end:
            raise UnityFSHeaderError(
                "UnityFS padding flag is unsupported with block-info-at-end"
            )
        if not _engine_uses_block_padding(str(parsed["engine_version"])):
            raise UnityFSHeaderError(
                "UnityFS 0x200 flag is ambiguous or encrypted for this engine version"
            )
        data_offset = (data_offset + 15) & ~15

    data_end = block_info_offset if block_info_at_end else total_size
    if data_offset > data_end:
        raise UnityFSHeaderError("UnityFS data block start exceeds its data region")

    cursor = data_offset
    logical_offset = 0
    blocks = []
    for block in parsed["blocks"]:
        compressed_size = int(block["compressed_size"])
        if compressed_size > data_end - cursor:
            raise UnityFSHeaderError("UnityFS data blocks exceed their physical region")
        mapped = dict(block)
        mapped["compression"] = COMPRESSION_NAMES[int(block["flags"]) & COMPRESSION_MASK]
        mapped["file_offset"] = cursor
        mapped["uncompressed_offset"] = logical_offset
        blocks.append(mapped)
        cursor += compressed_size
        logical_offset += int(block["uncompressed_size"])

    parsed["blocks"] = blocks
    parsed["data_offset"] = data_offset
    parsed["data_end"] = data_end
    parsed["blocks_payload_end"] = cursor
    parsed["unused_data_bytes"] = data_end - cursor
    return parsed


def read_path(path: Path) -> Dict[str, object]:
    """Read the header from a regular file without following a final reparse point."""
    path = Path(path)
    for parent in path.parents:
        try:
            parent_metadata = parent.lstat()
        except OSError as error:
            raise UnityFSHeaderError("cannot inspect bundle directory: {}".format(error))
        if (not stat.S_ISDIR(parent_metadata.st_mode) or stat.S_ISLNK(parent_metadata.st_mode)
                or bool(getattr(parent_metadata, "st_file_attributes", 0) & 0x400)):
            raise UnityFSHeaderError("bundle path contains a link or reparse point")
    try:
        metadata = path.lstat()
    except OSError as error:
        raise UnityFSHeaderError("cannot inspect bundle: {}".format(error))
    if (not stat.S_ISREG(metadata.st_mode) or stat.S_ISLNK(metadata.st_mode)
            or bool(getattr(metadata, "st_file_attributes", 0) & 0x400)):
        raise UnityFSHeaderError("bundle must be a regular file, not a link or reparse point")
    flags = os.O_RDONLY | getattr(os, "O_BINARY", 0) | getattr(os, "O_NOFOLLOW", 0)
    try:
        descriptor = os.open(str(path), flags)
        try:
            opened = os.fstat(descriptor)
            if not stat.S_ISREG(opened.st_mode) or opened.st_size != metadata.st_size:
                raise UnityFSHeaderError("bundle changed or is not a regular file")
            with os.fdopen(descriptor, "rb") as stream:
                descriptor = -1
                return parse_header(stream, opened.st_size)
        finally:
            if descriptor >= 0:
                os.close(descriptor)
    except OSError as error:
        raise UnityFSHeaderError("cannot read bundle header: {}".format(error))


def main(argv=None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("bundle", type=Path)
    args = parser.parse_args(argv)
    try:
        header = read_path(args.bundle)
    except UnityFSHeaderError as error:
        parser.error(str(error))
    print(json.dumps(header, ensure_ascii=True, indent=2, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

"""Bounded PNG codec for the local PC2PS3 asset pipeline.

Supported input is intentionally small: non-interlaced, 8-bit RGB or RGBA PNG
with only IHDR, IDAT and IEND chunks. Ancillary metadata is discarded by
rejecting it, rather than being copied into generated assets.
"""

import struct
import zlib
from typing import NamedTuple


PNG_SIGNATURE = b"\x89PNG\r\n\x1a\n"
MAX_INPUT_BYTES = 64 * 1024 * 1024
MAX_DIMENSION = 4096
MAX_PIXELS = 8 * 1024 * 1024
MAX_DECODED_BYTES = MAX_PIXELS * 4 + MAX_DIMENSION
MAX_CHUNKS = 65536


class PngError(ValueError):
    """Raised when a PNG is malformed or outside the supported contract."""


class PngImage(NamedTuple):
    width: int
    height: int
    rgba: bytes


def _paeth(left, above, upper_left):
    estimate = left + above - upper_left
    left_distance = abs(estimate - left)
    above_distance = abs(estimate - above)
    upper_left_distance = abs(estimate - upper_left)
    if left_distance <= above_distance and left_distance <= upper_left_distance:
        return left
    if above_distance <= upper_left_distance:
        return above
    return upper_left


def _unfilter(filter_type, filtered, previous, bytes_per_pixel):
    if filter_type > 4:
        raise PngError("unsupported PNG row filter")
    row = bytearray(len(filtered))
    for index, value in enumerate(filtered):
        left = row[index - bytes_per_pixel] if index >= bytes_per_pixel else 0
        above = previous[index]
        upper_left = previous[index - bytes_per_pixel] if index >= bytes_per_pixel else 0
        if filter_type == 0:
            predictor = 0
        elif filter_type == 1:
            predictor = left
        elif filter_type == 2:
            predictor = above
        elif filter_type == 3:
            predictor = (left + above) // 2
        else:
            predictor = _paeth(left, above, upper_left)
        row[index] = (value + predictor) & 0xFF
    return row


def decode_png(data, max_pixels=MAX_PIXELS):
    """Decode supported PNG bytes to immutable RGBA pixels."""
    if not isinstance(data, (bytes, bytearray)):
        raise PngError("PNG input must be bytes")
    if len(data) > MAX_INPUT_BYTES:
        raise PngError("PNG input exceeds byte limit")
    data_view = memoryview(data)
    if len(data_view) < len(PNG_SIGNATURE) or bytes(data_view[:8]) != PNG_SIGNATURE:
        raise PngError("invalid PNG signature")
    if type(max_pixels) is not int or max_pixels <= 0:
        raise PngError("pixel limit must be a positive integer")

    offset = 8
    width = height = color_type = None
    chunk_count = 0
    bytes_per_pixel = row_bytes = expected_size = None
    filtered = inflater = None
    seen_idat = False
    seen_iend = False
    while offset < len(data_view):
        chunk_count += 1
        if chunk_count > MAX_CHUNKS:
            raise PngError("PNG exceeds chunk-count limit")
        if offset + 12 > len(data_view):
            raise PngError("truncated PNG chunk")
        length = struct.unpack_from(">I", data_view, offset)[0]
        kind = bytes(data_view[offset + 4:offset + 8])
        end = offset + 12 + length
        if length > MAX_INPUT_BYTES or end > len(data_view):
            raise PngError("PNG chunk length is invalid")
        payload_start = offset + 8
        payload_end = payload_start + length
        payload = data_view[payload_start:payload_end]
        expected_crc = struct.unpack_from(">I", data_view, payload_end)[0]
        actual_crc = zlib.crc32(payload, zlib.crc32(kind)) & 0xFFFFFFFF
        if actual_crc != expected_crc:
            raise PngError("PNG chunk CRC mismatch")

        if width is None:
            if kind != b"IHDR" or length != 13:
                raise PngError("PNG must begin with a valid IHDR")
            width, height, bit_depth, color_type, compression, filtering, interlace = struct.unpack(
                ">IIBBBBB", payload
            )
            if width <= 0 or height <= 0 or width > MAX_DIMENSION or height > MAX_DIMENSION:
                raise PngError("PNG dimensions are outside supported limits")
            if width * height > min(max_pixels, MAX_PIXELS):
                raise PngError("PNG exceeds pixel limit")
            if bit_depth != 8 or color_type not in (2, 6):
                raise PngError("only 8-bit RGB and RGBA PNG are supported")
            if compression != 0 or filtering != 0 or interlace != 0:
                raise PngError("PNG compression, filter or interlace method is unsupported")
            bytes_per_pixel = 3 if color_type == 2 else 4
            row_bytes = width * bytes_per_pixel
            expected_size = height * (row_bytes + 1)
            if expected_size > MAX_DECODED_BYTES:
                raise PngError("decoded PNG exceeds byte limit")
            filtered = bytearray()
            inflater = zlib.decompressobj()
        elif kind == b"IHDR":
            raise PngError("duplicate PNG header")
        elif kind == b"IDAT":
            if seen_iend:
                raise PngError("PNG image data follows IEND")
            seen_idat = True
            remaining = expected_size + 1 - len(filtered)
            if remaining <= 0:
                raise PngError("PNG decompressed size exceeds dimensions")
            try:
                filtered.extend(inflater.decompress(payload, remaining))
            except zlib.error as error:
                raise PngError("PNG image data is invalid") from error
            if len(filtered) > expected_size or inflater.unconsumed_tail:
                raise PngError("PNG decompressed size exceeds dimensions")
        elif kind == b"IEND":
            if length != 0 or not seen_idat:
                raise PngError("invalid PNG end chunk")
            seen_iend = True
            offset = end
            if offset != len(data_view):
                raise PngError("trailing bytes after PNG end chunk")
            break
        else:
            raise PngError("unsupported PNG chunk")
        offset = end

    if width is None or not seen_idat or not seen_iend:
        raise PngError("PNG is missing required chunks")

    if (
        filtered is None
        or len(filtered) != expected_size
        or not inflater.eof
        or inflater.unconsumed_tail
        or inflater.unused_data
    ):
        raise PngError("PNG decompressed size or stream is invalid")

    rgba = bytearray(width * height * 4)
    previous = bytearray(row_bytes)
    source_offset = 0
    target_offset = 0
    for _ in range(height):
        filter_type = filtered[source_offset]
        source_offset += 1
        row = _unfilter(filter_type, filtered[source_offset:source_offset + row_bytes], previous, bytes_per_pixel)
        source_offset += row_bytes
        if color_type == 6:
            rgba[target_offset:target_offset + row_bytes] = row
            target_offset += row_bytes
        else:
            for x in range(width):
                source = x * 3
                rgba[target_offset:target_offset + 4] = row[source:source + 3] + b"\xff"
                target_offset += 4
        previous = row
    return PngImage(width, height, bytes(rgba))


def _chunk(kind, payload):
    body = kind + payload
    return struct.pack(">I", len(payload)) + body + struct.pack(">I", zlib.crc32(body) & 0xFFFFFFFF)


def encode_png(image):
    """Encode RGBA pixels as a deterministic, non-interlaced PNG."""
    if not isinstance(image, PngImage):
        raise PngError("encoder expects a PngImage")
    width, height, rgba = image
    if type(width) is not int or type(height) is not int:
        raise PngError("image dimensions must be integers")
    if width <= 0 or height <= 0 or width > MAX_DIMENSION or height > MAX_DIMENSION:
        raise PngError("image dimensions are outside supported limits")
    if width * height > MAX_PIXELS:
        raise PngError("image exceeds pixel limit")
    if not isinstance(rgba, (bytes, bytearray)) or len(rgba) != width * height * 4:
        raise PngError("RGBA byte count does not match image dimensions")

    row_bytes = width * 4
    scanlines = bytearray(height * (row_bytes + 1))
    source = memoryview(rgba)
    for y in range(height):
        destination = y * (row_bytes + 1)
        scanlines[destination] = 0
        scanlines[destination + 1:destination + row_bytes + 1] = source[y * row_bytes:(y + 1) * row_bytes]
    try:
        compressed = zlib.compress(bytes(scanlines), level=9)
    except zlib.error as error:
        raise PngError("PNG compression failed") from error
    header = struct.pack(">IIBBBBB", width, height, 8, 6, 0, 0, 0)
    return PNG_SIGNATURE + _chunk(b"IHDR", header) + _chunk(b"IDAT", compressed) + _chunk(b"IEND", b"")

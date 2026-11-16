"""Bounded in-memory conversion for Unity Texture2D payloads to RGBA8."""

from typing import NamedTuple

try:
    from .unity_serialized_file import (
        MAX_TEXTURE2D_IMAGE_BYTES,
        UnitySpriteRegion,
        UnityTexture2DImageData,
    )
except ImportError:  # Support importing when ``tools`` is directly on sys.path.
    from unity_serialized_file import (  # type: ignore[no-redef]
        MAX_TEXTURE2D_IMAGE_BYTES,
        UnitySpriteRegion,
        UnityTexture2DImageData,
    )


MAX_TEXTURE_DIMENSION = 4096
MAX_TEXTURE_PIXELS = 8 * 1024 * 1024
MAX_RGBA_BYTES = MAX_TEXTURE_PIXELS * 4


class UnityTexturePixelsError(ValueError):
    """Raised when a Texture2D payload cannot be safely converted to RGBA8."""


class UnityTexture2DRgba(NamedTuple):
    width: int
    height: int
    pixels: bytes


def _is_plain_int(value: object) -> bool:
    return isinstance(value, int) and not isinstance(value, bool)

# BC7 partition/fix-up masks adapted from bcdec.h v0.985 (Unlicense).
# Each packed nibble stores the subset in bits 0..1 and the anchor flag in bit 3.
_BC7_PARTITION_2 = bytes.fromhex(
    "08110011001100910810001000100090181110111011109108100011001110910800001000100091081110111011119108100011101111910800001000111091"
    "08000000001000910811101111111191080000101011119108000000001010910810101111111191080000001111119108001111111111910800000000001191"
    "08000100110111911819001000000000080000000900110118190011001000000819001000000000080001001900110108000000090011001811001100110090"
    "08190010001000000800010009001100180910011001100108191001100111000810101119010100080011111911000018190010010011010819011001101100"
    "18101010101010900800111100001191181001091010010108110011190011000819110000111100181010100901010118010110100101901810010101011090"
    "18190011110011010810001119000100081900011000110008190111111011001809011001101001081111001100009118011001011001900800100910010000"
    "18001109100000000809101100010000080000091011000108001000190110001801110001100091081110011100019018090011011011000819011011001001"
    "18011100110001901801001100110190181111010100009008100100110110910800111100110091081900111111000008090001110111011800100010111091"
)
_BC7_PARTITION_3 = bytes.fromhex(
    "08910011201222a2089000112a112212080002102a11229128a200220011109108000000192211a208910011002200a208a2002211111191081100112a112291"
    "08000000191122a208001111191122a208001119222222a208210029002100a118211029102110a118229022102210a208911021112221a2089102102a002202"
    "08900011102111a2189100110a10220008001122192211a208a200220022119118911011202220a2089000102a12221208000019102210a2080011002a092201"
    "18a290220011000008210021192222a21801211a2912100108001009211a211208221120192000a218019001022022a208111022102a0091080002002a112292"
    "08000020192221a228a200220021009108910021002220a218029002100a100208001119222a0000180221100a29100218020221a910100208112200112a0091"
    "0811112a2200009118901010222222a2080000001a12129208229122002211a208a20011002200912802211a200221921810222a22221090080012121a121292"
    "18901010101022a228a210112022109108209121002011a108009221122112a128229011101120a208201121192100a018019001100122a208000000122912a1"
    "18019001222222a208220011001900a208221122192200a208000000000092a108a000100020009028222122202229a218902222222222a2189102112a102202"
)
_BC7_MODES = {
    # subsets, partition bits, RGB bits, alpha bits, P-bit kind, index bits, second index bits
    0: (3, 4, 4, 0, 1, 3, 0),
    1: (2, 6, 6, 0, 2, 3, 0),
    3: (2, 6, 7, 0, 1, 2, 0),
    4: (1, 0, 5, 6, 0, 2, 3),
    5: (1, 0, 7, 8, 0, 2, 2),
    6: (1, 0, 7, 7, 1, 4, 0),
}
_BC7_WEIGHTS = {
    2: (0, 21, 43, 64),
    3: (0, 9, 18, 27, 37, 46, 55, 64),
    4: (0, 4, 9, 13, 17, 21, 26, 30, 34, 38, 43, 47, 51, 55, 60, 64),
}

def _bc7_partition_entry(mode: int, partition: int, pixel: int) -> int:
    if mode == 0:
        table = _BC7_PARTITION_3
        partition_count = 16
    elif mode in (1, 3):
        table = _BC7_PARTITION_2
        partition_count = 64
    else:
        return 8 if pixel == 0 else 0
    if not 0 <= partition < partition_count:
        raise UnityTexturePixelsError("BC7 partition index is outside range")
    packed = table[partition * 8 + pixel // 2]
    return (packed >> (4 * (pixel & 1))) & 0x0F

def _bc7_read_indices(
    block_bits: int, bit_offset: int, mode: int, partition: int, width: int
) -> tuple[tuple[int, ...], int]:
    indices = []
    for pixel in range(16):
        entry = _bc7_partition_entry(mode, partition, pixel)
        bit_count = width - (1 if entry & 8 else 0)
        indices.append((block_bits >> bit_offset) & ((1 << bit_count) - 1))
        bit_offset += bit_count
    return tuple(indices), bit_offset

def _bc7_unquantize(value: int, precision: int) -> int:
    value <<= 8 - precision
    return value | (value >> precision)

def _bc7_interpolate(first: int, second: int, index: int, width: int) -> int:
    weight = _BC7_WEIGHTS[width][index]
    return ((64 - weight) * first + weight * second + 32) >> 6

def _decode_bc7_block(block: bytes) -> bytes:
    if not isinstance(block, bytes) or len(block) != 16:
        raise UnityTexturePixelsError("BC7 block must contain 16 bytes")
    mode = next((bit for bit in range(8) if block[0] & (1 << bit)), None)
    if mode is None:
        raise UnityTexturePixelsError("BC7 mode prefix is invalid")
    layout = _BC7_MODES.get(mode)
    if layout is None:
        raise UnityTexturePixelsError("BC7 mode is unsupported")
    subsets, partition_width, rgb_width, alpha_width, pbit_kind, index_width, second_width = layout
    block_bits = int.from_bytes(block, "little")
    bit_offset = mode + 1

    def read(width: int) -> int:
        nonlocal bit_offset
        if width < 0 or bit_offset + width > 128:
            raise UnityTexturePixelsError("BC7 block is truncated")
        value = (block_bits >> bit_offset) & ((1 << width) - 1)
        bit_offset += width
        return value

    partition = read(partition_width)
    rotation = read(2 if mode in (4, 5) else 0)
    index_selection = read(1 if mode == 4 else 0)
    endpoint_count = subsets * 2
    endpoints = [[0, 0, 0, 255] for _ in range(endpoint_count)]
    for channel in range(3):
        for endpoint in range(endpoint_count):
            endpoints[endpoint][channel] = read(rgb_width)
    if alpha_width:
        for endpoint in range(endpoint_count):
            endpoints[endpoint][3] = read(alpha_width)

    if pbit_kind == 1:
        for endpoint in range(endpoint_count):
            pbit = read(1)
            for channel in range(4 if alpha_width else 3):
                endpoints[endpoint][channel] = (endpoints[endpoint][channel] << 1) | pbit
    elif pbit_kind == 2:
        for subset in range(subsets):
            pbit = read(1)
            for endpoint in (subset * 2, subset * 2 + 1):
                for channel in range(3):
                    endpoints[endpoint][channel] = (endpoints[endpoint][channel] << 1) | pbit

    rgb_precision = rgb_width + (1 if pbit_kind else 0)
    alpha_precision = alpha_width + (1 if pbit_kind else 0)
    for endpoint in endpoints:
        for channel in range(3):
            endpoint[channel] = _bc7_unquantize(endpoint[channel], rgb_precision)
        if alpha_width:
            endpoint[3] = _bc7_unquantize(endpoint[3], alpha_precision)

    primary, bit_offset = _bc7_read_indices(
        block_bits, bit_offset, mode, partition, index_width
    )
    secondary = ()
    if second_width:
        secondary, bit_offset = _bc7_read_indices(
            block_bits, bit_offset, mode, partition, second_width
        )
    if bit_offset != 128:
        raise UnityTexturePixelsError("BC7 block layout is invalid")

    pixels = bytearray(64)
    for pixel in range(16):
        entry = _bc7_partition_entry(mode, partition, pixel)
        subset = entry & 3
        if subset >= subsets:
            raise UnityTexturePixelsError("BC7 subset index is invalid")
        first = endpoints[subset * 2]
        second = endpoints[subset * 2 + 1]
        if mode == 4:
            if index_selection:
                color_index, alpha_index = secondary[pixel], primary[pixel]
                color_index_width, alpha_index_width = 3, 2
            else:
                color_index, alpha_index = primary[pixel], secondary[pixel]
                color_index_width, alpha_index_width = 2, 3
        elif mode == 5:
            color_index, alpha_index = primary[pixel], secondary[pixel]
            color_index_width = alpha_index_width = 2
        else:
            color_index = alpha_index = primary[pixel]
            color_index_width = alpha_index_width = index_width
        rgba = [
            _bc7_interpolate(first[channel], second[channel], color_index, color_index_width)
            for channel in range(3)
        ]
        rgba.append(
            _bc7_interpolate(first[3], second[3], alpha_index, alpha_index_width)
        )
        if rotation:
            channel = rotation - 1
            rgba[channel], rgba[3] = rgba[3], rgba[channel]
        pixels[pixel * 4 : pixel * 4 + 4] = bytes(rgba)
    return bytes(pixels)

def _decode_bc7_image(image_data: bytes, width: int, height: int) -> bytes:
    block_columns = (width + 3) // 4
    block_rows = (height + 3) // 4
    output = bytearray(width * height * 4)
    input_offset = 0
    for block_y in range(block_rows):
        for block_x in range(block_columns):
            decoded = _decode_bc7_block(image_data[input_offset : input_offset + 16])
            input_offset += 16
            for pixel_y in range(min(4, height - block_y * 4)):
                for pixel_x in range(min(4, width - block_x * 4)):
                    source_offset = (pixel_y * 4 + pixel_x) * 4
                    target_offset = ((block_y * 4 + pixel_y) * width + block_x * 4 + pixel_x) * 4
                    output[target_offset : target_offset + 4] = decoded[
                        source_offset : source_offset + 4
                    ]
    return bytes(output)

def _decode_bc3_block(block: bytes) -> bytes:
    if not isinstance(block, bytes) or len(block) != 16:
        raise UnityTexturePixelsError("BC3 block must contain 16 bytes")

    alpha0, alpha1 = block[0], block[1]
    if alpha0 > alpha1:
        alpha_palette = [alpha0, alpha1] + [
            ((7 - index) * alpha0 + index * alpha1) // 7
            for index in range(1, 7)
        ]
    else:
        alpha_palette = [alpha0, alpha1] + [
            ((5 - index) * alpha0 + index * alpha1) // 5
            for index in range(1, 5)
        ] + [0, 255]
    alpha_indices = int.from_bytes(block[2:8], "little")

    color0 = int.from_bytes(block[8:10], "little")
    color1 = int.from_bytes(block[10:12], "little")

    def expand_rgb565(color: int) -> tuple[int, int, int]:
        red = (color >> 11) & 0x1F
        green = (color >> 5) & 0x3F
        blue = color & 0x1F
        return (
            (red << 3) | (red >> 2),
            (green << 2) | (green >> 4),
            (blue << 3) | (blue >> 2),
        )

    first = expand_rgb565(color0)
    second = expand_rgb565(color1)
    colors = (
        first,
        second,
        tuple((2 * first[channel] + second[channel]) // 3 for channel in range(3)),
        tuple((first[channel] + 2 * second[channel]) // 3 for channel in range(3)),
    )
    color_indices = int.from_bytes(block[12:16], "little")
    output = bytearray(4 * 4 * 4)
    for pixel in range(16):
        color_index = (color_indices >> (pixel * 2)) & 0x03
        alpha_index = (alpha_indices >> (pixel * 3)) & 0x07
        offset = pixel * 4
        output[offset : offset + 4] = bytes((*colors[color_index], alpha_palette[alpha_index]))
    return bytes(output)


def _decode_bc3_image(image_data: bytes, width: int, height: int) -> bytes:
    block_columns = (width + 3) // 4
    block_rows = (height + 3) // 4
    output = bytearray(width * height * 4)
    input_offset = 0
    for block_y in range(block_rows):
        for block_x in range(block_columns):
            decoded = _decode_bc3_block(image_data[input_offset : input_offset + 16])
            input_offset += 16
            for pixel_y in range(min(4, height - block_y * 4)):
                for pixel_x in range(min(4, width - block_x * 4)):
                    source_offset = (pixel_y * 4 + pixel_x) * 4
                    target_offset = ((block_y * 4 + pixel_y) * width + block_x * 4 + pixel_x) * 4
                    output[target_offset : target_offset + 4] = decoded[
                        source_offset : source_offset + 4
                    ]
    return bytes(output)


def _expected_block_compressed_size(width: int, height: int, mip_count: int) -> int:
    maximum_mips = max(width, height).bit_length()
    if mip_count < 1 or mip_count > min(maximum_mips, 32):
        raise UnityTexturePixelsError("Texture2D mip count is outside range")
    total = 0
    for _ in range(mip_count):
        level_size = ((width + 3) // 4) * ((height + 3) // 4) * 16
        if level_size > MAX_TEXTURE2D_IMAGE_BYTES - total:
            raise UnityTexturePixelsError("Texture2D image exceeds byte limit")
        total += level_size
        width = max(1, width // 2)
        height = max(1, height // 2)
    return total


def _expected_uncompressed_size(
    width: int, height: int, bytes_per_pixel: int, mip_count: int
) -> int:
    if bytes_per_pixel == 0:
        return _expected_block_compressed_size(width, height, mip_count)
    maximum_mips = max(width, height).bit_length()
    if mip_count < 1 or mip_count > min(maximum_mips, 32):
        raise UnityTexturePixelsError("Texture2D mip count is outside supported range")

    total = 0
    mip_width = width
    mip_height = height
    for _ in range(mip_count):
        level_size = mip_width * mip_height * bytes_per_pixel
        if level_size > MAX_TEXTURE2D_IMAGE_BYTES - total:
            raise UnityTexturePixelsError("Texture2D image exceeds supported byte limit")
        total += level_size
        mip_width = max(1, mip_width // 2)
        mip_height = max(1, mip_height // 2)
    return total


def decode_texture2d_to_rgba(
    image: UnityTexture2DImageData,
    *,
    max_pixels: int = MAX_TEXTURE_PIXELS,
    max_output_bytes: int = MAX_RGBA_BYTES,
) -> UnityTexture2DRgba:
    """Convert the base mip of inline Alpha8, RGB24, or RGBA32 data to RGBA8.

    The input is the immutable result of ``read_texture2d_image_data``. The
    complete encoded payload is checked, including its mip tail, but only the
    base level is converted. Pixel order is preserved exactly as stored; this
    function does not flip rows, write files, or resolve external streams.
    """
    if not isinstance(image, UnityTexture2DImageData):
        raise UnityTexturePixelsError("Texture2D image data result is required")
    if not _is_plain_int(max_pixels) or not 1 <= max_pixels <= MAX_TEXTURE_PIXELS:
        raise UnityTexturePixelsError("pixel limit is outside supported range")
    if (
        not _is_plain_int(max_output_bytes)
        or max_output_bytes > MAX_RGBA_BYTES
        or max_output_bytes < 4
    ):
        raise UnityTexturePixelsError("output byte limit is outside supported range")

    if image.storage != "inline" or not isinstance(image.image_data, bytes):
        raise UnityTexturePixelsError("Texture2D payload must be inline")

    values = (image.width, image.height, image.complete_image_size, image.texture_format, image.mip_count)
    if any(not _is_plain_int(value) for value in values):
        raise UnityTexturePixelsError("Texture2D metadata contains invalid integer fields")
    width, height = image.width, image.height
    if not 1 <= width <= MAX_TEXTURE_DIMENSION or not 1 <= height <= MAX_TEXTURE_DIMENSION:
        raise UnityTexturePixelsError("Texture2D dimensions are outside supported range")

    bytes_per_pixel = {1: 1, 3: 3, 4: 4, 12: 0, 25: 0}.get(image.texture_format)
    if bytes_per_pixel is None:
        raise UnityTexturePixelsError("Texture2D pixel format is unsupported")

    if width > max_pixels // height:
        raise UnityTexturePixelsError("Texture2D pixel count exceeds configured limit")
    pixel_count = width * height
    output_size = pixel_count * 4
    if output_size > max_output_bytes:
        raise UnityTexturePixelsError("Texture2D RGBA output exceeds configured byte limit")

    expected_size = _expected_uncompressed_size(
        width, height, bytes_per_pixel, image.mip_count
    )
    if (
        image.complete_image_size != expected_size
        or len(image.image_data) != expected_size
    ):
        raise UnityTexturePixelsError("Texture2D payload size does not match its metadata")

    source = image.image_data
    if image.texture_format == 12:
        return UnityTexture2DRgba(width, height, _decode_bc3_image(source, width, height))
    if image.texture_format == 25:
        return UnityTexture2DRgba(width, height, _decode_bc7_image(source, width, height))
    base_level_size = pixel_count * bytes_per_pixel
    if image.texture_format == 4:
        pixels = source[:output_size]
    else:
        pixels_buffer = bytearray(output_size)
        if image.texture_format == 1:
            for index, alpha in enumerate(source[:base_level_size]):
                destination = index * 4
                pixels_buffer[destination] = 255
                pixels_buffer[destination + 1] = 255
                pixels_buffer[destination + 2] = 255
                pixels_buffer[destination + 3] = alpha
        else:  # RGB24
            for index in range(pixel_count):
                source_offset = index * 3
                destination = index * 4
                pixels_buffer[destination] = source[source_offset]
                pixels_buffer[destination + 1] = source[source_offset + 1]
                pixels_buffer[destination + 2] = source[source_offset + 2]
                pixels_buffer[destination + 3] = 255
        pixels = bytes(pixels_buffer)

    return UnityTexture2DRgba(width, height, pixels)


def decode_loaded_texture2d_to_rgba(
    image: UnityTexture2DImageData,
    *,
    max_pixels: int = MAX_TEXTURE_PIXELS,
    max_output_bytes: int = MAX_RGBA_BYTES,
) -> UnityTexture2DRgba:
    """Convert bytes loaded by the bounded external-stream reader to RGBA8."""
    if not isinstance(image, UnityTexture2DImageData) or image.storage != "loaded":
        raise UnityTexturePixelsError("loaded Texture2D image data result is required")
    return decode_texture2d_to_rgba(
        image._replace(storage="inline"),
        max_pixels=max_pixels,
        max_output_bytes=max_output_bytes,
    )


def crop_sprite_rgba(
    image: UnityTexture2DRgba,
    region: UnitySpriteRegion,
    *,
    source_origin: str,
    max_pixels: int = MAX_TEXTURE_PIXELS,
    max_output_bytes: int = MAX_RGBA_BYTES,
) -> UnityTexture2DRgba:
    """Crop a Unity bottom-left Sprite region into PNG top-left row order."""
    if not isinstance(image, UnityTexture2DRgba):
        raise UnityTexturePixelsError("RGBA texture result is required")
    if not isinstance(region, UnitySpriteRegion):
        raise UnityTexturePixelsError("Sprite region result is required")
    if source_origin not in ("top-left", "bottom-left"):
        raise UnityTexturePixelsError("source row origin is unsupported")
    if (
        not _is_plain_int(max_pixels)
        or not 1 <= max_pixels <= MAX_TEXTURE_PIXELS
        or not _is_plain_int(max_output_bytes)
        or not 4 <= max_output_bytes <= MAX_RGBA_BYTES
    ):
        raise UnityTexturePixelsError("crop output limits are invalid")

    width, height = image.width, image.height
    if (
        not _is_plain_int(width)
        or not 1 <= width <= MAX_TEXTURE_DIMENSION
        or not _is_plain_int(height)
        or not 1 <= height <= MAX_TEXTURE_DIMENSION
    ):
        raise UnityTexturePixelsError("RGBA texture dimensions are invalid")
    source_pixels = width * height
    if source_pixels > max_pixels or source_pixels * 4 > MAX_RGBA_BYTES:
        raise UnityTexturePixelsError("RGBA texture exceeds supported limits")
    if not isinstance(image.pixels, bytes) or len(image.pixels) != source_pixels * 4:
        raise UnityTexturePixelsError("RGBA texture buffer size is inconsistent")

    x, y, crop_width, crop_height = region
    if any(not _is_plain_int(value) for value in region):
        raise UnityTexturePixelsError("Sprite region coordinates are invalid")
    if (
        x < 0
        or y < 0
        or crop_width <= 0
        or crop_height <= 0
        or x + crop_width > width
        or y + crop_height > height
    ):
        raise UnityTexturePixelsError("Sprite region is outside the RGBA texture")

    output_pixels = crop_width * crop_height
    output_bytes = output_pixels * 4
    if output_pixels > max_pixels or output_bytes > max_output_bytes:
        raise UnityTexturePixelsError("Sprite crop exceeds configured output limit")

    pixels = bytearray(output_bytes)
    for output_y in range(crop_height):
        if source_origin == "top-left":
            source_y = height - y - crop_height + output_y
        else:
            source_y = y + crop_height - 1 - output_y
        source_start = (source_y * width + x) * 4
        output_start = output_y * crop_width * 4
        row_bytes = crop_width * 4
        pixels[output_start : output_start + row_bytes] = image.pixels[
            source_start : source_start + row_bytes
        ]
    return UnityTexture2DRgba(crop_width, crop_height, bytes(pixels))

import sys
import unittest
from pathlib import Path

REPOSITORY_ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(REPOSITORY_ROOT / "tools"))

from unity_serialized_file import UnitySpriteRegion, UnityTexture2DImageData
from unity_texture_pixels import (
    MAX_RGBA_BYTES,
    UnityTexture2DRgba,
    UnityTexturePixelsError,
    crop_sprite_rgba,
    decode_loaded_texture2d_to_rgba,
    decode_texture2d_to_rgba,
)


class UnityTexturePixelsBc7Tests(unittest.TestCase):
    def test_all_supported_bc7_modes_decode_constant_endpoints(self):
        for mode in (0, 1, 3, 4, 5, 6):
            with self.subTest(mode=mode):
                image = texture(25, 4, 4, _bc7_test_block(mode))
                decoded = decode_texture2d_to_rgba(image)
                self.assertEqual(decoded.pixels, bytes((255, 255, 255, 255)) * 16)

    def test_multisubset_partitions_select_distinct_endpoints(self):
        mode1_endpoints = [
            (0, 0, 0), (0, 0, 0),
            (63, 0, 0), (63, 0, 0),
        ]
        mode1 = _bc7_test_block(1, rgb_endpoints=mode1_endpoints, pbits=(0, 1))
        pixels = decode_texture2d_to_rgba(texture(25, 4, 4, mode1)).pixels
        self.assertEqual(pixels[:4], bytes((0, 0, 0, 255)))
        self.assertEqual(pixels[4:8], bytes((0, 0, 0, 255)))
        self.assertEqual(pixels[8:12], bytes((255, 2, 2, 255)))

        mode0_endpoints = [
            (0, 0, 0), (0, 0, 0),
            (15, 0, 0), (15, 0, 0),
            (7, 0, 0), (7, 0, 0),
        ]
        mode0 = _bc7_test_block(0, rgb_endpoints=mode0_endpoints, pbits=(0, 0, 1, 1, 1, 1))
        pixels = decode_texture2d_to_rgba(texture(25, 4, 4, mode0)).pixels
        self.assertEqual(pixels[2 * 4 : 3 * 4], bytes((255, 8, 8, 255)))
        self.assertEqual(pixels[9 * 4 : 10 * 4], bytes((123, 8, 8, 255)))

    def test_mode4_index_selector_and_rotation(self):
        endpoints = [(0, 0, 0), (31, 31, 31)]
        alpha = (0, 63)
        for selector, expected in (
            (0, bytes((255, 84, 84, 84))),
            (1, bytes((84, 255, 255, 255))),
        ):
            block = _bc7_test_block(
                4,
                rgb_endpoints=endpoints,
                alpha_endpoints=alpha,
                rotation=1,
                index_selection=selector,
                primary_indices={1: 1},
                secondary_indices={1: 7},
            )
            image = texture(25, 4, 4, block)
            decoded = decode_texture2d_to_rgba(image)
            self.assertEqual(decoded.pixels[4:8], expected)

    def test_mode5_separate_indices_and_alpha_rotation(self):
        block = _bc7_test_block(
            5,
            rgb_endpoints=[(0, 0, 0), (127, 127, 127)],
            alpha_endpoints=(0, 255),
            rotation=2,
            primary_indices={1: 1},
            secondary_indices={1: 3},
        )
        pixels = decode_texture2d_to_rgba(texture(25, 4, 4, block)).pixels
        self.assertEqual(pixels[4:8], bytes((84, 255, 84, 84)))

    def test_mode6_interpolation_and_loaded_stream(self):
        block = _bc7_test_block(
            6,
            rgb_endpoints=[(0, 0, 0), (127, 127, 127)],
            alpha_endpoints=(0, 127),
            pbits=(0, 1),
            primary_indices={1: 15},
        )
        decoded = decode_loaded_texture2d_to_rgba(
            texture(25, 4, 4, block, storage="loaded")
        )
        self.assertEqual(decoded.pixels[4:8], bytes((255, 255, 255, 255)))

    def test_bc7_edges_and_validated_mip_tail(self):
        block = _bc7_test_block(6)
        decoded = decode_texture2d_to_rgba(texture(25, 5, 3, block + block))
        self.assertEqual((decoded.width, decoded.height), (5, 3))
        self.assertEqual(len(decoded.pixels), 5 * 3 * 4)
        self.assertEqual(decoded.pixels, bytes((255, 255, 255, 255)) * 15)

        mip_chain = block + _bc7_test_block(5)
        mip = decode_texture2d_to_rgba(
            texture(25, 4, 4, mip_chain, mip_count=2, complete_image_size=32)
        )
        self.assertEqual(mip.pixels, bytes((255, 255, 255, 255)) * 16)

    def test_invalid_modes_prefixes_payloads_and_limits_are_rejected(self):
        for block, message in (
            (bytes(16), "mode prefix is invalid"),
            (bytes((1 << 2,)) + bytes(15), "mode is unsupported"),
            (bytes((1 << 7,)) + bytes(15), "mode is unsupported"),
        ):
            with self.subTest(message=message), self.assertRaisesRegex(
                UnityTexturePixelsError, message
            ):
                decode_texture2d_to_rgba(texture(25, 4, 4, block))

        block = _bc7_test_block(6)
        for image in (
            texture(25, 4, 4, block[:-1], complete_image_size=16),
            texture(25, 4, 4, block, complete_image_size=32),
        ):
            with self.assertRaisesRegex(UnityTexturePixelsError, "payload size"):
                decode_texture2d_to_rgba(image)
        with self.assertRaisesRegex(UnityTexturePixelsError, "pixel count"):
            decode_texture2d_to_rgba(texture(25, 4, 4, block), max_pixels=15)
        with self.assertRaisesRegex(UnityTexturePixelsError, "output exceeds configured byte limit"):
            decode_texture2d_to_rgba(texture(25, 4, 4, block), max_output_bytes=63)


def _bc7_test_block(
    mode,
    *,
    partition=0,
    rgb_endpoints=None,
    alpha_endpoints=None,
    pbits=None,
    rotation=0,
    index_selection=0,
    primary_indices=None,
    secondary_indices=None,
):
    layouts = {
        0: (3, 4, 4, 0, 1, 3, 0),
        1: (2, 6, 6, 0, 2, 3, 0),
        3: (2, 6, 7, 0, 1, 2, 0),
        4: (1, 0, 5, 6, 0, 2, 3),
        5: (1, 0, 7, 8, 0, 2, 2),
        6: (1, 0, 7, 7, 1, 4, 0),
    }
    subsets, partition_width, rgb_width, alpha_width, pbit_kind, index_width, second_width = layouts[mode]
    endpoint_count = subsets * 2
    if rgb_endpoints is None:
        maximum = (1 << rgb_width) - 1
        rgb_endpoints = [(maximum, maximum, maximum)] * endpoint_count
    if alpha_endpoints is None:
        maximum = (1 << alpha_width) - 1 if alpha_width else 0
        alpha_endpoints = [maximum] * endpoint_count
    if pbits is None:
        pbits = [1] * (endpoint_count if pbit_kind == 1 else subsets if pbit_kind == 2 else 0)
    primary_indices = primary_indices or {}
    secondary_indices = secondary_indices or {}

    value = 0
    offset = 0

    def write(bits, count):
        nonlocal value, offset
        if count and not 0 <= bits < (1 << count):
            raise AssertionError("fixture field is outside bit range")
        value |= bits << offset
        offset += count

    write(1 << mode, mode + 1)
    write(partition, partition_width)
    write(rotation, 2 if mode in (4, 5) else 0)
    write(index_selection, 1 if mode == 4 else 0)
    for channel in range(3):
        for endpoint in range(endpoint_count):
            write(rgb_endpoints[endpoint][channel], rgb_width)
    if alpha_width:
        for endpoint in range(endpoint_count):
            write(alpha_endpoints[endpoint], alpha_width)
    if pbit_kind == 1:
        for pbit in pbits:
            write(pbit, 1)
    elif pbit_kind == 2:
        for pbit in pbits:
            write(pbit, 1)

    anchor_pixels = {0: {0, 3, 15}, 1: {0, 15}, 3: {0, 15}}
    anchors = anchor_pixels.get(mode, {0})
    for pixel in range(16):
        write(primary_indices.get(pixel, 0), index_width - (pixel in anchors))
    if second_width:
        for pixel in range(16):
            write(secondary_indices.get(pixel, 0), second_width - (pixel == 0))
    if offset != 128:
        raise AssertionError(f"fixture wrote {offset} bits instead of 128")
    return value.to_bytes(16, "little")


def texture(
    texture_format,
    width,
    height,
    image_data,
    *,
    mip_count=1,
    storage="inline",
    complete_image_size=None,
):
    if complete_image_size is None:
        complete_image_size = len(image_data) if image_data is not None else 0
    return UnityTexture2DImageData(
        width,
        height,
        complete_image_size,
        texture_format,
        mip_count,
        storage,
        image_data,
    )


class UnityTexturePixelsTests(unittest.TestCase):
    def test_alpha8_expands_to_white_rgb_with_source_alpha(self):
        image = texture(1, 2, 1, bytes((0, 128)))

        decoded = decode_texture2d_to_rgba(image)

        self.assertEqual((decoded.width, decoded.height), (2, 1))
        self.assertEqual(
            decoded.pixels,
            bytes((255, 255, 255, 0, 255, 255, 255, 128)),
        )

    def test_alpha8_ignores_only_a_validated_mip_tail(self):
        image = texture(1, 2, 2, bytes((1, 2, 3, 4, 99)), mip_count=2)

        decoded = decode_texture2d_to_rgba(image)

        self.assertEqual(
            decoded.pixels,
            bytes((255, 255, 255, 1, 255, 255, 255, 2,
                   255, 255, 255, 3, 255, 255, 255, 4)),
        )

    def test_rgb24_adds_opaque_alpha_without_reordering(self):
        image = texture(3, 2, 1, bytes((1, 2, 3, 10, 20, 30)))

        decoded = decode_texture2d_to_rgba(image)

        self.assertEqual(
            decoded.pixels,
            bytes((1, 2, 3, 255, 10, 20, 30, 255)),
        )

    def test_rgba32_preserves_base_level_bytes_and_skips_mip_tail(self):
        base = bytes(range(16))
        image = texture(4, 2, 2, base + b"mip!", mip_count=2)

        decoded = decode_texture2d_to_rgba(image)

        self.assertEqual(decoded.pixels, base)
        self.assertIsInstance(decoded.pixels, bytes)

    def test_external_empty_and_missing_payloads_are_rejected(self):
        cases = (
            texture(1, 1, 1, None, storage="external", complete_image_size=1),
            texture(4, 0, 0, None, storage="empty", complete_image_size=0),
            texture(3, 1, 1, None, storage="inline", complete_image_size=3),
        )
        for image in cases:
            with self.subTest(storage=image.storage):
                with self.assertRaises(UnityTexturePixelsError):
                    decode_texture2d_to_rgba(image)

    def test_invalid_bc7_prefix_and_other_unknown_formats_are_rejected(self):
        with self.assertRaisesRegex(UnityTexturePixelsError, "mode prefix is invalid"):
            decode_texture2d_to_rgba(texture(25, 4, 4, bytes(16)))
        with self.assertRaises(UnityTexturePixelsError):
            decode_texture2d_to_rgba(texture(99, 1, 1, b""))

    def test_truncated_and_inconsistent_payloads_are_rejected(self):
        cases = (
            texture(3, 2, 1, b"12345", complete_image_size=6),
            texture(4, 1, 1, b"12345", complete_image_size=4),
            texture(1, 2, 2, b"1234", mip_count=2, complete_image_size=5),
        )
        for image in cases:
            with self.subTest(data_len=len(image.image_data)):
                with self.assertRaisesRegex(UnityTexturePixelsError, "payload size"):
                    decode_texture2d_to_rgba(image)

    def test_invalid_dimensions_and_mip_counts_are_rejected(self):
        cases = (
            texture(1, 0, 1, b""),
            texture(1, 1, 0, b""),
            texture(1, 4097, 1, b""),
            texture(1, 10**100, 1, b""),
            texture(1, 1, 1, b"x", mip_count=0),
            UnityTexture2DImageData(True, 1, 1, 1, 1, "inline", b"x"),
        )
        for image in cases:
            with self.subTest(width=image.width, height=image.height):
                with self.assertRaises(UnityTexturePixelsError):
                    decode_texture2d_to_rgba(image)

    def test_pixel_and_output_limits_are_checked_before_conversion(self):
        image = texture(3, 2, 2, bytes(range(12)))
        with self.assertRaisesRegex(UnityTexturePixelsError, "pixel count"):
            decode_texture2d_to_rgba(image, max_pixels=3)
        with self.assertRaisesRegex(UnityTexturePixelsError, "byte limit"):
            decode_texture2d_to_rgba(image, max_output_bytes=15)
        with self.assertRaises(UnityTexturePixelsError):
            decode_texture2d_to_rgba(image, max_output_bytes=MAX_RGBA_BYTES + 1)
        with self.assertRaises(UnityTexturePixelsError):
            decode_texture2d_to_rgba(image, max_output_bytes=3)
        with self.assertRaises(UnityTexturePixelsError):
            decode_texture2d_to_rgba(image, max_pixels=True)

    def test_requires_parser_result_type(self):
        with self.assertRaisesRegex(UnityTexturePixelsError, "result is required"):
            decode_texture2d_to_rgba((1, 1, 1, 1, 1, "inline", b"x"))


class UnityTexturePixelsSpriteCropTests(unittest.TestCase):
    @staticmethod
    def texture():
        top = ((255, 0, 0, 1), (0, 255, 0, 2), (0, 0, 255, 3))
        bottom = ((255, 255, 0, 4), (255, 0, 255, 5), (0, 255, 255, 6))
        pixels = bytes(channel for row in (top, bottom) for pixel in row for channel in pixel)
        return UnityTexture2DRgba(3, 2, pixels)

    def test_crops_unity_region_into_png_order_for_both_source_origins(self):
        image = self.texture()
        region = UnitySpriteRegion(1, 0, 2, 2)
        green, blue = (0, 255, 0, 2), (0, 0, 255, 3)
        magenta, cyan = (255, 0, 255, 5), (0, 255, 255, 6)

        top_left = crop_sprite_rgba(image, region, source_origin="top-left")
        bottom_left = crop_sprite_rgba(image, region, source_origin="bottom-left")

        self.assertEqual((top_left.width, top_left.height), (2, 2))
        self.assertEqual(
            top_left.pixels,
            bytes(channel for pixel in (green, blue, magenta, cyan) for channel in pixel),
        )
        self.assertEqual(
            bottom_left.pixels,
            bytes(channel for pixel in (magenta, cyan, green, blue) for channel in pixel),
        )

    def test_rejects_invalid_crop_inputs_and_limits(self):
        image = self.texture()
        for region in (
            UnitySpriteRegion(-1, 0, 1, 1),
            UnitySpriteRegion(2, 0, 2, 1),
            UnitySpriteRegion(0, 1, 1, 2),
            UnitySpriteRegion(0, 0, 0, 1),
        ):
            with self.subTest(region=region):
                with self.assertRaises(UnityTexturePixelsError):
                    crop_sprite_rgba(image, region, source_origin="top-left")

        with self.assertRaises(UnityTexturePixelsError):
            crop_sprite_rgba(image, UnitySpriteRegion(0, 0, 1, 1), source_origin="left")
        with self.assertRaises(UnityTexturePixelsError):
            crop_sprite_rgba(image, (0, 0, 1, 1), source_origin="top-left")
        with self.assertRaises(UnityTexturePixelsError):
            crop_sprite_rgba(
                UnityTexture2DRgba(3, 2, b"short"),
                UnitySpriteRegion(0, 0, 1, 1),
                source_origin="top-left",
            )
        with self.assertRaises(UnityTexturePixelsError):
            crop_sprite_rgba(image, UnitySpriteRegion(0, 0, 2, 2), source_origin="top-left", max_pixels=3)
        with self.assertRaises(UnityTexturePixelsError):
            crop_sprite_rgba(
                image,
                UnitySpriteRegion(0, 0, 2, 2),
                source_origin="top-left",
                max_output_bytes=15,
            )
        with self.assertRaises(UnityTexturePixelsError):
            crop_sprite_rgba(image, UnitySpriteRegion(0, 0, 1, 1), source_origin="top-left", max_pixels=5)


def _bc3_test_block(
    *,
    alpha0=255,
    alpha1=255,
    alpha_indices=0,
    color0=0xF800,
    color1=0x07E0,
    color_indices=0,
):
    return (
        bytes((alpha0, alpha1))
        + alpha_indices.to_bytes(6, "little")
        + color0.to_bytes(2, "little")
        + color1.to_bytes(2, "little")
        + color_indices.to_bytes(4, "little")
    )


class UnityTexturePixelsBc3Tests(unittest.TestCase):
    def test_dxt5_decodes_color_and_both_alpha_palette_modes(self):
        alpha_indices = (0 << 0) | (1 << 3) | (2 << 6)
        block = _bc3_test_block(
            alpha0=0,
            alpha1=255,
            alpha_indices=alpha_indices,
            color_indices=(1 << 2) | (2 << 4),
        )
        pixels = decode_texture2d_to_rgba(texture(12, 4, 4, block)).pixels
        self.assertEqual(pixels[0:4], bytes((255, 0, 0, 0)))
        self.assertEqual(pixels[4:8], bytes((0, 255, 0, 255)))
        self.assertEqual(pixels[8:12], bytes((170, 85, 0, 51)))

        alpha_indices = (2 << 0) | (7 << 3)
        block = _bc3_test_block(alpha0=200, alpha1=100, alpha_indices=alpha_indices)
        pixels = decode_texture2d_to_rgba(texture(12, 4, 4, block)).pixels
        self.assertEqual(pixels[0:4], bytes((255, 0, 0, 185)))
        self.assertEqual(pixels[4:8], bytes((255, 0, 0, 114)))

    def test_dxt5_edges_and_mip_tail_are_bounded(self):
        block = _bc3_test_block(alpha0=255, alpha1=0, color0=0x001F)
        image = texture(12, 5, 3, block * 3, mip_count=2, complete_image_size=48)
        decoded = decode_texture2d_to_rgba(image)
        self.assertEqual((decoded.width, decoded.height), (5, 3))
        self.assertEqual(decoded.pixels, bytes((0, 0, 255, 255)) * 15)

    def test_dxt5_rejects_truncation_mismatch_and_output_limits(self):
        block = _bc3_test_block()
        invalid = (
            texture(12, 4, 4, block[:-1], complete_image_size=16),
            texture(12, 4, 4, block, complete_image_size=32),
        )
        for image in invalid:
            with self.subTest(size=len(image.image_data)):
                with self.assertRaisesRegex(UnityTexturePixelsError, "payload size"):
                    decode_texture2d_to_rgba(image)
        with self.assertRaisesRegex(UnityTexturePixelsError, "pixel count"):
            decode_texture2d_to_rgba(texture(12, 4, 4, block), max_pixels=15)
        with self.assertRaisesRegex(UnityTexturePixelsError, "output"):
            decode_texture2d_to_rgba(texture(12, 4, 4, block), max_output_bytes=63)


if __name__ == "__main__":
    unittest.main()

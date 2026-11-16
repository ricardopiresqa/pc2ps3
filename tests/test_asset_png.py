import struct
import unittest
import zlib

from tools.asset_png import PngError, PngImage, decode_png, encode_png


_PNG_SIGNATURE = b"\x89PNG\r\n\x1a\n"


def _chunk(kind, data):
    body = kind + data
    return struct.pack(">I", len(data)) + body + struct.pack(">I", zlib.crc32(body))


def _paeth(left, above, upper_left):
    estimate = left + above - upper_left
    distances = (abs(estimate - left), abs(estimate - above), abs(estimate - upper_left))
    if distances[0] <= distances[1] and distances[0] <= distances[2]:
        return left
    if distances[1] <= distances[2]:
        return above
    return upper_left


def _make_png(width, rows, color_type=2, filters=None, bit_depth=8, interlace=0):
    bytes_per_pixel = {0: 1, 2: 3, 6: 4}[color_type]
    filters = filters or [0] * len(rows)
    encoded_rows = bytearray()
    previous = bytes(width * bytes_per_pixel)
    for row, filter_type in zip(rows, filters):
        encoded_rows.append(filter_type)
        for index, value in enumerate(row):
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
            encoded_rows.append((value - predictor) & 0xFF)
        previous = row
    height = len(rows)
    header = struct.pack(">IIBBBBB", width, height, bit_depth, color_type, 0, 0, interlace)
    return (
        _PNG_SIGNATURE
        + _chunk(b"IHDR", header)
        + _chunk(b"IDAT", zlib.compress(bytes(encoded_rows)))
        + _chunk(b"IEND", b"")
    )


class AssetPngTests(unittest.TestCase):
    def test_decodes_rgb_rows_using_every_png_filter(self):
        rows = [
            bytes((1 + row * 13 + column for column in range(9)))
            for row in range(5)
        ]
        decoded = decode_png(_make_png(3, rows, filters=[0, 1, 2, 3, 4]))
        expected = bytes(channel for row in rows for pixel in range(3) for channel in row[pixel * 3:pixel * 3 + 3] + b"\xff")
        self.assertEqual((decoded.width, decoded.height, decoded.rgba), (3, 5, expected))

    def test_rgba_round_trip_preserves_alpha_and_encoding_is_repeatable(self):
        image = PngImage(2, 1, bytes((10, 20, 30, 0, 40, 50, 60, 127)))
        first = encode_png(image)
        self.assertEqual(first, encode_png(image))
        self.assertEqual(decode_png(first), image)

    def test_decodes_contiguous_split_idat_chunks(self):
        header = struct.pack(">IIBBBBB", 1, 1, 8, 2, 0, 0, 0)
        compressed = zlib.compress(b"\x00\x07\x08\x09")
        split = len(compressed) // 2
        data = (
            _PNG_SIGNATURE
            + _chunk(b"IHDR", header)
            + _chunk(b"IDAT", compressed[:split])
            + _chunk(b"IDAT", compressed[split:])
            + _chunk(b"IEND", b"")
        )
        self.assertEqual(decode_png(data), PngImage(1, 1, bytes((7, 8, 9, 255))))

    def test_rejects_bad_crc_and_truncated_or_trailing_data(self):
        image = _make_png(1, [bytes((1, 2, 3))])
        bad_crc = bytearray(image)
        bad_crc[31] ^= 0x01
        for malformed in (bytes(bad_crc), image[:-1], image + b"x"):
            with self.subTest(length=len(malformed)):
                with self.assertRaises(PngError):
                    decode_png(malformed)

    def test_rejects_unsupported_color_depth_and_interlace(self):
        cases = (
            _make_png(1, [b"\x7f"], color_type=0),
            _make_png(1, [bytes((1, 2, 3))], bit_depth=16),
            _make_png(1, [bytes((1, 2, 3))], interlace=1),
        )
        for malformed in cases:
            with self.subTest(header=malformed[16:29]):
                with self.assertRaises(PngError):
                    decode_png(malformed)

    def test_rejects_invalid_chunk_order_dimensions_and_duplicate_header(self):
        valid_header = struct.pack(">IIBBBBB", 1, 1, 8, 2, 0, 0, 0)
        zero_width_header = struct.pack(">IIBBBBB", 0, 1, 8, 2, 0, 0, 0)
        compressed = zlib.compress(b"\x00\x01\x02\x03")
        cases = (
            _PNG_SIGNATURE
            + _chunk(b"IDAT", compressed)
            + _chunk(b"IHDR", valid_header)
            + _chunk(b"IEND", b""),
            _PNG_SIGNATURE
            + _chunk(b"IHDR", zero_width_header)
            + _chunk(b"IDAT", compressed)
            + _chunk(b"IEND", b""),
            _PNG_SIGNATURE
            + _chunk(b"IHDR", valid_header)
            + _chunk(b"IHDR", valid_header)
            + _chunk(b"IDAT", compressed)
            + _chunk(b"IEND", b""),
        )
        for malformed in cases:
            with self.subTest(length=len(malformed)):
                with self.assertRaises(PngError):
                    decode_png(malformed)

    def test_rejects_invalid_deflate_and_unsupported_chunks(self):
        header = struct.pack(">IIBBBBB", 1, 1, 8, 2, 0, 0, 0)
        invalid_deflate = (
            _PNG_SIGNATURE
            + _chunk(b"IHDR", header)
            + _chunk(b"IDAT", b"not a zlib stream")
            + _chunk(b"IEND", b"")
        )
        truncated_deflate = (
            _PNG_SIGNATURE
            + _chunk(b"IHDR", header)
            + _chunk(b"IDAT", zlib.compress(b"\x00\x01\x02\x03")[:-2])
            + _chunk(b"IEND", b"")
        )
        unsupported_chunk = (
            _PNG_SIGNATURE
            + _chunk(b"IHDR", header)
            + _chunk(b"tEXt", b"metadata")
            + _chunk(b"IDAT", zlib.compress(b"\x00\x01\x02\x03"))
            + _chunk(b"IEND", b"")
        )
        for malformed in (invalid_deflate, truncated_deflate, unsupported_chunk):
            with self.subTest(length=len(malformed)):
                with self.assertRaises(PngError):
                    decode_png(malformed)

    def test_rejects_invalid_filter_and_pixel_limit(self):
        invalid_filter = _make_png(1, [bytes((1, 2, 3))], filters=[5])
        with self.assertRaises(PngError):
            decode_png(invalid_filter)
        with self.assertRaises(PngError):
            decode_png(_make_png(2, [bytes((1, 2, 3, 4, 5, 6))]), max_pixels=1)

    def test_encoder_rejects_inconsistent_pixels_and_dimensions(self):
        with self.assertRaises(PngError):
            encode_png(PngImage(1, 1, b"rgb"))
        with self.assertRaises(PngError):
            encode_png(PngImage(4097, 1, b""))


if __name__ == "__main__":
    unittest.main()

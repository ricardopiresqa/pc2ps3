import unittest

from tools.atlas_layout import AtlasLayout, LayoutError, SpriteSize, pack_atlas


class AtlasLayoutTests(unittest.TestCase):
    def test_places_sprites_within_bounds_and_padding_without_overlap(self):
        sprites = [
            SpriteSize("small", 1, 1),
            SpriteSize("wide", 4, 3),
            SpriteSize("tall", 3, 4),
        ]
        layout = pack_atlas(sprites, max_width=8, max_height=10, padding=1)
        self.assertIsInstance(layout, AtlasLayout)
        self.assertLessEqual(layout.width, 8)
        self.assertLessEqual(layout.height, 10)
        self.assertEqual([item.id for item in layout.placements], [item.id for item in sprites])
        for placement in layout.placements:
            self.assertGreaterEqual(placement.x, 1)
            self.assertGreaterEqual(placement.y, 1)
            self.assertLessEqual(placement.x + placement.width + 1, layout.width)
            self.assertLessEqual(placement.y + placement.height + 1, layout.height)
        for index, left in enumerate(layout.placements):
            for right in layout.placements[index + 1:]:
                separated_x = (
                    left.x + left.width + 1 <= right.x
                    or right.x + right.width + 1 <= left.x
                )
                separated_y = (
                    left.y + left.height + 1 <= right.y
                    or right.y + right.height + 1 <= left.y
                )
                self.assertTrue(separated_x or separated_y)

    def test_layout_is_repeatable_and_empty_input_is_empty(self):
        sprites = [SpriteSize("b", 3, 2), SpriteSize("a", 2, 2)]
        first = pack_atlas(sprites, max_width=8, max_height=8, padding=0)
        second = pack_atlas(sprites, max_width=8, max_height=8, padding=0)
        self.assertEqual(first, second)
        self.assertEqual(pack_atlas([], max_width=8, max_height=8), AtlasLayout(0, 0, ()))

    def test_rejects_duplicate_ids_and_invalid_dimensions(self):
        invalid_inputs = (
            [SpriteSize("same", 1, 1), SpriteSize("same", 2, 2)],
            [SpriteSize("zero", 0, 1)],
            [SpriteSize("negative", 1, -1)],
            [SpriteSize("", 1, 1)],
            [SpriteSize("   ", 1, 1)],
        )
        for sprites in invalid_inputs:
            with self.subTest(sprites=sprites):
                with self.assertRaises(LayoutError):
                    pack_atlas(sprites, max_width=8, max_height=8)

    def test_rejects_padding_dimensions_capacity_and_no_rotation(self):
        cases = (
            ([SpriteSize("too-wide", 9, 1)], 8, 8, 0),
            ([SpriteSize("too-tall", 1, 9)], 8, 8, 0),
            ([SpriteSize("needs-padding", 7, 7)], 8, 8, 1),
            ([SpriteSize("large", 4097, 1)], 4096, 8, 0),
            ([SpriteSize("one", 1, 1)], 0, 8, 0),
            ([SpriteSize("one", 1, 1)], 8, 8, -1),
            ([SpriteSize("one", 1, 1)], 8, 8, 17),
        )
        for sprites, width, height, padding in cases:
            with self.subTest(sprites=sprites, width=width, height=height, padding=padding):
                with self.assertRaises(LayoutError):
                    pack_atlas(sprites, max_width=width, max_height=height, padding=padding)

    def test_rejects_item_count_above_limit(self):
        sprites = [SpriteSize(str(index), 1, 1) for index in range(1025)]
        with self.assertRaises(LayoutError):
            pack_atlas(sprites, max_width=4096, max_height=4096)


if __name__ == "__main__":
    unittest.main()

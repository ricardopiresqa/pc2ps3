"""Deterministic, bounded shelf layout for offline 2D sprite atlases."""

from typing import NamedTuple


MAX_SPRITES = 1024
MAX_DIMENSION = 4096
MAX_PADDING = 16
MAX_ID_BYTES = 128


class LayoutError(ValueError):
    """Raised when sprites cannot be laid out within the requested bounds."""


class SpriteSize(NamedTuple):
    id: str
    width: int
    height: int


class Placement(NamedTuple):
    id: str
    x: int
    y: int
    width: int
    height: int


class AtlasLayout(NamedTuple):
    width: int
    height: int
    placements: tuple


def _positive_dimension(value, name):
    if type(value) is not int or value <= 0 or value > MAX_DIMENSION:
        raise LayoutError("{} must be an integer from 1 to {}".format(name, MAX_DIMENSION))


def _validate_sprite(sprite):
    if not isinstance(sprite, SpriteSize):
        raise LayoutError("each sprite must be a SpriteSize")
    if (
        not isinstance(sprite.id, str)
        or not sprite.id.strip()
        or not sprite.id.isprintable()
    ):
        raise LayoutError("sprite ID must be a non-empty printable string")
    try:
        id_bytes = sprite.id.encode("utf-8")
    except UnicodeEncodeError as error:
        raise LayoutError("sprite ID is not valid UTF-8") from error
    if len(id_bytes) > MAX_ID_BYTES:
        raise LayoutError("sprite ID exceeds byte limit")
    _positive_dimension(sprite.width, "sprite width")
    _positive_dimension(sprite.height, "sprite height")


def pack_atlas(sprites, max_width, max_height, padding=0):
    """Place sprites deterministically and return only a complete layout.

    Padding is the transparent gap between neighboring sprites and the outer
    atlas edge. Items are sorted by descending height/width and then UTF-8 ID;
    returned placements retain the caller's input order. Rotation is disabled.
    """
    _positive_dimension(max_width, "max width")
    _positive_dimension(max_height, "max height")
    if type(padding) is not int or padding < 0 or padding > MAX_PADDING:
        raise LayoutError("padding must be an integer from 0 to {}".format(MAX_PADDING))
    if not isinstance(sprites, (list, tuple)):
        raise LayoutError("sprites must be a list or tuple")
    if len(sprites) > MAX_SPRITES:
        raise LayoutError("sprite count exceeds limit")
    if not sprites:
        return AtlasLayout(0, 0, ())

    seen_ids = set()
    for sprite in sprites:
        _validate_sprite(sprite)
        if sprite.id in seen_ids:
            raise LayoutError("sprite IDs must be unique")
        seen_ids.add(sprite.id)
        if sprite.width + 2 * padding > max_width or sprite.height + 2 * padding > max_height:
            raise LayoutError("sprite cannot fit within atlas bounds without rotation")

    ordered = sorted(sprites, key=lambda item: (-item.height, -item.width, item.id.encode("utf-8")))
    by_id = {}
    x = padding
    y = padding
    shelf_height = 0
    used_width = 0

    for sprite in ordered:
        if x > padding and x + sprite.width + padding > max_width:
            y += shelf_height + padding
            x = padding
            shelf_height = 0
        if y + sprite.height + padding > max_height:
            raise LayoutError("sprites do not fit within atlas bounds")
        by_id[sprite.id] = Placement(sprite.id, x, y, sprite.width, sprite.height)
        x += sprite.width + padding
        shelf_height = max(shelf_height, sprite.height)
        used_width = max(used_width, x)

    used_height = y + shelf_height + padding
    placements = tuple(by_id[sprite.id] for sprite in sprites)
    return AtlasLayout(used_width, used_height, placements)

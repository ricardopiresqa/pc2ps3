"""Validate bounded, path-free PC2PS3 fidelity traces."""

import argparse
import json
import sys
from pathlib import Path


SCHEMA_VERSION = 1
MAX_TRACE_BYTES = 16 * 1024 * 1024
MAX_SAMPLES = 65536
TOP_LEVEL_FIELDS = {"schema_version", "samples"}
SAMPLE_LIMITS = {
    "tick": (0, (1 << 64) - 1),
    "buttons": (0, (1 << 32) - 1),
    "axis_x": (-(1 << 15), (1 << 15) - 1),
    "axis_y": (-(1 << 15), (1 << 15) - 1),
    "position_x_milli": (-(1 << 31), (1 << 31) - 1),
    "position_y_milli": (-(1 << 31), (1 << 31) - 1),
    "velocity_x_milli": (-(1 << 31), (1 << 31) - 1),
    "velocity_y_milli": (-(1 << 31), (1 << 31) - 1),
    "collision_mask": (0, (1 << 32) - 1),
    "state_id": (0, (1 << 32) - 1),
    "animation_id": (0, (1 << 32) - 1),
    "camera_x_milli": (-(1 << 31), (1 << 31) - 1),
    "camera_y_milli": (-(1 << 31), (1 << 31) - 1),
    "checkpoint_id": (0, (1 << 32) - 1),
    "phase_id": (0, (1 << 32) - 1),
    "audio_event_mask": (0, (1 << 32) - 1),
}


class TraceError(ValueError):
    """Invalid or oversized trace input."""


def _object_without_duplicates(pairs):
    result = {}
    for key, value in pairs:
        if key in result:
            raise TraceError("duplicate JSON field")
        result[key] = value
    return result


def _reject_constant(_value):
    raise TraceError("non-standard JSON number")


def _require_exact_fields(value, expected, label):
    if not isinstance(value, dict):
        raise TraceError("{} must be an object".format(label))
    if set(value) != expected:
        raise TraceError("{} has missing or unknown fields".format(label))


def _require_integer(value, minimum, maximum, label):
    if isinstance(value, bool) or not isinstance(value, int):
        raise TraceError("{} must be an integer".format(label))
    if value < minimum or value > maximum:
        raise TraceError("{} is out of range".format(label))
    return value


def validate_trace(document):
    """Return a normalized trace or raise TraceError without echoing values."""
    _require_exact_fields(document, TOP_LEVEL_FIELDS, "trace")
    version = _require_integer(
        document["schema_version"], SCHEMA_VERSION, SCHEMA_VERSION,
        "schema_version")
    samples = document["samples"]
    if not isinstance(samples, list):
        raise TraceError("samples must be an array")
    if len(samples) > MAX_SAMPLES:
        raise TraceError("samples exceeds the configured limit")

    normalized = []
    previous_tick = None
    for index, sample in enumerate(samples):
        label = "sample {}".format(index)
        _require_exact_fields(sample, set(SAMPLE_LIMITS), label)
        clean = {}
        for name, limits in SAMPLE_LIMITS.items():
            clean[name] = _require_integer(
                sample[name], limits[0], limits[1], label + "." + name)
        if previous_tick is not None and clean["tick"] <= previous_tick:
            raise TraceError(label + ".tick must increase")
        previous_tick = clean["tick"]
        normalized.append(clean)

    return {"schema_version": version, "samples": normalized}


def load_trace(path):
    """Read at most MAX_TRACE_BYTES and validate a UTF-8 JSON trace."""
    try:
        with Path(path).open("rb") as stream:
            raw = stream.read(MAX_TRACE_BYTES + 1)
    except OSError as error:
        raise TraceError("unable to read trace") from error
    if len(raw) > MAX_TRACE_BYTES:
        raise TraceError("trace exceeds the configured byte limit")
    try:
        text = raw.decode("utf-8")
        document = json.loads(
            text,
            object_pairs_hook=_object_without_duplicates,
            parse_constant=_reject_constant)
    except TraceError:
        raise
    except (UnicodeDecodeError, json.JSONDecodeError, RecursionError) as error:
        raise TraceError("trace is not valid UTF-8 JSON") from error
    return validate_trace(document)


def summarize(trace):
    samples = trace["samples"]
    return {
        "schema_version": trace["schema_version"],
        "sample_count": len(samples),
        "first_tick": samples[0]["tick"] if samples else None,
        "last_tick": samples[-1]["tick"] if samples else None,
    }


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("trace", type=Path)
    args = parser.parse_args(argv)
    try:
        summary = summarize(load_trace(args.trace))
    except TraceError as error:
        print("invalid trace: {}".format(error), file=sys.stderr)
        return 2
    print(json.dumps(summary, sort_keys=True, separators=(",", ":")))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

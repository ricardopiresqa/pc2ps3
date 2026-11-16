"""Compare two bounded PC2PS3 FidelityTrace JSON files."""

import argparse
import json
import sys

import fidelity_trace_json


_MAX_TOLERANCE = (1 << 31) - 1


def _validate_tolerance(value, name):
    if isinstance(value, bool) or not isinstance(value, int):
        raise fidelity_trace_json.TraceError(name + " must be an integer")
    if value < 0 or value > _MAX_TOLERANCE:
        raise fidelity_trace_json.TraceError(name + " is out of range")
    return value


def compare_traces(reference, candidate, position_tolerance=0,
                   velocity_tolerance=0, camera_tolerance=0):
    reference = fidelity_trace_json.validate_trace(reference)
    candidate = fidelity_trace_json.validate_trace(candidate)
    position_tolerance = _validate_tolerance(
        position_tolerance, "position tolerance")
    velocity_tolerance = _validate_tolerance(
        velocity_tolerance, "velocity tolerance")
    camera_tolerance = _validate_tolerance(camera_tolerance, "camera tolerance")

    left = reference["samples"]
    right = candidate["samples"]
    if not left or len(left) != len(right):
        raise fidelity_trace_json.TraceError("trace sample counts are incompatible")
    if any(a["tick"] != b["tick"] for a, b in zip(left, right)):
        raise fidelity_trace_json.TraceError("trace tick sequences are incompatible")

    counts = {
        "position": 0,
        "velocity": 0,
        "camera": 0,
        "input": 0,
        "collision": 0,
        "state": 0,
        "animation": 0,
        "progression": 0,
        "audio": 0,
    }
    maximum = {"position": 0, "velocity": 0, "camera": 0}

    for a, b in zip(left, right):
        errors = {
            "position": max(
                abs(a["position_x_milli"] - b["position_x_milli"]),
                abs(a["position_y_milli"] - b["position_y_milli"])),
            "velocity": max(
                abs(a["velocity_x_milli"] - b["velocity_x_milli"]),
                abs(a["velocity_y_milli"] - b["velocity_y_milli"])),
            "camera": max(
                abs(a["camera_x_milli"] - b["camera_x_milli"]),
                abs(a["camera_y_milli"] - b["camera_y_milli"])),
        }
        tolerances = {
            "position": position_tolerance,
            "velocity": velocity_tolerance,
            "camera": camera_tolerance,
        }
        for category, error in errors.items():
            maximum[category] = max(maximum[category], error)
            if error > tolerances[category]:
                counts[category] += 1

        if (a["buttons"], a["axis_x"], a["axis_y"]) != (
                b["buttons"], b["axis_x"], b["axis_y"]):
            counts["input"] += 1
        if a["collision_mask"] != b["collision_mask"]:
            counts["collision"] += 1
        if a["state_id"] != b["state_id"]:
            counts["state"] += 1
        if a["animation_id"] != b["animation_id"]:
            counts["animation"] += 1
        if (a["checkpoint_id"], a["phase_id"]) != (
                b["checkpoint_id"], b["phase_id"]):
            counts["progression"] += 1
        if a["audio_event_mask"] != b["audio_event_mask"]:
            counts["audio"] += 1

    return {
        "schema_version": 1,
        "sample_count": len(left),
        "verdict": "PASS" if not any(counts.values()) else "DIVERGED",
        "counts": counts,
        "max_error_milli": maximum,
    }


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("reference")
    parser.add_argument("candidate")
    parser.add_argument("--position-tolerance-milli", type=int, default=0)
    parser.add_argument("--velocity-tolerance-milli", type=int, default=0)
    parser.add_argument("--camera-tolerance-milli", type=int, default=0)
    args = parser.parse_args(argv)

    try:
        result = compare_traces(
            fidelity_trace_json.load_trace(args.reference),
            fidelity_trace_json.load_trace(args.candidate),
            args.position_tolerance_milli,
            args.velocity_tolerance_milli,
            args.camera_tolerance_milli)
    except fidelity_trace_json.TraceError as error:
        print("invalid comparison: {}".format(error), file=sys.stderr)
        return 2

    print(json.dumps(result, sort_keys=True, separators=(",", ":")))
    return 0 if result["verdict"] == "PASS" else 1


if __name__ == "__main__":
    raise SystemExit(main())

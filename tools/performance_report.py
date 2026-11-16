"""Summarize bounded PC2PS3 frame-time and memory sample reports."""

import argparse
import json
import sys
from pathlib import Path


SCHEMA_VERSION = 1
MAX_REPORT_BYTES = 16 * 1024 * 1024
MAX_SAMPLES = 65536
MAX_FRAME_NS = 10_000_000_000
MAX_UINT64 = (1 << 64) - 1
TOP_LEVEL_FIELDS = {
    "schema_version", "ram_budget_bytes", "vram_budget_bytes", "samples"
}
SAMPLE_FIELDS = {"elapsed_ns", "ram_bytes", "vram_bytes"}


class ReportError(ValueError):
    """Invalid, oversized, or inconsistent performance report."""


def _object_without_duplicates(pairs):
    result = {}
    for key, value in pairs:
        if key in result:
            raise ReportError("duplicate JSON field")
        result[key] = value
    return result


def _reject_constant(_value):
    raise ReportError("non-standard JSON number")


def _require_fields(value, expected, label):
    if not isinstance(value, dict):
        raise ReportError("{} must be an object".format(label))
    if set(value) != expected:
        raise ReportError("{} has missing or unknown fields".format(label))


def _require_integer(value, minimum, maximum, label):
    if isinstance(value, bool) or not isinstance(value, int):
        raise ReportError("{} must be an integer".format(label))
    if value < minimum or value > maximum:
        raise ReportError("{} is out of range".format(label))
    return value


def _require_memory(value, label):
    if value is None:
        return None
    return _require_integer(value, 0, MAX_UINT64, label)


def validate_report(document):
    _require_fields(document, TOP_LEVEL_FIELDS, "report")
    version = _require_integer(
        document["schema_version"], SCHEMA_VERSION, SCHEMA_VERSION,
        "schema_version")
    ram_budget = _require_integer(
        document["ram_budget_bytes"], 1, MAX_UINT64, "ram_budget_bytes")
    vram_budget = _require_integer(
        document["vram_budget_bytes"], 1, MAX_UINT64, "vram_budget_bytes")
    samples = document["samples"]
    if not isinstance(samples, list):
        raise ReportError("samples must be an array")
    if len(samples) > MAX_SAMPLES:
        raise ReportError("samples exceeds the configured limit")

    normalized = []
    for index, sample in enumerate(samples):
        label = "sample {}".format(index)
        _require_fields(sample, SAMPLE_FIELDS, label)
        normalized.append({
            "elapsed_ns": _require_integer(
                sample["elapsed_ns"], 1, MAX_FRAME_NS, label + ".elapsed_ns"),
            "ram_bytes": _require_memory(sample["ram_bytes"], label + ".ram_bytes"),
            "vram_bytes": _require_memory(sample["vram_bytes"], label + ".vram_bytes"),
        })
    return {
        "schema_version": version,
        "ram_budget_bytes": ram_budget,
        "vram_budget_bytes": vram_budget,
        "samples": normalized,
    }


def load_report(path):
    try:
        with Path(path).open("rb") as stream:
            raw = stream.read(MAX_REPORT_BYTES + 1)
    except OSError as error:
        raise ReportError("unable to read performance report") from error
    if len(raw) > MAX_REPORT_BYTES:
        raise ReportError("report exceeds the configured byte limit")
    try:
        document = json.loads(
            raw.decode("utf-8"),
            object_pairs_hook=_object_without_duplicates,
            parse_constant=_reject_constant)
    except ReportError:
        raise
    except (UnicodeDecodeError, json.JSONDecodeError, RecursionError) as error:
        raise ReportError("report is not valid UTF-8 JSON") from error
    return validate_report(document)


def summarize_report(report):
    samples = report["samples"]
    count = len(samples)
    frame_times = [sample["elapsed_ns"] for sample in samples]
    ram_values = [sample["ram_bytes"] for sample in samples
                  if sample["ram_bytes"] is not None]
    vram_values = [sample["vram_bytes"] for sample in samples
                   if sample["vram_bytes"] is not None]
    ram_violations = sum(value > report["ram_budget_bytes"] for value in ram_values)
    vram_violations = sum(value > report["vram_budget_bytes"] for value in vram_values)

    if ram_violations or vram_violations:
        status = "OVER_BUDGET"
    elif (count == 0 or len(ram_values) != count or len(vram_values) != count):
        status = "NOT_PROVEN"
    else:
        status = "PASS"

    average_ns = sum(frame_times) // count if count else None
    return {
        "schema_version": report["schema_version"],
        "status": status,
        "sample_count": count,
        "fps": (1_000_000_000 // average_ns) if average_ns else None,
        "frame_time_ns": {
            "average": average_ns,
            "minimum": min(frame_times) if frame_times else None,
            "maximum": max(frame_times) if frame_times else None,
        },
        "ram": {
            "budget_bytes": report["ram_budget_bytes"],
            "known_samples": len(ram_values),
            "peak_bytes": max(ram_values) if ram_values else None,
            "over_budget_samples": ram_violations,
        },
        "vram": {
            "budget_bytes": report["vram_budget_bytes"],
            "known_samples": len(vram_values),
            "peak_bytes": max(vram_values) if vram_values else None,
            "over_budget_samples": vram_violations,
        },
    }


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("report", type=Path)
    args = parser.parse_args(argv)
    try:
        summary = summarize_report(load_report(args.report))
    except ReportError as error:
        print("invalid performance report: {}".format(error), file=sys.stderr)
        return 2
    print(json.dumps(summary, sort_keys=True, separators=(",", ":")))
    return {"PASS": 0, "OVER_BUDGET": 1, "NOT_PROVEN": 3}[summary["status"]]


if __name__ == "__main__":
    raise SystemExit(main())

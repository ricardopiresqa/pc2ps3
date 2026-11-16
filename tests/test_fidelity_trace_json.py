import contextlib
import io
import json
import sys
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

REPOSITORY_ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(REPOSITORY_ROOT / "tools"))

import fidelity_trace_json


def make_sample(tick=0):
    return {
        "tick": tick,
        "buttons": 3,
        "axis_x": -10,
        "axis_y": 20,
        "position_x_milli": -100,
        "position_y_milli": 200,
        "velocity_x_milli": 300,
        "velocity_y_milli": -400,
        "collision_mask": 1,
        "state_id": 2,
        "animation_id": 4,
        "camera_x_milli": 500,
        "camera_y_milli": -600,
        "checkpoint_id": 7,
        "phase_id": 8,
        "audio_event_mask": 16,
    }


def make_trace(samples=None):
    return {"schema_version": 1, "samples": [make_sample(0)] if samples is None else samples}


class FidelityTraceJsonTests(unittest.TestCase):
    def test_valid_trace_is_normalized_and_summarized(self):
        trace = fidelity_trace_json.validate_trace(
            make_trace([make_sample(0), make_sample(4)]))
        self.assertEqual(trace["samples"][1]["tick"], 4)
        self.assertEqual(fidelity_trace_json.summarize(trace), {
            "schema_version": 1,
            "sample_count": 2,
            "first_tick": 0,
            "last_tick": 4,
        })

    def test_empty_trace_is_structurally_valid(self):
        trace = fidelity_trace_json.validate_trace(make_trace([]))
        self.assertEqual(fidelity_trace_json.summarize(trace)["sample_count"], 0)

    def test_rejects_missing_unknown_and_wrong_version_fields(self):
        with self.assertRaises(fidelity_trace_json.TraceError):
            fidelity_trace_json.validate_trace({"schema_version": 1})
        extra = make_trace()
        extra["game_name"] = "not allowed"
        with self.assertRaises(fidelity_trace_json.TraceError):
            fidelity_trace_json.validate_trace(extra)
        extra_sample = make_sample()
        extra_sample["name"] = "not allowed"
        with self.assertRaises(fidelity_trace_json.TraceError):
            fidelity_trace_json.validate_trace(make_trace([extra_sample]))
        wrong_version = make_trace()
        wrong_version["schema_version"] = 2
        with self.assertRaises(fidelity_trace_json.TraceError):
            fidelity_trace_json.validate_trace(wrong_version)

    def test_rejects_wrong_types_booleans_and_out_of_range_values(self):
        for value in (True, 1.5, "1", -1, 1 << 64):
            sample = make_sample()
            sample["tick"] = value
            with self.subTest(value_type=type(value).__name__):
                with self.assertRaises(fidelity_trace_json.TraceError):
                    fidelity_trace_json.validate_trace(make_trace([sample]))

        sample = make_sample()
        sample["axis_x"] = 32768
        with self.assertRaises(fidelity_trace_json.TraceError):
            fidelity_trace_json.validate_trace(make_trace([sample]))

    def test_rejects_non_monotonic_ticks(self):
        with self.assertRaises(fidelity_trace_json.TraceError):
            fidelity_trace_json.validate_trace(
                make_trace([make_sample(3), make_sample(3)]))
        with self.assertRaises(fidelity_trace_json.TraceError):
            fidelity_trace_json.validate_trace(
                make_trace([make_sample(4), make_sample(2)]))

    def test_rejects_duplicate_keys_and_nonstandard_numbers(self):
        with tempfile.TemporaryDirectory(dir=REPOSITORY_ROOT / "build") as temp_dir:
            path = Path(temp_dir) / "trace.json"
            path.write_text('{"schema_version":1,"schema_version":1,"samples":[]}',
                            encoding="utf-8")
            with self.assertRaises(fidelity_trace_json.TraceError):
                fidelity_trace_json.load_trace(path)
            path.write_text('{"schema_version":NaN,"samples":[]}',
                            encoding="utf-8")
            with self.assertRaises(fidelity_trace_json.TraceError):
                fidelity_trace_json.load_trace(path)

    def test_rejects_oversized_file_before_json_parse(self):
        with tempfile.TemporaryDirectory(dir=REPOSITORY_ROOT / "build") as temp_dir:
            path = Path(temp_dir) / "large.json"
            path.write_bytes(b" " * 33)
            with patch.object(fidelity_trace_json, "MAX_TRACE_BYTES", 32):
                with self.assertRaises(fidelity_trace_json.TraceError):
                    fidelity_trace_json.load_trace(path)

    def test_rejects_too_many_samples(self):
        with patch.object(fidelity_trace_json, "MAX_SAMPLES", 1):
            with self.assertRaises(fidelity_trace_json.TraceError):
                fidelity_trace_json.validate_trace(
                    make_trace([make_sample(0), make_sample(1)]))

    def test_rejects_invalid_utf8(self):
        with tempfile.TemporaryDirectory(dir=REPOSITORY_ROOT / "build") as temp_dir:
            path = Path(temp_dir) / "invalid.json"
            path.write_bytes(b"\xff")
            with self.assertRaises(fidelity_trace_json.TraceError):
                fidelity_trace_json.load_trace(path)

    def test_cli_does_not_echo_path_or_trace_values(self):
        with tempfile.TemporaryDirectory(dir=REPOSITORY_ROOT / "build") as temp_dir:
            path = Path(temp_dir) / "private-name.json"
            path.write_text(json.dumps(make_trace([make_sample(12)])),
                            encoding="utf-8")
            stdout = io.StringIO()
            stderr = io.StringIO()
            with contextlib.redirect_stdout(stdout), contextlib.redirect_stderr(stderr):
                result = fidelity_trace_json.main([str(path)])
            self.assertEqual(result, 0)
            self.assertEqual(stderr.getvalue(), "")
            output = stdout.getvalue()
            self.assertNotIn(str(path), output)
            self.assertNotIn("position_x_milli", output)
            self.assertEqual(json.loads(output)["last_tick"], 12)


if __name__ == "__main__":
    unittest.main()

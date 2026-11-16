import contextlib
import io
import json
import sys
import tempfile
import unittest
from pathlib import Path

REPOSITORY_ROOT = Path(__file__).resolve().parents[1]
TOOLS_DIR = REPOSITORY_ROOT / "tools"
sys.path.insert(0, str(TOOLS_DIR))

import compare_fidelity_trace
import fidelity_trace_json


def make_sample(tick=0):
    return {
        "tick": tick,
        "buttons": 1,
        "axis_x": 10,
        "axis_y": -10,
        "position_x_milli": 1000,
        "position_y_milli": 2000,
        "velocity_x_milli": 3000,
        "velocity_y_milli": 4000,
        "collision_mask": 2,
        "state_id": 3,
        "animation_id": 4,
        "camera_x_milli": 5000,
        "camera_y_milli": 6000,
        "checkpoint_id": 7,
        "phase_id": 8,
        "audio_event_mask": 16,
    }


def make_trace(samples):
    return {"schema_version": 1, "samples": samples}


class FidelityTraceComparisonTests(unittest.TestCase):
    def test_equal_traces_pass_deterministically(self):
        trace = make_trace([make_sample(0), make_sample(1)])
        first = compare_fidelity_trace.compare_traces(trace, trace)
        second = compare_fidelity_trace.compare_traces(trace, trace)
        self.assertEqual(first, second)
        self.assertEqual(first["verdict"], "PASS")
        self.assertEqual(first["sample_count"], 2)

    def test_numeric_tolerances_are_inclusive(self):
        expected = make_sample(0)
        actual = dict(expected)
        actual["position_x_milli"] += 2
        actual["velocity_y_milli"] -= 3
        actual["camera_y_milli"] += 4
        result = compare_fidelity_trace.compare_traces(
            make_trace([expected]), make_trace([actual]), 2, 3, 4)
        self.assertEqual(result["verdict"], "PASS")
        self.assertEqual(result["max_error_milli"], {
            "position": 2, "velocity": 3, "camera": 4})

    def test_numeric_error_above_tolerance_is_divergence(self):
        expected = make_sample(0)
        actual = dict(expected)
        actual["position_x_milli"] += 3
        result = compare_fidelity_trace.compare_traces(
            make_trace([expected]), make_trace([actual]), 2)
        self.assertEqual(result["verdict"], "DIVERGED")
        self.assertEqual(result["counts"]["position"], 1)

    def test_categorical_divergences_are_exact(self):
        expected = make_sample(0)
        actual = dict(expected)
        for key in ("buttons", "collision_mask", "state_id", "animation_id",
                    "checkpoint_id", "phase_id", "audio_event_mask"):
            actual[key] += 1
        result = compare_fidelity_trace.compare_traces(
            make_trace([expected]), make_trace([actual]), 100, 100, 100)
        self.assertEqual(result["verdict"], "DIVERGED")
        self.assertEqual(result["counts"]["input"], 1)
        self.assertEqual(result["counts"]["collision"], 1)
        self.assertEqual(result["counts"]["state"], 1)
        self.assertEqual(result["counts"]["animation"], 1)
        self.assertEqual(result["counts"]["progression"], 1)
        self.assertEqual(result["counts"]["audio"], 1)

    def test_incompatible_ticks_and_empty_trace_are_errors(self):
        with self.assertRaises(fidelity_trace_json.TraceError):
            compare_fidelity_trace.compare_traces(
                make_trace([make_sample(0)]), make_trace([make_sample(1)]))
        with self.assertRaises(fidelity_trace_json.TraceError):
            compare_fidelity_trace.compare_traces(make_trace([]), make_trace([]))

    def test_invalid_tolerance_is_an_error(self):
        for value in (-1, True, 1 << 31):
            with self.subTest(value=value):
                with self.assertRaises(fidelity_trace_json.TraceError):
                    compare_fidelity_trace.compare_traces(
                        make_trace([make_sample()]), make_trace([make_sample()]),
                        value)

    def test_cli_exit_codes_and_output_do_not_echo_paths(self):
        with tempfile.TemporaryDirectory(dir=REPOSITORY_ROOT / "build") as temp_dir:
            reference = Path(temp_dir) / "private-reference.json"
            candidate = Path(temp_dir) / "private-candidate.json"
            reference.write_text(json.dumps(make_trace([make_sample(3)])),
                                 encoding="utf-8")
            candidate.write_text(json.dumps(make_trace([make_sample(3)])),
                                 encoding="utf-8")
            stdout = io.StringIO()
            stderr = io.StringIO()
            with contextlib.redirect_stdout(stdout), contextlib.redirect_stderr(stderr):
                result = compare_fidelity_trace.main([str(reference), str(candidate)])
            self.assertEqual(result, 0)
            self.assertNotIn(str(reference), stdout.getvalue())
            self.assertNotIn(str(candidate), stdout.getvalue())
            self.assertEqual(json.loads(stdout.getvalue())["verdict"], "PASS")
            first_output = stdout.getvalue()

            stdout = io.StringIO()
            with contextlib.redirect_stdout(stdout), contextlib.redirect_stderr(io.StringIO()):
                result = compare_fidelity_trace.main([str(reference), str(candidate)])
            self.assertEqual(result, 0)
            self.assertEqual(stdout.getvalue(), first_output)

            divergent = make_sample(3)
            divergent["position_x_milli"] += 5
            candidate.write_text(json.dumps(make_trace([divergent])), encoding="utf-8")
            stdout = io.StringIO()
            with contextlib.redirect_stdout(stdout), contextlib.redirect_stderr(io.StringIO()):
                result = compare_fidelity_trace.main([str(reference), str(candidate)])
            self.assertEqual(result, 1)
            self.assertEqual(json.loads(stdout.getvalue())["verdict"], "DIVERGED")

            candidate.write_text("{}", encoding="utf-8")
            stdout = io.StringIO()
            stderr = io.StringIO()
            with contextlib.redirect_stdout(stdout), contextlib.redirect_stderr(stderr):
                result = compare_fidelity_trace.main([str(reference), str(candidate)])
            self.assertEqual(result, 2)
            self.assertEqual(stdout.getvalue(), "")
            self.assertNotIn(str(candidate), stderr.getvalue())


if __name__ == "__main__":
    unittest.main()

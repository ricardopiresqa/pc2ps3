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

import performance_report


def make_sample(elapsed_ns=10_000_000, ram_bytes=800, vram_bytes=300):
    return {
        "elapsed_ns": elapsed_ns,
        "ram_bytes": ram_bytes,
        "vram_bytes": vram_bytes,
    }


def make_report(samples=None, ram_budget=1000, vram_budget=500):
    return {
        "schema_version": 1,
        "ram_budget_bytes": ram_budget,
        "vram_budget_bytes": vram_budget,
        "samples": [make_sample()] if samples is None else samples,
    }


class PerformanceReportTests(unittest.TestCase):
    def test_aggregates_fps_frame_times_and_memory_peaks(self):
        report = performance_report.validate_report(make_report([
            make_sample(20_000_000, 800, 300),
            make_sample(10_000_000, 1000, 500),
        ]))
        summary = performance_report.summarize_report(report)
        self.assertEqual(summary["status"], "PASS")
        self.assertEqual(summary["fps"], 66)
        self.assertEqual(summary["frame_time_ns"], {
            "average": 15_000_000,
            "minimum": 10_000_000,
            "maximum": 20_000_000,
        })
        self.assertEqual(summary["ram"]["peak_bytes"], 1000)
        self.assertEqual(summary["vram"]["peak_bytes"], 500)

    def test_memory_absence_is_not_zero_or_pass(self):
        report = performance_report.validate_report(make_report([
            make_sample(10_000_000, None, None),
            make_sample(10_000_000, None, 200),
        ]))
        summary = performance_report.summarize_report(report)
        self.assertEqual(summary["status"], "NOT_PROVEN")
        self.assertEqual(summary["ram"]["known_samples"], 0)
        self.assertIsNone(summary["ram"]["peak_bytes"])
        self.assertEqual(summary["vram"]["known_samples"], 1)

    def test_any_known_budget_violation_is_reported_even_if_partial(self):
        report = performance_report.validate_report(make_report([
            make_sample(10_000_000, 1200, None),
        ]))
        summary = performance_report.summarize_report(report)
        self.assertEqual(summary["status"], "OVER_BUDGET")
        self.assertEqual(summary["ram"]["over_budget_samples"], 1)
        self.assertEqual(summary["vram"]["over_budget_samples"], 0)

    def test_empty_report_is_not_proven(self):
        summary = performance_report.summarize_report(
            performance_report.validate_report(make_report([])))
        self.assertEqual(summary["status"], "NOT_PROVEN")
        self.assertIsNone(summary["fps"])

    def test_rejects_invalid_budgets_samples_and_fields(self):
        invalid = make_report(ram_budget=0)
        with self.assertRaises(performance_report.ReportError):
            performance_report.validate_report(invalid)
        for sample in (make_sample(0), make_sample(True),
                       make_sample(performance_report.MAX_FRAME_NS + 1),
                       make_sample(10_000_000, True, 100)):
            with self.subTest(sample=sample):
                with self.assertRaises(performance_report.ReportError):
                    performance_report.validate_report(make_report([sample]))
        extra = make_report()
        extra["source_path"] = "must not be accepted"
        with self.assertRaises(performance_report.ReportError):
            performance_report.validate_report(extra)
        with patch.object(performance_report, "MAX_SAMPLES", 1):
            with self.assertRaises(performance_report.ReportError):
                performance_report.validate_report(make_report([
                    make_sample(), make_sample()]))

    def test_rejects_duplicate_json_fields_and_oversized_input(self):
        with tempfile.TemporaryDirectory(dir=REPOSITORY_ROOT / "build") as temp_dir:
            path = Path(temp_dir) / "report.json"
            path.write_text(
                '{"schema_version":1,"schema_version":1,'
                '"ram_budget_bytes":1000,"vram_budget_bytes":500,"samples":[]}',
                encoding="utf-8")
            with self.assertRaises(performance_report.ReportError):
                performance_report.load_report(path)
            path.write_bytes(b" " * 17)
            with patch.object(performance_report, "MAX_REPORT_BYTES", 16):
                with self.assertRaises(performance_report.ReportError):
                    performance_report.load_report(path)

    def test_cli_status_exit_code_and_output_are_deterministic(self):
        with tempfile.TemporaryDirectory(dir=REPOSITORY_ROOT / "build") as temp_dir:
            path = Path(temp_dir) / "private-report.json"
            path.write_text(json.dumps(make_report()), encoding="utf-8")
            outputs = []
            for _ in range(2):
                stdout = io.StringIO()
                with contextlib.redirect_stdout(stdout), contextlib.redirect_stderr(io.StringIO()):
                    result = performance_report.main([str(path)])
                self.assertEqual(result, 0)
                self.assertNotIn(str(path), stdout.getvalue())
                outputs.append(stdout.getvalue())
            self.assertEqual(outputs[0], outputs[1])

            path.write_text(json.dumps(make_report([
                make_sample(10_000_000, None, None)])), encoding="utf-8")
            stdout = io.StringIO()
            with contextlib.redirect_stdout(stdout), contextlib.redirect_stderr(io.StringIO()):
                result = performance_report.main([str(path)])
            self.assertEqual(result, 3)
            self.assertEqual(json.loads(stdout.getvalue())["status"], "NOT_PROVEN")

            path.write_text(json.dumps(make_report([
                make_sample(10_000_000, 1200, 300)])), encoding="utf-8")
            stdout = io.StringIO()
            with contextlib.redirect_stdout(stdout), contextlib.redirect_stderr(io.StringIO()):
                result = performance_report.main([str(path)])
            self.assertEqual(result, 1)
            self.assertEqual(json.loads(stdout.getvalue())["status"], "OVER_BUDGET")


if __name__ == "__main__":
    unittest.main()

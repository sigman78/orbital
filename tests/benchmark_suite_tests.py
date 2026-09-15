"""Validate benchmark analysis without requiring a GPU or demo executable."""
import copy
import importlib.util
from pathlib import Path
import unittest

spec = importlib.util.spec_from_file_location('benchmark_suite', Path(__file__).resolve().parents[1] / 'tools/benchmark-suite.py')
suite = importlib.util.module_from_spec(spec)
spec.loader.exec_module(suite)


def report(value=10, low=9.9, high=10.1):
    metrics = {key: {'median': value, 'p95': value, 'run_min': low, 'run_max': high} for key in suite.METRICS}
    return {'label': 'fixture', 'suite_version': 1, 'protocol': {'repeats': 3},
            'machine': {'gpu': 'fixture'}, 'assets_sha256': 'fixture',
            'cases': {'belt/fullscreen': {'resolution': [3440, 1440], 'metrics': metrics, 'runs': [{}, {}, {}]}}}


class BenchmarkTests(unittest.TestCase):
    def test_warmup_and_tail(self):
        rows = [{key: str(i) for key in suite.METRICS} for i in [999] * 60 + list(range(1, 101))]
        stats = suite.summarize(rows, 60)['gpu_ms']
        self.assertEqual(stats['median'], 50.5)
        self.assertEqual(stats['p95'], 95)
        rows[-1]['gpu_ms'] = 'nan'
        with self.assertRaises(ValueError):
            suite.summarize(rows, 60)

    def test_children_summarized_and_optional_in_baselines(self):
        fields = suite.METRICS + ['gpu_child_ms', 'frame_ms']
        self.assertEqual(suite.csv_metrics(fields), suite.METRICS + ['gpu_child_ms'])
        rows = [{key: str(i) for key in fields} for i in [999] * 60 + list(range(1, 101))]
        stats = suite.summarize(rows, 60, suite.csv_metrics(fields))
        self.assertEqual(stats['gpu_child_ms']['median'], 50.5)
        self.assertNotIn('frame_ms', stats)
        # A report with children compares with a baseline without them, on the shared metrics.
        current = report()
        current['cases']['belt/fullscreen']['metrics']['gpu_child_ms'] = {'median': 1, 'p95': 1, 'run_min': 1, 'run_max': 1}
        changes = suite.compare(current, report())['belt/fullscreen']
        self.assertIn('gpu_ms', changes)
        self.assertNotIn('gpu_child_ms', changes)

    def test_noise_and_absolute_floor(self):
        base = report()
        self.assertEqual(suite.compare(report(11, 10.9, 11.1), base)['belt/fullscreen']['gpu_ms']['status'], 'regression')
        self.assertEqual(suite.compare(report(11, 10, 12), base)['belt/fullscreen']['gpu_ms']['status'], 'noisy increase')
        self.assertEqual(suite.compare(report(.2, .19, .21), report(.1, .09, .11))['belt/fullscreen']['gpu_ms']['status'], 'within threshold')

    def test_mismatched_or_partial_runs_rejected(self):
        base = report()
        for field in ['machine', 'assets_sha256', 'protocol']:
            other = copy.deepcopy(base)
            other[field] = 'different'
            with self.assertRaises(ValueError):
                suite.compare(other, base)
        other = copy.deepcopy(base)
        other['cases']['belt/fullscreen']['resolution'] = [1600, 900]
        with self.assertRaises(ValueError):
            suite.compare(other, base)
        other = copy.deepcopy(base)
        other['cases']['belt/fullscreen']['runs'].pop()
        with self.assertRaises(ValueError):
            suite.compare(other, base)


if __name__ == '__main__':
    unittest.main()

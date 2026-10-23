"""Exercise the actual report executable and its filesystem behavior."""
import json
import os
from pathlib import Path
import re
import resource
import signal
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
BINARY = Path(os.environ.get('STS_REPORT_BIN', ROOT / 'cmake-build-report/sts-report'))


def embedded_report(path):
    match = re.search(r'<script\b[^>]*\bid="report-data"[^>]*>(.*?)</script>', path.read_text(), re.S)
    if not match:
        raise AssertionError('standalone report data missing')
    return json.loads(match.group(1))


class ReportCommandTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix='sts-command-', dir='/tmp/opencode')
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)

    def run_report(self, *args):
        return subprocess.run([str(BINARY), *map(str, args)], capture_output=True, text=True, timeout=30)

    def test_real_simple_run(self):
        output = self.root / 'simple.html'
        result = self.run_report('--seed', '1', '--agent', 'simple', '--output', output)
        self.assertEqual(result.returncode, 0, result.stderr)
        report = embedded_report(output)
        self.assertIn(report['status'], ('victory', 'loss'))
        self.assertEqual(report['run']['seed'], '1')
        self.assertEqual(report['run']['agent'], 'simple')
        self.assertTrue(any(f['state']['combat'] for f in report['frames']))
        self.assertTrue(any(not f['state']['combat'] for f in report['frames']))
        self.assertTrue(output.with_name(output.name + '.trace.jsonl').exists())
        self.assertNotIn('<script src=', output.read_text())

    def test_real_search_run(self):
        output = self.root / 'search.html'
        result = self.run_report('--seed', '1', '--agent', 'search', '--simulations', '10', '--output', output)
        self.assertEqual(result.returncode, 0, result.stderr)
        report = embedded_report(output)
        self.assertEqual(report['run']['settings']['simulationCount'], 10)
        self.assertEqual(report['run']['settings']['bossSimulationMultiplier'], 3)
        self.assertIn(report['status'], ('victory', 'loss'))

    def test_invalid_arguments_never_start_recording(self):
        output = self.root / 'bad.html'
        cases = [[], ['--seed', '1'], ['--output', output],
                 ['--seed', '!'], ['--seed', '1', '--ascension', '21'],
                 ['--seed', '1', '--unknown', '2'],
                 ['--seed', '1', '--simulations', '0'],
                 ['--seed', '1', '--simulations', '2147483648'],
                 ['--seed', '1', '--agent', 'simple', '--simulations', '10'],
                 ['--seed', '1', '--seed', '2']]
        for args in cases:
            with self.subTest(args=args):
                # The explicit missing-output case must stay missing.
                actual = args if args == ['--seed', '1'] or not args else [*args, '--output', output]
                result = self.run_report(*actual)
                self.assertNotEqual(result.returncode, 0)
                self.assertIn('error:', result.stderr)
                self.assertNotIn('Recording', result.stdout)
                self.assertFalse(output.exists())
                self.assertFalse(Path(str(output) + '.trace.jsonl').exists())

    def test_existing_report_is_not_overwritten(self):
        output = self.root / 'existing.html'; output.write_text('user report')
        result = self.run_report('--seed', '1', '--agent', 'simple', '--output', output)
        self.assertNotEqual(result.returncode, 0)
        self.assertEqual(output.read_text(), 'user report')
        self.assertFalse(Path(str(output) + '.trace.jsonl').exists())

    def test_existing_trace_is_not_overwritten(self):
        output = self.root / 'existing.html'; trace = Path(str(output) + '.trace.jsonl'); trace.write_text('user trace')
        result = self.run_report('--seed', '1', '--agent', 'simple', '--output', output)
        self.assertNotEqual(result.returncode, 0)
        self.assertEqual(trace.read_text(), 'user trace'); self.assertFalse(output.exists())

    def test_bad_destination_and_manifest_fail_preflight(self):
        for args, output in [([], self.root / 'missing-dir/report.html'),
                             (['--assets', self.root / 'bad-manifest.json'], self.root / 'bad-assets.html')]:
            (self.root / 'bad-manifest.json').write_text('{bad json')
            result = self.run_report('--seed', '1', '--agent', 'simple', '--output', output, *args)
            self.assertNotEqual(result.returncode, 0)
            self.assertNotIn('Recording', result.stdout)
            self.assertFalse(Path(str(output) + '.trace.jsonl').exists())

    def test_help_does_not_run_simulation(self):
        result = self.run_report('--help')
        self.assertEqual(result.returncode, 0)
        self.assertIn('--seed', result.stdout)
        self.assertNotIn('Recording', result.stdout)

    def test_write_failure_preserves_complete_trace_lines(self):
        output = self.root / 'limited.html'
        def limit_output():
            signal.signal(signal.SIGXFSZ, signal.SIG_IGN)
            resource.setrlimit(resource.RLIMIT_FSIZE, (65536, 65536))
        result = subprocess.run([str(BINARY), '--seed', '1', '--agent', 'simple', '--output', str(output)],
                                capture_output=True, text=True, timeout=30, preexec_fn=limit_output)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn('error:', result.stderr)
        self.assertNotIn('Report written:', result.stdout)
        trace = Path(str(output) + '.trace.jsonl')
        self.assertTrue(trace.exists())
        complete_lines = trace.read_bytes().split(b'\n')[:-1]
        self.assertGreater(len(complete_lines), 2)
        records = [json.loads(line) for line in complete_lines]
        self.assertEqual(records[0]['type'], 'header')
        self.assertTrue(any(record['type'] == 'frame' for record in records))
        if output.exists():
            self.assertEqual(embedded_report(output)['status'], 'interrupted')

    def test_fatal_termination_preserves_initial_trace(self):
        output = self.root / 'killed.html'
        child = subprocess.Popen([str(BINARY), '--seed', '1', '--agent', 'search', '--simulations', '1000000', '--output', str(output)],
                                 stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
        try:
            self.assertIn('Recording', child.stdout.readline())
            child.kill(); child.communicate(timeout=10)
            self.assertNotEqual(child.returncode, 0)
            self.assertFalse(output.exists())
            lines = Path(str(output) + '.trace.jsonl').read_bytes().split(b'\n')[:-1]
            self.assertGreaterEqual(len(lines), 1)
            self.assertEqual(json.loads(lines[0])['type'], 'header')
            for line in lines:
                self.assertIsInstance(json.loads(line), dict)
        finally:
            if child.poll() is None:
                child.kill(); child.communicate()


if __name__ == '__main__':
    unittest.main()

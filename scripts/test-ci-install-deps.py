#!/usr/bin/env python3
"""Exercise mirror fallback offline: curl, sudo and timeout are shell stubs."""
import os
from pathlib import Path
import subprocess
import unittest

SCRIPT = Path(__file__).with_name('ci-install-deps.sh').resolve().as_posix()


class DependencySetupTests(unittest.TestCase):
    def run_case(self, mode='success', arch='amd64'):
        shell = r'''
source "$SCRIPT"
curl() {
    echo "PROBE $*"
    case "$MODE" in
        offline) return 1 ;;
        fallback) [[ "$*" != *archive.ubuntu.com* ]] ;;
        *) return 0 ;;
    esac
}
timeout() { echo "BOUND $1"; shift; "$@"; }
sudo() {
    echo "APT $*"
    local arg sources=
    for arg in "$@"; do
        if [[ "$arg" == Dir::Etc::sourcelist=* ]]; then sources=${arg#*=}; fi
    done
    cat "$sources"
    if [[ "$MODE" == fail-update && "$*" == *update ]]; then return 1; fi
    if [[ "$MODE" == fail-install && "$*" == *install* ]] &&
        grep -q archive.ubuntu.com "$sources"; then return 1; fi
    return 0
}
install_ci_dependencies noble "$ARCH" cmake ninja-build
'''
        env = dict(os.environ, SCRIPT=SCRIPT, MODE=mode, ARCH=arch)
        return subprocess.run([os.environ.get('CI_TEST_BASH', 'bash'), '-s'],
                              input=shell, text=True, capture_output=True, env=env, timeout=15)

    def test_primary_success(self):
        result = self.run_case()
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(result.stdout.count('PROBE '), 1)
        self.assertIn('signed-by=/usr/share/keyrings/ubuntu-archive-keyring.gpg', result.stdout)
        self.assertIn('APT::Update::Error-Mode=any', result.stdout)
        self.assertIn('BOUND 60s', result.stdout)
        self.assertIn('BOUND 180s', result.stdout)

    def test_probe_failure_uses_fallback(self):
        result = self.run_case('fallback')
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn('https://mirrors.edge.kernel.org/ubuntu noble', result.stdout)
        self.assertEqual(result.stdout.count('PROBE '), 2)

    def test_install_failure_uses_fallback(self):
        result = self.run_case('fail-install')
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(result.stdout.count('PROBE '), 2)

    def test_offline_stops_before_apt(self):
        result = self.run_case('offline')
        self.assertNotEqual(result.returncode, 0)
        self.assertNotIn('APT ', result.stdout)
        self.assertEqual(result.stdout.count('PROBE '), 2)

    def test_failed_update_never_installs(self):
        result = self.run_case('fail-update')
        self.assertNotEqual(result.returncode, 0)
        self.assertNotIn('install -y', result.stdout)

    def test_arm_sources(self):
        result = self.run_case(arch='arm64')
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn('arch=arm64', result.stdout)
        self.assertIn('https://ports.ubuntu.com/ubuntu-ports noble-security', result.stdout)

    def test_unknown_architecture_is_rejected(self):
        result = self.run_case(arch='unknown')
        self.assertNotEqual(result.returncode, 0)
        self.assertNotIn('PROBE ', result.stdout)


if __name__ == '__main__':
    unittest.main()

#!/usr/bin/env python3
"""Run AAX Validator dsh tests against one or more AAX bundles."""

import argparse
import os
from pathlib import Path
import signal
import subprocess
import sys
import time


MAX_PRINTED_LINES = 250


def print_limited_output(output: str) -> None:
    lines = output.splitlines()
    if len(lines) > MAX_PRINTED_LINES:
        omitted = len(lines) - MAX_PRINTED_LINES
        print(f"... omitted {omitted} earlier validator output lines ...")
        lines = lines[-MAX_PRINTED_LINES:]

    for line in lines:
        print(line)


def terminate_process_group(process: subprocess.Popen[str]) -> None:
    try:
        os.killpg(process.pid, signal.SIGTERM)
    except ProcessLookupError:
        return

    deadline = time.time() + 5
    while process.poll() is None and time.time() < deadline:
        time.sleep(0.1)

    if process.poll() is None:
        try:
            os.killpg(process.pid, signal.SIGKILL)
        except ProcessLookupError:
            pass


def validate_plugin(validator: Path, plugin: Path, timeout: int) -> int:
    if not plugin.is_dir():
        print(f"ERROR: AAX plugin not found: {plugin}", file=sys.stderr)
        return 1

    print()
    print(f"--- Validating {plugin} ---")

    command_input = (
        "load_dish aaxval\n"
        f'runtests "{plugin.resolve()}"\n'
        "exit\n"
    )

    process = subprocess.Popen(
        [str(validator)],
        stdin=subprocess.PIPE,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
        errors="replace",
        start_new_session=True,
    )

    try:
        output, _ = process.communicate(command_input, timeout=timeout)
    except subprocess.TimeoutExpired:
        terminate_process_group(process)
        output, _ = process.communicate()
        print_limited_output(output)
        print(f"ERROR: AAX validation timed out after {timeout} seconds: {plugin}", file=sys.stderr)
        return 124

    print_limited_output(output)
    if process.returncode != 0:
        print(f"ERROR: AAX validation failed with exit code {process.returncode}: {plugin}", file=sys.stderr)
        return process.returncode

    print(f"AAX validation passed: {plugin}")
    return 0


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--validator", required=True, help="Path to PACE AAX Validator CommandLineTools/dsh")
    parser.add_argument("--timeout", type=int, default=300, help="Timeout per plugin in seconds")
    parser.add_argument("plugins", nargs="+", help="AAX plugin bundles to validate")
    args = parser.parse_args()

    validator = Path(args.validator)
    if not validator.is_file():
        print(f"ERROR: AAX validator not found: {validator}", file=sys.stderr)
        return 1

    failed = False
    for plugin_arg in args.plugins:
        status = validate_plugin(validator, Path(plugin_arg), args.timeout)
        failed = failed or status != 0

    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())

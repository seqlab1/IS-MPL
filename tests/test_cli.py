#!/usr/bin/env python3
"""Small independent correctness checks for the standalone IS+MPL CLI."""

from __future__ import annotations

import itertools
import re
import subprocess
import sys
import tempfile
from pathlib import Path


def is_subsequence(candidate: str, sequence: str) -> bool:
    iterator = iter(sequence)
    return all(any(symbol == item for item in iterator) for symbol in candidate)


def brute_force_mlcs(sequences: list[str]) -> set[str]:
    shortest = min(sequences, key=len)
    for length in range(len(shortest), -1, -1):
        answers: set[str] = set()
        candidates: set[str] = set()
        for positions in itertools.combinations(range(len(shortest)), length):
            candidate = "".join(shortest[position] for position in positions)
            if candidate in candidates:
                continue
            candidates.add(candidate)
            if all(is_subsequence(candidate, sequence) for sequence in sequences):
                answers.add(candidate)
        if answers:
            return answers
    raise AssertionError("unreachable")


def parse_result_sets(output: str) -> list[set[str]]:
    lines = output.splitlines()
    results: list[set[str]] = []
    index = 0
    while index < len(lines):
        match = re.fullmatch(r"Results Count: (\d+)", lines[index])
        if match:
            count = int(match.group(1))
            values = lines[index + 1:index + 1 + count]
            if len(values) != count:
                raise AssertionError("truncated result list")
            results.append(set(values))
            index += count
        index += 1
    return results


def run_case(executable: Path, initial: list[str], additions: list[str]) -> None:
    with tempfile.TemporaryDirectory(prefix="is_mpl_test_") as directory:
        root = Path(directory)
        initial_path = root / "initial.txt"
        additions_path = root / "additions.txt"
        initial_path.write_text("\n".join(initial) + "\n", encoding="ascii")
        additions_path.write_text("\n".join(additions) + "\n", encoding="ascii")
        process = subprocess.run(
            [
                str(executable),
                "--mode", "auto",
                "--input", str(initial_path),
                "--add", str(additions_path),
                "--measure", "include-backtrack",
                "--max-results", "1000000",
                "--print-results",
            ],
            check=True,
            text=True,
            encoding="utf-8",
            errors="replace",
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
        )

    lengths = [
        int(value)
        for value in re.findall(r"After Add #\d+ MLCS Length: (\d+)", process.stdout)
    ]
    expected_sets = [
        brute_force_mlcs(initial + additions[:step])
        for step in range(1, len(additions) + 1)
    ]
    expected_lengths = [len(next(iter(values))) for values in expected_sets]
    actual_sets = parse_result_sets(process.stdout)
    if lengths != expected_lengths:
        raise AssertionError(f"lengths={lengths}, expected={expected_lengths}")
    if actual_sets != expected_sets:
        raise AssertionError(f"results={actual_sets}, expected={expected_sets}")


def main() -> None:
    if len(sys.argv) != 2:
        raise SystemExit(f"usage: {sys.argv[0]} PATH_TO_IS_MPL")
    executable = Path(sys.argv[1]).resolve()
    if not executable.is_file():
        raise SystemExit(f"executable not found: {executable}")

    cases = [
        (["ACGT", "AGCT"], ["ACT", "GACT"]),
        (["AAAA", "AA"], ["AAA", "A"]),
        (["ACAC", "CACA"], ["AACC", "CCAA"]),
        (["GATTACA", "TACTAGA"], ["ATACGA", "GTACAA"]),
    ]
    for initial, additions in cases:
        run_case(executable, initial, additions)
    print(f"PASS: {len(cases)} standalone IS+MPL cases")


if __name__ == "__main__":
    main()

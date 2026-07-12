#!/usr/bin/env python3

import argparse
import json
import re
import statistics
from dataclasses import asdict, dataclass
from pathlib import Path
from typing import Iterable


_PAGE_RE = re.compile(r"\bprewarm=(\d+)ms\b.*\bbw_render=(\d+)ms\b")
_OVERFLOW_RE = re.compile(r"\boverflow_reads=(\d+)\b")
_MAX_ALLOC_RE = re.compile(r"\bmax_alloc=(\d+)\b")


@dataclass(frozen=True)
class ProfileResult:
    control_median_ms: float
    candidate_median_ms: float
    overhead_percent: float
    overflow_reads: int
    minimum_max_alloc: int
    passed: bool


def _samples(lines: Iterable[str]) -> list[tuple[int, int, int]]:
    samples: list[tuple[int, int, int]] = []
    for line in lines:
        page_match = _PAGE_RE.search(line)
        if not page_match:
            continue
        cpu_ms = int(page_match.group(1)) + int(page_match.group(2))
        overflow_match = _OVERFLOW_RE.search(line)
        max_alloc_match = _MAX_ALLOC_RE.search(line)
        overflow_reads = int(overflow_match.group(1)) if overflow_match else 0
        max_alloc = int(max_alloc_match.group(1)) if max_alloc_match else 2**31 - 1
        samples.append((cpu_ms, overflow_reads, max_alloc))
    return samples


def compare_runs(
    control_lines: Iterable[str],
    candidate_lines: Iterable[str],
    *,
    max_overhead_percent: float,
    min_max_alloc: int = 0,
) -> ProfileResult:
    control = _samples(control_lines)
    candidate = _samples(candidate_lines)
    if not control or not candidate:
        raise ValueError("no page render samples in one or both logs")

    control_median = statistics.median(sample[0] for sample in control)
    candidate_median = statistics.median(sample[0] for sample in candidate)
    if control_median <= 0:
        raise ValueError("control median must be positive")

    overhead = (candidate_median - control_median) * 100.0 / control_median
    overflow_reads = sum(sample[1] for sample in candidate)
    minimum_max_alloc = min(sample[2] for sample in candidate)
    passed = overhead <= max_overhead_percent and overflow_reads == 0 and minimum_max_alloc >= min_max_alloc
    return ProfileResult(
        control_median_ms=control_median,
        candidate_median_ms=candidate_median,
        overhead_percent=overhead,
        overflow_reads=overflow_reads,
        minimum_max_alloc=minimum_max_alloc,
        passed=passed,
    )


def main() -> int:
    parser = argparse.ArgumentParser(description="Compare CrossInk mixed-font page-render profiles")
    parser.add_argument("control", type=Path)
    parser.add_argument("candidate", type=Path)
    parser.add_argument("--max-overhead-percent", type=float, required=True)
    parser.add_argument("--min-max-alloc", type=int, default=0)
    args = parser.parse_args()

    result = compare_runs(
        args.control.read_text(encoding="utf-8").splitlines(),
        args.candidate.read_text(encoding="utf-8").splitlines(),
        max_overhead_percent=args.max_overhead_percent,
        min_max_alloc=args.min_max_alloc,
    )
    print(json.dumps(asdict(result), indent=2, sort_keys=True))
    return 0 if result.passed else 1


if __name__ == "__main__":
    raise SystemExit(main())

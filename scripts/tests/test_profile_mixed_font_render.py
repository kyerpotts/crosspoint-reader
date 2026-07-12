import unittest

from scripts.profile_mixed_font_render import compare_runs


def render_line(prewarm: int, bw_render: int, *, overflow_reads: int = 0, max_alloc: int = 50000) -> str:
    return (
        f"Page render: prewarm={prewarm}ms bw_render={bw_render}ms display=900ms total=1200ms "
        f"overflow_reads={overflow_reads} max_alloc={max_alloc}"
    )


class MixedFontProfileTest(unittest.TestCase):
    def test_mixed_page_accepts_ten_percent_boundary(self):
        control = [render_line(400, 600) for _ in range(5)]
        candidate = [render_line(450, 650) for _ in range(5)]

        result = compare_runs(control, candidate, max_overhead_percent=10.0)

        self.assertEqual(result.control_median_ms, 1000)
        self.assertEqual(result.candidate_median_ms, 1100)
        self.assertAlmostEqual(result.overhead_percent, 10.0)
        self.assertTrue(result.passed)

    def test_mixed_page_rejects_more_than_ten_percent(self):
        control = [render_line(400, 600) for _ in range(5)]
        candidate = [render_line(451, 650) for _ in range(5)]

        result = compare_runs(control, candidate, max_overhead_percent=10.0)

        self.assertGreater(result.overhead_percent, 10.0)
        self.assertFalse(result.passed)

    def test_prose_page_rejects_more_than_two_percent(self):
        control = [render_line(200, 800) for _ in range(5)]
        candidate = [render_line(221, 800) for _ in range(5)]

        result = compare_runs(control, candidate, max_overhead_percent=2.0)

        self.assertGreater(result.overhead_percent, 2.0)
        self.assertFalse(result.passed)

    def test_any_overflow_read_fails(self):
        control = [render_line(200, 800) for _ in range(5)]
        candidate = [render_line(200, 800, overflow_reads=1) for _ in range(5)]

        result = compare_runs(control, candidate, max_overhead_percent=10.0)

        self.assertEqual(result.overflow_reads, 5)
        self.assertFalse(result.passed)

    def test_largest_allocation_floor_is_enforced(self):
        control = [render_line(200, 800) for _ in range(5)]
        candidate = [render_line(200, 800, max_alloc=32767) for _ in range(5)]

        result = compare_runs(control, candidate, max_overhead_percent=10.0, min_max_alloc=32768)

        self.assertEqual(result.minimum_max_alloc, 32767)
        self.assertFalse(result.passed)

    def test_invalid_or_empty_logs_raise(self):
        with self.assertRaisesRegex(ValueError, "no page render samples"):
            compare_runs([], [], max_overhead_percent=10.0)
        with self.assertRaisesRegex(ValueError, "no page render samples"):
            compare_runs(["unrelated"], ["unrelated"], max_overhead_percent=10.0)


if __name__ == "__main__":
    unittest.main()

#include "check.h"
#include "core/bench_stats.h"

#include <cmath>
#include <limits>
#include <string>
#include <vector>

// Unit tests for the benchmark statistics (ticket 0013): order statistics and
// the CSV row the `--bench` entry point publishes. Every expected value below
// is derived by hand from the sample, so the test pins the definitions rather
// than whatever the implementation happens to compute. Pure CPU — no GL.

namespace {

void checkSummary(const bench::Summary& s, double median, double p95,
                  double mean, double min, double fps) {
    CHECK_CLOSE(s.median_ms, median, 1e-9);
    CHECK_CLOSE(s.p95_ms, p95, 1e-9);
    CHECK_CLOSE(s.mean_ms, mean, 1e-9);
    CHECK_CLOSE(s.min_ms, min, 1e-9);
    CHECK_CLOSE(s.fps, fps, 1e-6);
}

// Number of CSV fields in one line, counting separators outside quotes only —
// a naive comma count would see the comma inside a quoted renderer name.
std::size_t fields(const std::string& line) {
    std::size_t n = 1;
    bool inQuotes = false;
    for (char c : line) {
        if (c == '"') {
            inQuotes = !inQuotes;
        } else if (c == ',' && !inQuotes) {
            ++n;
        }
    }
    CHECK(!inQuotes);  // an unterminated quote corrupts every following line
    return n;
}

} // namespace

int main() {
    // ── Odd sample count: median is the middle value ─────────────────────────
    // {4, 1, 3, 2, 10} sorted = {1, 2, 3, 4, 10}
    //   median = 3, p95 = nearest-rank ceil(0.95·5) = 5th = 10,
    //   mean = 20/5 = 4, min = 1, fps = 1000/3
    checkSummary(bench::summarize({4.0, 1.0, 3.0, 2.0, 10.0}),
                 3.0, 10.0, 4.0, 1.0, 1000.0 / 3.0);

    // ── Even sample count: median is the mean of the two middle values ───────
    // {1, 2, 3, 4} sorted = {1, 2, 3, 4}
    //   median = (2+3)/2 = 2.5, p95 = ceil(0.95·4) = 4th = 4,
    //   mean = 2.5, min = 1, fps = 1000/2.5 = 400
    checkSummary(bench::summarize({1.0, 2.0, 3.0, 4.0}),
                 2.5, 4.0, 2.5, 1.0, 400.0);

    // ── Single sample: every statistic collapses onto it ─────────────────────
    checkSummary(bench::summarize({7.0}), 7.0, 7.0, 7.0, 7.0, 1000.0 / 7.0);

    // ── p95 of a large sample is a tail value, not the max or the mean ───────
    // 1..100 sorted; median = (50+51)/2 = 50.5;
    // p95 = ceil(0.95·100) = 95th value = 95; min = 1; mean = 50.5.
    std::vector<double> hundred;
    for (int i = 1; i <= 100; ++i) hundred.push_back(static_cast<double>(i));
    checkSummary(bench::summarize(hundred), 50.5, 95.0, 50.5, 1.0,
                 1000.0 / 50.5);

    // ── Empty input is zeroed, never a division by zero ──────────────────────
    checkSummary(bench::summarize({}), 0.0, 0.0, 0.0, 0.0, 0.0);

    // ── Non-positive and non-finite samples are measurement noise ────────────
    // {0, -1, 2.5} → usable {2.5}: a zero/negative GPU time cannot be a frame.
    checkSummary(bench::summarize({0.0, -1.0, 2.5}), 2.5, 2.5, 2.5, 2.5, 400.0);
    // {NaN, 1, inf, 2} → usable {1, 2}: median 1.5, p95 = 2nd = 2, min = 1.
    checkSummary(bench::summarize({std::nan(""), 1.0,
                                   std::numeric_limits<double>::infinity(), 2.0}),
                 1.5, 2.0, 1.5, 1.0, 1000.0 / 1.5);
    // Nothing usable → zeroed, not a crash.
    checkSummary(bench::summarize({0.0, std::nan("")}), 0.0, 0.0, 0.0, 0.0, 0.0);

    // ── CSV contract ─────────────────────────────────────────────────────────
    const std::string header = bench::csvHeader();
    CHECK(header ==
          "median_ms,p95_ms,mean_ms,min_ms,fps,renderer,steps_per_ray,"
          "stats_median_ms,stats_overhead_pct,width,height,max_steps,"
          "jet_enabled,disk_enabled,frames,warmup");
    // The header must parse to exactly 16 fields.
    CHECK(fields(header) == 16);
    // The same literal is pinned by bench/test_gate.py: the header lives on
    // both sides of the C++/Python seam, so a rename on either side must fail
    // the other side's test instead of silently re-aligning every column.
    CHECK(header ==
          "median_ms,p95_ms,mean_ms,min_ms,fps,renderer,steps_per_ray,"
          "stats_median_ms,stats_overhead_pct,width,height,max_steps,"
          "jet_enabled,disk_enabled,frames,warmup");

    bench::Frame frame;
    frame.summary            = bench::summarize({2.0, 4.0});
    frame.renderer           = "D3D12 (NVIDIA GeForce RTX 4070)";
    frame.steps_per_ray      = 812.5;
    frame.stats_median_ms    = 4.22;
    frame.stats_overhead_pct = 11.0;
    frame.width              = 512;
    frame.height             = 512;
    frame.max_steps          = 5000;
    frame.jet_enabled        = true;
    frame.disk_enabled       = false;
    frame.frames             = 20;
    frame.warmup             = 5;

    const std::string row = bench::csvRow(frame);
    // Same field count as the header — a row that drifts from the header is
    // worse than no row, because it parses silently into the wrong columns.
    CHECK(fields(row) == fields(header));
    CHECK(row.rfind("3,", 0) == 0);          // median of {2, 4}
    CHECK(row.find("D3D12 (NVIDIA GeForce RTX 4070)") != std::string::npos);
    CHECK(row.find(",812.5,4.22,11,") != std::string::npos);
    CHECK(row.find(",512,512,5000,1,0,20,5") != std::string::npos);

    // Booleans publish as 1/0, not "true"/"false" — a CSV consumer must not
    // need to know the emitter's spelling.
    CHECK(row.find(",1,0,20,5") != std::string::npos);

    // A renderer name that carries a comma (they do exist) must reach the row
    // already quoted, or the row silently gains a column and every later field
    // shifts left.
    bench::Frame commaFrame;
    commaFrame.renderer = "GPU, Inc.";
    commaFrame.width    = 512;
    const std::string commaRow = bench::csvRow(commaFrame);
    CHECK(commaRow.find("\"GPU, Inc.\",") != std::string::npos);
    CHECK(fields(commaRow) == fields(header));

    // A renderer string carrying a comma or a quote must be quoted with its
    // quotes doubled, otherwise the row silently gains a column.
    CHECK(bench::csvQuote("plain") == "plain");
    CHECK(bench::csvQuote("a,b") == "\"a,b\"");
    CHECK(bench::csvQuote("say \"hi\"") == "\"say \"\"hi\"\"\"");
    CHECK(bench::csvQuote("multi\nline") == "\"multi\nline\"");

    return check::report("test_bench_stats");
}

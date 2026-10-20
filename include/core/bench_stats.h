#ifndef BENCH_STATS_H
#define BENCH_STATS_H

#include <string>
#include <vector>

// ── Benchmark statistics (ticket 0013, spec/0002 "Performance protocol") ──────
// Summarises a sample of GPU frame times and formats the CSV row that
// `--bench` publishes. Deliberately GL-free so it lives in blackhole_core and
// is testable without a context (ADR-0005); the measurement itself is the
// only part that needs a runtime.

namespace bench {

// Order statistics of a frame-time sample, all in milliseconds.
struct Summary {
    double median_ms = 0.0;  // primary number: the gate compares this
    double p95_ms    = 0.0;  // tail, per the standard's "never a lone mean"
    double mean_ms   = 0.0;
    double min_ms    = 0.0;
    double fps       = 0.0;  // frames/second implied by the median
};

// Summarise `samplesMs`.
//
// Non-finite and non-positive samples are dropped: a NaN/Inf or a zero/negative
// duration is a broken timer read, not a frame time, and letting one into the
// median would silently corrupt the baseline. If nothing usable remains the
// result is zeroed — the caller (and therefore the gate) sees median 0 and
// refuses to publish a number rather than dividing by zero.
Summary summarize(const std::vector<double>& samplesMs);

// One published benchmark row: the protocol's statistics plus the environment
// and the configuration matrix axes the ticket asks to keep reportable.
struct Frame {
    Summary     summary;
    std::string renderer;             // GL_RENDERER, quoted by csvRow
    double      steps_per_ray      = 0.0;  // derived diagnostic (stats pass)
    double      stats_median_ms    = 0.0;  // same workload with counting on
    double      stats_overhead_pct = 0.0;  // cost of the per-step atomic
    int         width              = 0;
    int         height             = 0;
    int         max_steps          = 0;
    bool        jet_enabled        = false;
    bool        disk_enabled       = false;
    int         frames             = 0;
    int         warmup             = 0;
};

// CSV header line, field order fixed to match csvRow().
std::string csvHeader();

// CSV data row for `frame`, in csvHeader()'s field order. The row always has
// the same number of fields as the header — a row that drifts from the header
// is worse than no row, because it parses silently into the wrong columns.
std::string csvRow(const Frame& frame);

// RFC 4180 quoting: leave a plain field alone, otherwise wrap it in double
// quotes and double any embedded quote. Also used for multi-line fields.
std::string csvQuote(const std::string& field);

}  // namespace bench

#endif  // BENCH_STATS_H

#include "core/bench_stats.h"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>

namespace bench {
namespace {

// Six significant digits: enough to see a ±10 % gate breach (1 % ≫ 1e-6 of the
// value) without dumping float noise into a committed baseline, and short
// enough that a row stays readable. Non-finite values would print as "nan"/
// "inf" — those are filtered out of samples, and the fields below are guarded.
std::string format(double value) {
    if (!std::isfinite(value)) {
        return "0";
    }
    std::ostringstream out;
    out << std::setprecision(6) << value;
    return out.str();
}

// Nearest-rank percentile on a sorted sample: the value at ceil(p·n), 1-based.
double percentile(const std::vector<double>& sorted, double p) {
    if (sorted.empty()) return 0.0;
    auto rank = static_cast<std::size_t>(std::ceil(p * static_cast<double>(sorted.size())));
    rank = std::max<std::size_t>(1, std::min(rank, sorted.size()));
    return sorted[rank - 1];
}

}  // namespace

Summary summarize(const std::vector<double>& samplesMs) {
    std::vector<double> sorted;
    sorted.reserve(samplesMs.size());
    for (double value : samplesMs) {
        if (std::isfinite(value) && value > 0.0) {
            sorted.push_back(value);
        }
    }

    Summary summary;
    if (sorted.empty()) {
        return summary;  // zeroed — never divide by zero
    }

    std::sort(sorted.begin(), sorted.end());

    const std::size_t n = sorted.size();
    if (n % 2 == 1) {
        summary.median_ms = sorted[n / 2];
    } else {
        summary.median_ms = 0.5 * (sorted[n / 2 - 1] + sorted[n / 2]);
    }

    summary.p95_ms = percentile(sorted, 0.95);
    summary.min_ms = sorted.front();

    double sum = 0.0;
    for (double value : sorted) sum += value;
    summary.mean_ms = sum / static_cast<double>(n);

    summary.fps = summary.median_ms > 0.0 ? 1000.0 / summary.median_ms : 0.0;
    return summary;
}

std::string csvQuote(const std::string& field) {
    const bool needsQuotes =
        field.find_first_of(",\"\r\n") != std::string::npos;
    if (!needsQuotes) {
        return field;
    }
    std::string escaped;
    escaped.reserve(field.size() + 2);
    for (char c : field) {
        if (c == '"') escaped += '"';
        escaped += c;
    }
    return "\"" + escaped + "\"";
}

std::string csvHeader() {
    return "median_ms,p95_ms,mean_ms,min_ms,fps,renderer,steps_per_ray,"
           "stats_median_ms,stats_overhead_pct,width,height,max_steps,"
           "jet_enabled,disk_enabled,frames,warmup";
}

std::string csvRow(const Frame& frame) {
    std::ostringstream row;
    row << format(frame.summary.median_ms) << ','
        << format(frame.summary.p95_ms) << ','
        << format(frame.summary.mean_ms) << ','
        << format(frame.summary.min_ms) << ','
        << format(frame.summary.fps) << ','
        << csvQuote(frame.renderer) << ','
        << format(frame.steps_per_ray) << ','
        << format(frame.stats_median_ms) << ','
        << format(frame.stats_overhead_pct) << ','
        << frame.width << ','
        << frame.height << ','
        << frame.max_steps << ','
        << (frame.jet_enabled ? 1 : 0) << ','
        << (frame.disk_enabled ? 1 : 0) << ','
        << frame.frames << ','
        << frame.warmup;
    return row.str();
}

}  // namespace bench

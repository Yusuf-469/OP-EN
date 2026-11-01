#include "report_writers.h"

#include <cmath>
#include <cstdio>

namespace uew {
namespace {

// Full double precision so exported CSVs reproduce the solver output exactly;
// non-finite values become an empty cell rather than "nan"/"inf".
std::string num(double v) {
    if (!std::isfinite(v)) return "";
    char buffer[40];
    std::snprintf(buffer, sizeof buffer, "%.17g", v);
    return buffer;
}

std::string csvRow(std::initializer_list<std::string> cells) {
    std::string row;
    for (const auto& c : cells) row += (row.empty() ? "" : ",") + c;
    return row + "\n";
}

// Undefined (NaN) when the errors are at round-off level, e.g. q = 0 where linear
// elements are exact everywhere and the "order" would be noise.
double observedOrder(const StudyEntry& coarse, const StudyEntry& fine) {
    constexpr double kRoundOffC = 1e-10;
    if (coarse.error.max_midpoint_error_C < kRoundOffC || fine.error.max_midpoint_error_C < kRoundOffC) return NAN;
    const double ratio = static_cast<double>(fine.problem.elements) / coarse.problem.elements;
    return std::log(coarse.error.max_midpoint_error_C / fine.error.max_midpoint_error_C) / std::log(ratio);
}

void writeVec3(JsonWriter& json, const char* name, const Vec3& v) {
    json.key(name).beginArray().value(v[0]).value(v[1]).value(v[2]).endArray();
}

void writeParams(JsonWriter& json, const char* name, const Params& p) {
    json.key(name).beginObject().field("w", p.w).field("b", p.b).field("v", p.v).field("c", p.c).endObject();
}

void writeGradients(JsonWriter& json, const char* name, const Gradients& g) {
    json.key(name).beginObject().field("w", g.dw).field("b", g.db).field("v", g.dv).field("c", g.dc).endObject();
}

}  // namespace

std::vector<StudyEntry> runThermalStudy(const RodProblem& base, const std::vector<int>& meshes) {
    std::vector<StudyEntry> study;
    for (int n : meshes) {
        StudyEntry entry{base, {}, {}};
        entry.problem.elements = n;
        entry.solution = solveRod(entry.problem);
        entry.error = measureError(entry.problem, entry.solution);
        study.push_back(std::move(entry));
    }
    return study;
}

std::string thermalNodesCsv(const std::vector<StudyEntry>& study) {
    std::string csv = "N,node,x_m,T_fe_C,T_exact_C,abs_error_C\n";
    for (const auto& s : study) {
        for (std::size_t i = 0; i < s.solution.x_m.size(); ++i) {
            const double x = s.solution.x_m[i], fe = s.solution.temperature_C[i];
            const double exact = exactTemperature(s.problem, x);
            csv += csvRow({std::to_string(s.problem.elements), std::to_string(i), num(x), num(fe), num(exact),
                           num(std::abs(fe - exact))});
        }
    }
    return csv;
}

std::string thermalMidpointsCsv(const std::vector<StudyEntry>& study) {
    std::string csv = "N,element,x_mid_m,T_fe_C,T_exact_C,abs_error_C\n";
    for (const auto& s : study) {
        const auto& x = s.solution.x_m;
        const auto& T = s.solution.temperature_C;
        for (std::size_t e = 0; e + 1 < x.size(); ++e) {
            const double xm = 0.5 * (x[e] + x[e + 1]), fe = 0.5 * (T[e] + T[e + 1]);
            const double exact = exactTemperature(s.problem, xm);
            csv += csvRow({std::to_string(s.problem.elements), std::to_string(e + 1), num(xm), num(fe), num(exact),
                           num(std::abs(fe - exact))});
        }
    }
    return csv;
}

std::string errorProfileCsv(const std::vector<StudyEntry>& study, int samplesPerElement) {
    std::string csv = "N,x_m,T_fe_minus_exact_C\n";
    for (const auto& s : study) {
        const int samples = s.problem.elements * samplesPerElement;
        for (int i = 0; i <= samples; ++i) {
            const double x = s.problem.length_m * i / samples;
            csv += csvRow({std::to_string(s.problem.elements), num(x),
                           num(interpolate(s.solution, x) - exactTemperature(s.problem, x))});
        }
    }
    return csv;
}

std::string convergenceCsv(const std::vector<StudyEntry>& study) {
    std::string csv = "N,h_m,max_nodal_error_C,max_midpoint_error_C,observed_order\n";
    for (std::size_t i = 0; i < study.size(); ++i) {
        const auto& s = study[i];
        csv += csvRow({std::to_string(s.problem.elements), num(s.problem.length_m / s.problem.elements),
                       num(s.error.max_nodal_error_C), num(s.error.max_midpoint_error_C),
                       i ? num(observedOrder(study[i - 1], s)) : ""});
    }
    return csv;
}

void writeThermalStudy(JsonWriter& json, const std::vector<StudyEntry>& study) {
    json.beginObject().key("meshes").beginArray();
    for (std::size_t i = 0; i < study.size(); ++i) {
        const auto& s = study[i];
        json.beginObject()
            .field("N", s.problem.elements)
            .field("h_m", s.problem.length_m / s.problem.elements)
            .field("max_nodal_error_C", s.error.max_nodal_error_C)
            .field("max_midpoint_error_C", s.error.max_midpoint_error_C)
            .field("observed_order", i ? observedOrder(study[i - 1], s) : NAN)
            .field("heat_out_left_W", s.solution.heat_out_left_W)
            .field("heat_out_right_W", s.solution.heat_out_right_W);
        json.key("x_m").beginArray();
        for (double x : s.solution.x_m) json.value(x);
        json.endArray().key("T_C").beginArray();
        for (double t : s.solution.temperature_C) json.value(t);
        json.endArray();
        // Pointwise error T_FE(x) - T_exact(x), sampled inside every element: zero
        // at the nodes, largest at the midpoints.
        constexpr int kSamplesPerElement = 8;
        const double h = s.problem.length_m / s.problem.elements;
        json.key("error_x_m").beginArray();
        for (int i = 0; i <= s.problem.elements * kSamplesPerElement; ++i) json.value(i * h / kSamplesPerElement);
        json.endArray().key("error_C").beginArray();
        for (int i = 0; i <= s.problem.elements * kSamplesPerElement; ++i) {
            const double x = i * h / kSamplesPerElement;
            json.value(interpolate(s.solution, x) - exactTemperature(s.problem, x));
        }
        json.endArray().endObject();
    }
    json.endArray();
    // Exact curve sampled finely for plotting.
    const RodProblem& p = study.front().problem;
    json.key("exact").beginObject().key("x_m").beginArray();
    for (int i = 0; i <= 200; ++i) json.value(p.length_m * i / 200.0);
    json.endArray().key("T_C").beginArray();
    for (int i = 0; i <= 200; ++i) json.value(exactTemperature(p, p.length_m * i / 200.0));
    json.endArray().endObject().endObject();
}

void writeSystemInfo(JsonWriter& json, const SystemInfo& info) {
    json.beginObject()
        .field("cpu_model", info.cpu_model)
        .field("physical_cores", static_cast<int>(info.physical_cores))
        .field("logical_cpus", static_cast<int>(info.logical_cpus))
        .field("ram_GiB", static_cast<double>(info.ram_bytes) / (1024.0 * 1024.0 * 1024.0))
        .field("os", info.os)
        .field("compiler", info.compiler)
        .field("build_flags", info.build_flags)
        .endObject();
}

void writeBenchmark(JsonWriter& json, const BenchmarkReport& r) {
    json.beginObject()
        .field("seed", static_cast<std::int64_t>(r.config.batch.seed))
        .field("cases", r.config.batch.cases)
        .field("elements_per_case", r.config.batch.elements)
        .field("repetitions", r.config.repetitions)
        .field("max_workers", r.config.max_workers)
        .field("logical_cpus", static_cast<int>(r.logical_cpus))
        .field("warmup_ms", r.warmup_ms)
        .field("agreement_tolerance_C", kAgreementToleranceC)
        .field("agreement_ok", r.agreement_ok)
        .key("rows").beginArray();
    for (const auto& row : r.rows) {
        json.beginObject()
            .field("variant", row.variant)
            .field("workers", row.workers)
            .field("median_ms", row.median_ms)
            .field("speedup", row.speedup)
            .field("efficiency", row.efficiency)
            .field("max_abs_diff_C", row.max_abs_diff_C)
            .key("samples_ms").beginArray();
        for (double s : row.samples_ms) json.value(s);
        json.endArray().endObject();
    }
    json.endArray().endObject();
}

std::string benchmarkSummaryCsv(const BenchmarkReport& r) {
    std::string csv = "variant,workers,median_ms,speedup_vs_serial,efficiency,max_abs_diff_vs_serial_C\n";
    for (const auto& row : r.rows)
        csv += csvRow({row.variant, std::to_string(row.workers), num(row.median_ms), num(row.speedup),
                       num(row.efficiency), num(row.max_abs_diff_C)});
    return csv;
}

std::string benchmarkSamplesCsv(const BenchmarkReport& r) {
    std::string csv = "variant,workers,repetition,wall_ms\n";
    for (const auto& row : r.rows)
        for (std::size_t i = 0; i < row.samples_ms.size(); ++i)
            csv += csvRow({row.variant, std::to_string(row.workers), std::to_string(i + 1), num(row.samples_ms[i])});
    return csv;
}

void writeMesh(JsonWriter& json, const TriangleMesh& mesh) {
    json.beginObject()
        .field("solid_name", mesh.solid_name)
        .field("triangles", mesh.triangles)
        .field("degenerate_triangles", mesh.degenerate_triangles)
        .field("surface_area", mesh.surface_area);
    writeVec3(json, "min", mesh.min);
    writeVec3(json, "max", mesh.max);
    json.key("vertices").beginArray();
    for (float v : mesh.vertices) json.value(static_cast<double>(v), 7);
    json.endArray().endObject();
}

void writeNodalResults(JsonWriter& json, const NodalResults& r) {
    json.beginObject()
        .field("count", r.nodes.size())
        .field("min_C", r.min_C)
        .field("max_C", r.max_C)
        .field("mean_C", r.mean_C);
    writeVec3(json, "min_m", r.min_m);
    writeVec3(json, "max_m", r.max_m);
    // Columnar arrays keep the payload compact for large point clouds.
    json.key("id").beginArray();
    for (const auto& n : r.nodes) json.value(n.id);
    json.endArray().key("xyz_m").beginArray();
    for (const auto& n : r.nodes) json.value(n.x_m, 9).value(n.y_m, 9).value(n.z_m, 9);
    json.endArray().key("T_C").beginArray();
    for (const auto& n : r.nodes) json.value(n.temperature_C, 9);
    json.endArray().endObject();
}

BackpropStudy runBackpropStudy() {
    BackpropStudy s;
    s.forward_pass = forward(s.initial, s.sample);
    s.analytic = backward(s.initial, s.sample);
    s.numeric = finiteDifference(s.initial, s.sample, s.epsilon);
    s.updated = gradientStep(s.initial, s.analytic, s.learning_rate);
    s.losses = lossCurve(s.initial, s.sample, s.learning_rate, 100);
    return s;
}

void writeBackprop(JsonWriter& json, const BackpropStudy& s) {
    const ForwardPass& f = s.forward_pass;
    json.beginObject().field("x", s.sample.x).field("y", s.sample.y)
        .field("learning_rate", s.learning_rate).field("epsilon", s.epsilon);
    writeParams(json, "initial", s.initial);
    json.key("forward").beginObject().field("z", f.z).field("h", f.h).field("y_hat", f.y_hat)
        .field("loss", f.loss).endObject();
    writeGradients(json, "analytic", s.analytic);
    writeGradients(json, "finite_difference", s.numeric);
    writeParams(json, "updated", s.updated);
    json.key("losses").beginArray();
    for (double l : s.losses) json.value(l);
    json.endArray().endObject();
}

std::string backpropLossCsv(const BackpropStudy& s) {
    std::string csv = "step,loss\n";
    for (std::size_t i = 0; i < s.losses.size(); ++i) csv += csvRow({std::to_string(i), num(s.losses[i])});
    return csv;
}

std::string backpropGradientCsv(const BackpropStudy& s) {
    std::string csv = "parameter,initial,analytic_gradient,central_difference,abs_error,updated\n";
    const struct { const char* name; double p, a, n, u; } rows[] = {
        {"w", s.initial.w, s.analytic.dw, s.numeric.dw, s.updated.w},
        {"b", s.initial.b, s.analytic.db, s.numeric.db, s.updated.b},
        {"v", s.initial.v, s.analytic.dv, s.numeric.dv, s.updated.v},
        {"c", s.initial.c, s.analytic.dc, s.numeric.dc, s.updated.c},
    };
    for (const auto& r : rows) csv += csvRow({r.name, num(r.p), num(r.a), num(r.n), num(std::abs(r.a - r.n)), num(r.u)});
    return csv;
}

}  // namespace uew

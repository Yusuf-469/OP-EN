#pragma once
// CSV / JSON serialisation of core results. Shared by the HTTP API and the CLI
// so that UI exports and files in results/ have exactly the same format.

#include <string>
#include <vector>

#include "backprop.h"
#include "batch.h"
#include "geometry_io.h"
#include "json_writer.h"
#include "system_info.h"
#include "thermal_fea.h"

namespace uew {

struct StudyEntry {
    RodProblem problem;
    RodSolution solution;
    ErrorMeasures error;
};

// Solves `base` once per mesh size in `meshes`.
std::vector<StudyEntry> runThermalStudy(const RodProblem& base, const std::vector<int>& meshes);

std::string thermalNodesCsv(const std::vector<StudyEntry>& study);      // N,node,x_m,T_fe_C,T_exact_C,abs_error_C
std::string thermalMidpointsCsv(const std::vector<StudyEntry>& study);  // N,element,x_mid_m,T_fe_C,T_exact_C,abs_error_C
std::string convergenceCsv(const std::vector<StudyEntry>& study);       // N,h_m,max errors,observed order

void writeThermalStudy(JsonWriter& json, const std::vector<StudyEntry>& study);
void writeSystemInfo(JsonWriter& json, const SystemInfo& info);
void writeBenchmark(JsonWriter& json, const BenchmarkReport& report);
std::string benchmarkSummaryCsv(const BenchmarkReport& report);
std::string benchmarkSamplesCsv(const BenchmarkReport& report);

void writeMesh(JsonWriter& json, const TriangleMesh& mesh);
void writeNodalResults(JsonWriter& json, const NodalResults& results);

struct BackpropStudy {
    Params initial;
    Sample sample;
    double learning_rate = 0.1;
    double epsilon = 1e-6;
    ForwardPass forward_pass;
    Gradients analytic, numeric;
    Params updated;
    std::vector<double> losses;  // initial + 100 steps
};
BackpropStudy runBackpropStudy();
void writeBackprop(JsonWriter& json, const BackpropStudy& study);
std::string backpropLossCsv(const BackpropStudy& study);
std::string backpropGradientCsv(const BackpropStudy& study);

}  // namespace uew

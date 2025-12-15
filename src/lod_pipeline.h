#ifndef LOD_PIPELINE_H
#define LOD_PIPELINE_H

#include <vector>
#include <cstddef>
#include "mesh_processor.h"

// Settings for a single LOD output (one b3dm per level)
struct LODLevelSettings {
    float target_ratio = 1.0f;              // 1.0 = full detail; smaller = more aggressive simplify
    float target_error = 0.01f;             // Simplification error budget (matches SimplificationParams)
    bool enable_simplification = false;     // Whether to run mesh simplification for this LOD
    bool enable_draco = false;              // Whether to apply Draco on this LOD output

    SimplificationParams simplify;          // Base simplify params (ratio/error will be overridden)
    DracoCompressionParams draco;           // Base Draco params
};

// Pipeline-wide LOD configuration
struct LODPipelineSettings {
    bool enable_lod = false;                // Master switch; when false, only emit LOD0
    std::vector<LODLevelSettings> levels;   // Ordered coarse->fine or fine->coarse depending on usage
};

// Build levels from ratios and templates; ratios are applied in order to simplify.target_ratio and set enable_simplification=true
// base_error is used as simplify.target_error for all levels
// If draco_for_lod0 is false, LOD0 will have enable_draco=false even if draco_template.enable_compression is true
std::vector<LODLevelSettings> build_lod_levels(
    const std::vector<float>& ratios,
    float base_error,
    const SimplificationParams& simplify_template,
    const DracoCompressionParams& draco_template,
    bool draco_for_lod0 = false);

#endif // LOD_PIPELINE_H

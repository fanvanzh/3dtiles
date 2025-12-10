#include "lod_pipeline.h"
#include <cstddef>

// Global LOD configuration shared across pipelines (shape/osgb)
static LODPipelineSettings g_lod_settings = [](){
    LODPipelineSettings cfg;
    cfg.enable_lod = false;
    return cfg;
}();

LODPipelineSettings make_default_lod_pipeline() {
    LODPipelineSettings cfg;
    cfg.enable_lod = true;

    SimplificationParams simplify_base;
    simplify_base.enable_simplification = true;
    simplify_base.target_ratio = 1.0f;  // will be overridden per level
    simplify_base.target_error = 0.01f;

    DracoCompressionParams draco_base;
    draco_base.enable_compression = true;

    cfg.levels = build_lod_levels({1.0f, 0.5f, 0.25f}, simplify_base.target_error, simplify_base, draco_base, false);
    return cfg;
}

std::vector<LODLevelSettings> build_lod_levels(
    const std::vector<float>& ratios,
    float base_error,
    const SimplificationParams& simplify_template,
    const DracoCompressionParams& draco_template,
    bool draco_for_lod0) {

    std::vector<LODLevelSettings> levels;
    levels.reserve(ratios.size());

    for (size_t i = 0; i < ratios.size(); ++i) {
        LODLevelSettings lvl;
        lvl.target_ratio = ratios[i];
        lvl.target_error = base_error;

        lvl.enable_simplification = true;
        lvl.simplify = simplify_template;
        lvl.simplify.target_ratio = ratios[i];
        lvl.simplify.target_error = base_error;

        lvl.enable_draco = draco_template.enable_compression;
        if (i == 0 && !draco_for_lod0) {
            lvl.enable_draco = false;
        }
        lvl.draco = draco_template;

        levels.push_back(lvl);
    }

    return levels;
}

void set_global_lod_config(const std::vector<float>& ratios,
                           float base_error,
                           const SimplificationParams& simplify_template,
                           const DracoCompressionParams& draco_template,
                           bool draco_for_lod0) {
    if (ratios.empty()) {
        g_lod_settings.enable_lod = false;
        g_lod_settings.levels.clear();
        return;
    }
    g_lod_settings.enable_lod = true;
    g_lod_settings.levels = build_lod_levels(ratios, base_error, simplify_template, draco_template, draco_for_lod0);
}

const LODPipelineSettings& get_global_lod_config() {
    return g_lod_settings;
}

extern "C" void set_lod_config(const float* ratios,
                                 std::size_t len,
                                 float base_error,
                                 bool draco_for_lod0,
                                 bool enable_draco,
                                 int position_q,
                                 int normal_q,
                                 int tex_q,
                                 int generic_q) {
    if (!ratios || len == 0) {
        g_lod_settings.enable_lod = false;
        g_lod_settings.levels.clear();
        return;
    }
    std::vector<float> r(ratios, ratios + len);

    SimplificationParams simplify_template;
    simplify_template.enable_simplification = true;
    simplify_template.target_ratio = 1.0f;
    simplify_template.target_error = base_error;

    DracoCompressionParams draco_template;
    draco_template.enable_compression = enable_draco;
    draco_template.position_quantization_bits = position_q;
    draco_template.normal_quantization_bits = normal_q;
    draco_template.tex_coord_quantization_bits = tex_q;
    draco_template.generic_quantization_bits = generic_q;

    set_global_lod_config(r, base_error, simplify_template, draco_template, draco_for_lod0);
}

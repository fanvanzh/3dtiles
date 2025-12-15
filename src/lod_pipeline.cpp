#include "lod_pipeline.h"
#include <cstddef>

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

        // Use user's enable_simplification setting instead of forcing true
        lvl.enable_simplification = simplify_template.enable_simplification;
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

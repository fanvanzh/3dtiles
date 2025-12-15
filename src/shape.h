#pragma once

#include "lod_pipeline.h"
#include "mesh_processor.h"
#include <cstddef>

struct ShapeConversionParams {
  const char *input_path;
  const char *output_path;
  const char *height_field;
  int layer_id;                      // Layer index in shapefile (default: 0)

  // Feature flags
  bool enable_lod;                   // Whether to enable LOD (uses default config)

  // Draco and Simplification settings
  DracoCompressionParams draco_compression_params;
  SimplificationParams simplify_params;
};

import type { Ref } from 'vue';

export interface TilesetConfig {
  url: string;
  maximumScreenSpaceError?: number;
  skipLevelOfDetail?: boolean;
  skipLevels?: number;
  immediatelyLoadDesiredLevelOfDetail?: boolean;
  loadSiblings?: boolean;
  cullWithChildrenBounds?: boolean;
}

export interface TilesetState {
  tilesetLayer: mars3d.layer.TilesetLayer | null;
  isLoading: boolean;
  error: Error | null;
  url: string | null;
  info: TilesetInfo | null;
}

export interface TilesetInfo {
  url: string;
  version?: string;
  gltfUpAxis?: string;
  geometricError?: number;
}

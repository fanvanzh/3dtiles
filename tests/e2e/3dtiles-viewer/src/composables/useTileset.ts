import { shallowRef } from 'vue';
import type { Ref } from 'vue';
import * as mars3d from 'mars3d';
import type { TilesetConfig, TilesetInfo } from '../types';

export function useTileset() {
  const tilesetLayer = shallowRef<mars3d.layer.TilesetLayer | null>(null);
  const isLoading = shallowRef<boolean>(false);
  const error = shallowRef<Error | null>(null);
  const tilesetInfo = shallowRef<TilesetInfo | null>(null);

  async function loadTileset(map: mars3d.Map, url: string, options?: TilesetConfig) {
    isLoading.value = true;
    error.value = null;

    try {
      const layer = new mars3d.layer.TilesetLayer({
        url: url,
        maximumScreenSpaceError: 16,
        skipLevelOfDetail: true,
        skipLevels: 0,
        immediatelyLoadDesiredLevelOfDetail: false,
        loadSiblings: false,
        cullWithChildrenBounds: true,
        // Fix floating and drift issues when terrain is enabled
        // Clamp tileset to ground to prevent floating
        clampToGround: true,
        ...options
      });

      map.addLayer(layer);
      tilesetLayer.value = layer;

      layer.readyPromise.then(() => {
        tilesetInfo.value = {
          url: url,
          version: (layer.tileset as any)?.version || '',
          gltfUpAxis: (layer.tileset as any)?.gltfUpAxis || 'Y',
          geometricError: (layer.tileset as any)?.geometricError || 0
        };
      });
    } catch (err) {
      error.value = err as Error;
    } finally {
      isLoading.value = false;
    }
  }

  function unloadTileset() {
    if (tilesetLayer.value) {
      tilesetLayer.value.destroy();
      tilesetLayer.value = null;
    }
    tilesetInfo.value = null;
  }

  async function flyToTileset(duration?: number) {
    if (tilesetLayer.value) {
      await tilesetLayer.value.flyTo({ duration: duration });
    }
  }

  function moveToOrigin() {
    console.log('moveToOrigin called');
  }

  return {
    tilesetLayer,
    isLoading,
    error,
    tilesetInfo,
    loadTileset,
    unloadTileset,
    flyToTileset,
    moveToOrigin
  };
}

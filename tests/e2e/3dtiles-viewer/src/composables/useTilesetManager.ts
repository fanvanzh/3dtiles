import { shallowRef } from 'vue';
import type { Ref } from 'vue';
import type { TilesetConfig } from '../types';

export function useTilesetManager() {
  const tilesets = shallowRef<Map<string, mars3d.layer.TilesetLayer>>(new Map());
  const activeTilesetId = shallowRef<string | null>(null);
  const isLoading = shallowRef<boolean>(false);

  async function addTileset(id: string, map: mars3d.Map, config: TilesetConfig) {
    isLoading.value = true;

    try {
      const layer = new mars3d.layer.TilesetLayer(config);
      map.addLayer(layer);
      tilesets.value.set(id, layer);
      activeTilesetId.value = id;

      await layer.readyPromise;
    } catch (err) {
      console.error('Failed to add tileset:', err);
      throw err;
    } finally {
      isLoading.value = false;
    }
  }

  function removeTileset(id: string) {
    const layer = tilesets.value.get(id);
    if (layer) {
      layer.destroy();
      tilesets.value.delete(id);

      if (activeTilesetId.value === id) {
        activeTilesetId.value = null;
      }
    }
  }

  function setActiveTileset(id: string | null) {
    activeTilesetId.value = id;
  }

  function getTileset(id: string): mars3d.layer.TilesetLayer | undefined {
    return tilesets.value.get(id);
  }

  function getActiveTileset(): mars3d.layer.TilesetLayer | undefined {
    if (!activeTilesetId.value) return undefined;
    return tilesets.value.get(activeTilesetId.value);
  }

  function clearAllTilesets() {
    tilesets.value.forEach((layer) => {
      layer.destroy();
    });
    tilesets.value.clear();
    activeTilesetId.value = null;
  }

  function getTilesetCount(): number {
    return tilesets.value.size;
  }

  function getAllTilesetIds(): string[] {
    return Array.from(tilesets.value.keys());
  }

  return {
    tilesets,
    activeTilesetId,
    isLoading,
    addTileset,
    removeTileset,
    setActiveTileset,
    getTileset,
    getActiveTileset,
    clearAllTilesets,
    getTilesetCount,
    getAllTilesetIds
  };
}

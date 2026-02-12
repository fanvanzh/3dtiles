import { defineStore } from 'pinia';
import type { TilesetInfo } from '../types';

interface TilesetState {
  tilesetLayer: mars3d.layer.TilesetLayer | null;
  isLoading: boolean;
  error: Error | null;
  url: string | null;
  info: TilesetInfo | null;
}

export const useTilesetStore = defineStore('tileset', {
  state: (): TilesetState => ({
    tilesetLayer: null,
    isLoading: false,
    error: null,
    url: null,
    info: null
  }),

  actions: {
    setLoading(isLoading: boolean) {
      this.isLoading = isLoading;
    },
    setError(error: Error | null) {
      this.error = error;
    },
    setTilesetLayer(tilesetLayer: mars3d.layer.TilesetLayer | null) {
      this.tilesetLayer = tilesetLayer;
    },
    setUrl(url: string | null) {
      this.url = url;
    },
    setInfo(info: TilesetInfo | null) {
      this.info = info;
    },
    clearTileset() {
      this.tilesetLayer = null;
      this.isLoading = false;
      this.error = null;
      this.url = null;
      this.info = null;
    }
  }
});

import { defineStore } from 'pinia';
import type { Viewer3DConfig } from '../types';

interface ViewerState {
  viewer: Cesium.Viewer | null;
  map: mars3d.Map | null;
  isReady: boolean;
  config: Viewer3DConfig;
}

export const useViewerStore = defineStore('viewer', {
  state: (): ViewerState => ({
    viewer: null,
    map: null,
    isReady: false,
    config: {}
  }),

  actions: {
    initViewer(config: Viewer3DConfig) {
      this.config = config;
    },
    destroyViewer() {
      this.viewer = null;
      this.map = null;
      this.isReady = false;
    },
    setViewer(viewer: Cesium.Viewer) {
      this.viewer = viewer;
    },
    setMap(map: mars3d.Map) {
      this.map = map;
    },
    setReady(isReady: boolean) {
      this.isReady = isReady;
    }
  }
});

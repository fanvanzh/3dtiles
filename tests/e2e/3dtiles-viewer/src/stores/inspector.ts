import { defineStore } from 'pinia';
import type { InspectorState } from '../types';

interface InspectorStoreState extends InspectorState {}

export const useInspectorStore = defineStore('inspector', {
  state: (): InspectorStoreState => ({
    debugWireframe: false,
    debugShowBoundingVolume: false,
    debugShowContentBoundingVolume: false,
    debugShowViewerRequestVolume: false,
    debugColorizeTiles: false,
    debugShowMemoryUsage: false
  }),

  actions: {
    setDebugWireframe(value: boolean) {
      this.debugWireframe = value;
    },
    setDebugShowBoundingVolume(value: boolean) {
      this.debugShowBoundingVolume = value;
    },
    setDebugShowContentBoundingVolume(value: boolean) {
      this.debugShowContentBoundingVolume = value;
    },
    setDebugShowViewerRequestVolume(value: boolean) {
      this.debugShowViewerRequestVolume = value;
    },
    setDebugColorizeTiles(value: boolean) {
      this.debugColorizeTiles = value;
    },
    setDebugShowMemoryUsage(value: boolean) {
      this.debugShowMemoryUsage = value;
    },
    resetDebugOptions() {
      this.debugWireframe = false;
      this.debugShowBoundingVolume = false;
      this.debugShowContentBoundingVolume = false;
      this.debugShowViewerRequestVolume = false;
      this.debugColorizeTiles = false;
      this.debugShowMemoryUsage = false;
    }
  }
});

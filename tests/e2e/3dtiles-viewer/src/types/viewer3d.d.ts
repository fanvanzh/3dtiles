import type { Ref } from 'vue';

declare namespace mars3d {
  class Map {
    viewer: Cesium.Viewer;
    constructor(container: string | HTMLElement, options?: any);
    addLayer(layer: any): void;
    destroy(): void;
  }

  namespace layer {
    class TilesetLayer {
      url: string;
      tileset: any;
      readyPromise: Promise<void>;
      constructor(options: any);
      flyTo(duration?: number): Promise<void>;
      moveToOrigin(): void;
      destroy(): void;
    }
  }
}

export interface Viewer3DConfig {
  container?: string;
  scene?: {
    center?: {
      lat: number;
      lng: number;
      alt: number;
      heading: number;
      pitch: number;
    };
    fxaa?: boolean;
  };
  control?: {
    contextmenu?: {
      hasDefault: boolean;
    };
  };
}

export interface Viewer3DState {
  viewer: Cesium.Viewer | null;
  map: mars3d.Map | null;
  isReady: boolean;
  config: Viewer3DConfig;
}

export interface TilesetInfo {
  url: string;
  version?: string;
  gltfUpAxis?: string;
  geometricError?: number;
}

export interface PerformanceMetrics {
  fps: number;
  drawCalls: number;
  triangles: number;
  vertices: number;
  materials: number;
  memory: number;
  tiles: number;
}

export interface InspectorState {
  debugWireframe: boolean;
  debugShowBoundingVolume: boolean;
  debugShowContentBoundingVolume: boolean;
  debugShowViewerRequestVolume: boolean;
  debugColorizeTiles: boolean;
  debugShowMemoryUsage: boolean;
}

export interface CameraInfo {
  position: {
    longitude: number;
    latitude: number;
    height: number;
  };
  heading: number;
  pitch: number;
  roll: number;
  fov: number;
}

export interface FeatureInfo {
  id: string;
  type: string;
  properties: Record<string, any>;
  geometry?: any;
}

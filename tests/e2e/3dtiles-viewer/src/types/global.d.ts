declare global {
  namespace Cesium {
    class Viewer {
      scene: Scene;
      camera: Camera;
      canvas: HTMLCanvasElement;
      constructor(container: string | HTMLElement, options?: any);
      destroy(): void;
      render(): void;
    }

    class Scene {
      primitives: PrimitiveCollection;
      debugShowFramesPerSecond: boolean;
      cache: any;
      context: any;
      pick(position: any): any;
    }

    class Camera {
      positionCartographic: Cartographic;
      heading: number;
      pitch: number;
      roll: number;
      frustum: any;
      flyTo(options: any): void;
      flyToBoundingSphere(boundingSphere: any, options?: any): void;
      flyHome(): void;
      lookAt(target: any, offset?: any): void;
      rotateLeft(angle: number): void;
      rotateRight(angle: number): void;
      lookUp(angle: number): void;
      lookDown(angle: number): void;
      zoomIn(amount: number): void;
      zoomOut(amount: number): void;
      setView(options: any): void;
    }

    class Cartographic {
      longitude: number;
      latitude: number;
      height: number;
    }

    class Cartesian3 {
      x: number;
      y: number;
      z: number;
      static fromRadians(longitude: number, latitude: number, height?: number): Cartesian3;
      static fromDegrees(longitude: number, latitude: number, height?: number): Cartesian3;
      static distance(left: Cartesian3, right: Cartesian3): number;
      static fromCartesian(cartesian: Cartesian3): Cartographic;
    }

    class ScreenSpaceEventHandler {
      constructor(canvas: HTMLCanvasElement);
      setInputAction(action: (movement: any) => void, type: number): void;
      destroy(): void;
    }

    class ScreenSpaceEventType {
      static readonly LEFT_CLICK: number;
      static readonly MOUSE_MOVE: number;
    }

    class Color {
      static YELLOW: Color;
      static WHITE: Color;
    }

    class PrimitiveCollection {
      length: number;
      get(index: number): any;
      reduce(callback: (acc: any, item: any, index: number) => any, initial?: any): any;
    }

    namespace Math {
      function toRadians(degrees: number): number;
      function toDegrees(radians: number): number;
    }

    function toRadians(degrees: number): number;
    function toDegrees(radians: number): number;

    interface HeadingPitchRange {
      heading?: number;
      pitch?: number;
      range?: number;
    }
  }

  namespace mars3d {
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
        flyTo(options?: any): Promise<void>;
        destroy(): void;
      }
    }
  }
}

export {};

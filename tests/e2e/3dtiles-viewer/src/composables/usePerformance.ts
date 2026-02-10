import { shallowRef, onUnmounted } from 'vue';
import type { Ref } from 'vue';
import type { PerformanceMetrics } from '../types';

export function usePerformance(viewer: Ref<Cesium.Viewer | null>) {
  const metrics = shallowRef<PerformanceMetrics>({
    fps: 0,
    drawCalls: 0,
    triangles: 0,
    vertices: 0,
    materials: 0,
    memory: 0,
    tiles: 0
  });

  const isMonitoring = shallowRef<boolean>(false);
  let updateInterval: number | null = null;
  let frameCount = 0;
  let lastTime = performance.now();

  function updateMetrics() {
    if (!viewer.value) return;

    const scene = viewer.value.scene;
    const currentTime = performance.now();
    frameCount++;

    if (currentTime - lastTime >= 1000) {
      metrics.value.fps = frameCount;
      frameCount = 0;
      lastTime = currentTime;
    }

    metrics.value.drawCalls = scene.debugShowFramesPerSecond ? (scene.debugShowFramesPerSecond as any) : 0;
    metrics.value.triangles = scene.primitives.length;
    metrics.value.memory = (performance as any).memory?.usedJSHeapSize || 0;
    metrics.value.tiles = scene.primitives.reduce((count: number, primitive: any) => {
      if (primitive.tileset) {
        return count + 1;
      }
      return count;
    }, 0);
  }

  function startMonitoring(interval = 1000) {
    if (isMonitoring.value) return;

    isMonitoring.value = true;
    updateInterval = window.setInterval(updateMetrics, interval);
  }

  function stopMonitoring() {
    if (!isMonitoring.value) return;

    isMonitoring.value = false;
    if (updateInterval !== null) {
      clearInterval(updateInterval);
      updateInterval = null;
    }
  }

  function resetMetrics() {
    metrics.value = {
      fps: 0,
      drawCalls: 0,
      triangles: 0,
      vertices: 0,
      materials: 0,
      memory: 0,
      tiles: 0
    };
  }

  function getDetailedMetrics() {
    if (!viewer.value) return null;

    const scene = viewer.value.scene;

    return {
      fps: metrics.value.fps,
      frameTime: 1000 / (metrics.value.fps || 1),
      drawCalls: scene.debugShowFramesPerSecond || 0,
      triangles: scene.primitives.length,
      vertices: 0,
      textures: scene.context._textures.length,
      materials: 0,
      memory: {
        used: (performance as any).memory?.usedJSHeapSize || 0,
        total: (performance as any).memory?.totalJSHeapSize || 0,
        limit: (performance as any).memory?.jsHeapSizeLimit || 0
      },
      tiles: metrics.value.tiles,
      cache: {
        bytes: 0,
        items: 0
      }
    };
  }

  onUnmounted(() => {
    stopMonitoring();
  });

  return {
    metrics,
    isMonitoring,
    startMonitoring,
    stopMonitoring,
    resetMetrics,
    getDetailedMetrics
  };
}

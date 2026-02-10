<template>
  <div class="performance-metrics">
    <div class="metrics-header">
      <span class="metrics-title">Performance</span>
      <button class="toggle-btn" @click="toggleMonitoring">
        {{ isMonitoring ? 'Stop' : 'Start' }}
      </button>
    </div>
    <div class="metrics-content">
      <div class="metric-item">
        <span class="metric-label">FPS:</span>
        <span class="metric-value" :class="{ 'good': metrics.fps >= 30, 'warning': metrics.fps >= 15 && metrics.fps < 30, 'poor': metrics.fps < 15 }">
          {{ metrics.fps }}
        </span>
      </div>
      <div class="metric-item">
        <span class="metric-label">Draw Calls:</span>
        <span class="metric-value">{{ metrics.drawCalls }}</span>
      </div>
      <div class="metric-item">
        <span class="metric-label">Triangles:</span>
        <span class="metric-value">{{ formatNumber(metrics.triangles) }}</span>
      </div>
      <div class="metric-item">
        <span class="metric-label">Vertices:</span>
        <span class="metric-value">{{ formatNumber(metrics.vertices) }}</span>
      </div>
      <div class="metric-item">
        <span class="metric-label">Materials:</span>
        <span class="metric-value">{{ metrics.materials }}</span>
      </div>
      <div class="metric-item">
        <span class="metric-label">Memory:</span>
        <span class="metric-value">{{ formatMemory(metrics.memory) }}</span>
      </div>
      <div class="metric-item">
        <span class="metric-label">Tiles:</span>
        <span class="metric-value">{{ metrics.tiles }}</span>
      </div>
    </div>
  </div>
</template>

<script setup lang="ts">
import { ref, inject, onMounted, onUnmounted, watch, type ComputedRef } from 'vue';
import type { PerformanceMetrics } from '../../types';

const viewerRef = inject<ComputedRef<any> | null>('viewer', null);
const tilesetLayerRef = inject<ComputedRef<any> | null>('tilesetLayer', null);

const metrics = ref<PerformanceMetrics>({
  fps: 0,
  drawCalls: 0,
  triangles: 0,
  vertices: 0,
  materials: 0,
  memory: 0,
  tiles: 0
});

const isMonitoring = ref(false);
let frameCount = 0;
let lastFpsTime = performance.now();
let cachedMetrics: Partial<PerformanceMetrics> = {};
let hasSetupListeners = false;

function getViewer() {
  return viewerRef?.value;
}

function getTilesetLayer() {
  return tilesetLayerRef?.value;
}

function updateMetrics() {
  const viewer = getViewer();
  if (!viewer) return;

  const currentTime = performance.now();
  frameCount++;

  if (currentTime - lastFpsTime >= 1000) {
    metrics.value.fps = frameCount;
    frameCount = 0;
    lastFpsTime = currentTime;
  }

  const scene = viewer.scene;
  const primitives = scene.primitives;
  const frameState = scene._frameState;

  const newDrawCalls = frameState?.commandList?.length || 0;
  const newMemory = (performance as any).memory?.usedJSHeapSize || 0;

  let triangles = 0;
  let vertices = 0;
  let tiles = 0;

  const tilesetLayer = getTilesetLayer();
  if (tilesetLayer && tilesetLayer.tileset) {
    const tileset = tilesetLayer.tileset;

    const stats = tileset.statistics;
    if (stats) {
      tiles = stats.selected || 0;
      triangles = stats.numberOfTrianglesSelected || 0;
      vertices = stats.numberOfPointsSelected || 0;

      if (vertices === 0 && triangles > 0) {
        vertices = triangles * 3;
      }
    }
  }

  const newMaterials = primitives ? primitives.length : 0;

  if (cachedMetrics.drawCalls !== newDrawCalls) {
    metrics.value.drawCalls = newDrawCalls;
    cachedMetrics.drawCalls = newDrawCalls;
  }

  if (cachedMetrics.triangles !== triangles) {
    metrics.value.triangles = triangles;
    cachedMetrics.triangles = triangles;
  }

  if (cachedMetrics.vertices !== vertices) {
    metrics.value.vertices = vertices;
    cachedMetrics.vertices = vertices;
  }

  if (cachedMetrics.materials !== newMaterials) {
    metrics.value.materials = newMaterials;
    cachedMetrics.materials = newMaterials;
  }

  if (cachedMetrics.memory !== newMemory) {
    metrics.value.memory = newMemory;
    cachedMetrics.memory = newMemory;
  }

  if (cachedMetrics.tiles !== tiles) {
    metrics.value.tiles = tiles;
    cachedMetrics.tiles = tiles;
  }
}

function toggleMonitoring() {
  if (isMonitoring.value) {
    stopMonitoring();
  } else {
    startMonitoring();
  }
}

function startMonitoring() {
  const viewer = getViewer();
  if (isMonitoring.value || !viewer || hasSetupListeners) return;
  isMonitoring.value = true;
  viewer.scene.postRender.addEventListener(updateMetrics);
  hasSetupListeners = true;
  console.log('PerformanceMetrics: Monitoring started');
}

function stopMonitoring() {
  const viewer = getViewer();
  if (!isMonitoring.value || !viewer) return;
  isMonitoring.value = false;
  viewer.scene.postRender.removeEventListener(updateMetrics);
  hasSetupListeners = false;
  console.log('PerformanceMetrics: Monitoring stopped');
}

function formatNumber(value: number): string {
  if (value >= 1000000) {
    return (value / 1000000).toFixed(2) + 'M';
  }
  if (value >= 1000) {
    return (value / 1000).toFixed(2) + 'K';
  }
  return value.toString();
}

function formatMemory(bytes: number): string {
  if (bytes >= 1073741824) {
    return (bytes / 1073741824).toFixed(2) + ' GB';
  }
  if (bytes >= 1048576) {
    return (bytes / 1048576).toFixed(2) + ' MB';
  }
  if (bytes >= 1024) {
    return (bytes / 1024).toFixed(2) + ' KB';
  }
  return bytes + ' B';
}

watch(() => viewerRef?.value, (newViewer) => {
  if (newViewer && !hasSetupListeners) {
    console.log('PerformanceMetrics: Viewer updated:', newViewer);
    startMonitoring();
  } else if (!newViewer && hasSetupListeners) {
    stopMonitoring();
  }
});

onMounted(() => {
  console.log('PerformanceMetrics mounted, viewer:', viewerRef?.value);
  const viewer = getViewer();
  if (viewer && !hasSetupListeners) {
    startMonitoring();
  }
});

onUnmounted(() => {
  stopMonitoring();
});
</script>

<style scoped>
.performance-metrics {
  padding: 12px 0;
  border-bottom: 1px solid rgba(255, 255, 255, 0.1);
}

.metrics-header {
  display: flex;
  justify-content: space-between;
  align-items: center;
  padding: 0 16px 12px;
}

.metrics-title {
  font-size: 14px;
  font-weight: 600;
  color: #fff;
}

.toggle-btn {
  background: rgba(255, 255, 255, 0.1);
  border: 1px solid rgba(255, 255, 255, 0.2);
  border-radius: 4px;
  color: #fff;
  font-size: 12px;
  padding: 4px 12px;
  cursor: pointer;
  transition: all 0.2s;
}

.toggle-btn:hover {
  background: rgba(255, 255, 255, 0.2);
}

.metrics-content {
  display: flex;
  flex-direction: column;
  gap: 8px;
  padding: 0 16px;
}

.metric-item {
  display: flex;
  justify-content: space-between;
  align-items: center;
}

.metric-label {
  font-size: 12px;
  color: rgba(255, 255, 255, 0.6);
}

.metric-value {
  font-size: 13px;
  font-weight: 600;
  color: #fff;
  font-variant-numeric: tabular-nums;
}

.metric-value.good {
  color: #4ade80;
}

.metric-value.warning {
  color: #fbbf24;
}

.metric-value.poor {
  color: #f87171;
}
</style>

<template>
  <div class="camera-controls">
    <div class="controls-row">
      <button
        class="control-btn"
        title="Rotate Left"
        @click="rotateLeft"
      >
        ↺
      </button>
      <button
        class="control-btn"
        title="Rotate Right"
        @click="rotateRight"
      >
        ↻
      </button>
    </div>
    <div class="controls-row">
      <button
        class="control-btn"
        title="Tilt Up"
        @click="tiltUp"
      >
        ↑
      </button>
      <button
        class="control-btn"
        title="Reset View"
        @click="resetView"
      >
        ⟲
      </button>
      <button
        class="control-btn"
        title="Tilt Down"
        @click="tiltDown"
      >
        ↓
      </button>
    </div>
    <div class="controls-row">
      <button
        class="control-btn"
        title="Zoom In"
        @click="zoomIn"
      >
        +
      </button>
      <button
        class="control-btn"
        title="Zoom Out"
        @click="zoomOut"
      >
        −
      </button>
    </div>
  </div>
</template>

<script setup lang="ts">
import { inject } from 'vue';

const viewer = inject<Cesium.Viewer | null>('viewer', null);

function rotateLeft() {
  if (!viewer) return;
  const camera = viewer.camera;
  camera.rotateLeft(Cesium.Math.toRadians(15));
}

function rotateRight() {
  if (!viewer) return;
  const camera = viewer.camera;
  camera.rotateRight(Cesium.Math.toRadians(15));
}

function tiltUp() {
  if (!viewer) return;
  const camera = viewer.camera;
  camera.lookUp(Cesium.Math.toRadians(10));
}

function tiltDown() {
  if (!viewer) return;
  const camera = viewer.camera;
  camera.lookDown(Cesium.Math.toRadians(10));
}

function zoomIn() {
  if (!viewer) return;
  const camera = viewer.camera;
  camera.zoomIn(camera.positionCartographic.height * 0.2);
}

function zoomOut() {
  if (!viewer) return;
  const camera = viewer.camera;
  camera.zoomOut(camera.positionCartographic.height * 0.25);
}

function resetView() {
  if (!viewer) return;
  viewer.camera.flyHome();
}
</script>

<style scoped>
.camera-controls {
  display: flex;
  flex-direction: column;
  gap: 4px;
  background: rgba(0, 0, 0, 0.85);
  border: 1px solid rgba(255, 255, 255, 0.2);
  border-radius: 8px;
  padding: 8px;
  backdrop-filter: blur(10px);
}

.controls-row {
  display: flex;
  gap: 4px;
  justify-content: center;
}

.control-btn {
  width: 36px;
  height: 36px;
  background: rgba(255, 255, 255, 0.1);
  border: 1px solid rgba(255, 255, 255, 0.2);
  border-radius: 6px;
  color: #fff;
  font-size: 16px;
  cursor: pointer;
  display: flex;
  align-items: center;
  justify-content: center;
  transition: all 0.2s;
  font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif;
}

.control-btn:hover {
  background: rgba(255, 255, 255, 0.2);
  border-color: rgba(255, 255, 255, 0.3);
  transform: scale(1.05);
}

.control-btn:active {
  transform: scale(0.95);
}
</style>

<template>
  <div class="status-bar">
    <div class="status-section">
      <CoordinateDisplay :position="cameraInfo.position" />
    </div>
    <div class="status-divider"></div>
    <div class="status-section">
      <CameraInfo
        :heading="cameraInfo.heading"
        :pitch="cameraInfo.pitch"
        :roll="cameraInfo.roll"
        :fov="cameraInfo.fov"
      />
    </div>
  </div>
</template>

<script setup lang="ts">
import { ref, inject, onMounted, onUnmounted, watch, type ComputedRef } from 'vue';
import CoordinateDisplay from './CoordinateDisplay.vue';
import CameraInfo from './CameraInfo.vue';
import type { CameraInfo as CameraInfoType } from '../../types';

const viewerRef = inject<ComputedRef<any> | null>('viewer', null);

const cameraInfo = ref<CameraInfoType>({
  position: { longitude: 0, latitude: 0, height: 0 },
  heading: 0,
  pitch: 0,
  roll: 0,
  fov: 60
});

let lastUpdateTime = 0;
const UPDATE_INTERVAL = 200;
let hasSetupListeners = false;

function getViewer() {
  return viewerRef?.value;
}

function updateCameraInfo() {
  const viewer = getViewer();
  if (!viewer) return;

  const currentTime = performance.now();

  if (currentTime - lastUpdateTime < UPDATE_INTERVAL) {
    return;
  }

  lastUpdateTime = currentTime;

  const camera = viewer.camera;
  const position = camera.positionCartographic;

  cameraInfo.value = {
    position: {
      longitude: (window as any).Cesium.Math.toDegrees(position.longitude),
      latitude: (window as any).Cesium.Math.toDegrees(position.latitude),
      height: position.height
    },
    heading: (window as any).Cesium.Math.toDegrees(camera.heading),
    pitch: (window as any).Cesium.Math.toDegrees(camera.pitch),
    roll: (window as any).Cesium.Math.toDegrees(camera.roll),
    fov: (window as any).Cesium.Math.toDegrees(camera.frustum.fov)
  };
}

function setupEventListeners() {
  const viewer = getViewer();
  if (!viewer || hasSetupListeners) return;

  viewer.scene.postRender.addEventListener(updateCameraInfo);
  viewer.camera.moveEnd.addEventListener(updateCameraInfo);
  hasSetupListeners = true;
  console.log('StatusBar: Event listeners setup');
}

function cleanupEventListeners() {
  const viewer = getViewer();
  if (!viewer) return;

  viewer.scene.postRender.removeEventListener(updateCameraInfo);
  viewer.camera.moveEnd.removeEventListener(updateCameraInfo);
  hasSetupListeners = false;
  console.log('StatusBar: Event listeners cleaned up');
}

watch(() => viewerRef?.value, (newViewer) => {
  if (newViewer && !hasSetupListeners) {
    console.log('StatusBar: Viewer updated:', newViewer);
    setupEventListeners();
    updateCameraInfo();
  } else if (!newViewer && hasSetupListeners) {
    cleanupEventListeners();
  }
});

onMounted(() => {
  console.log('StatusBar mounted, viewer:', viewerRef?.value);
  const viewer = getViewer();
  if (viewer && !hasSetupListeners) {
    setupEventListeners();
    updateCameraInfo();
  }
});

onUnmounted(() => {
  cleanupEventListeners();
});
</script>

<style scoped>
.status-bar {
  position: absolute;
  bottom: 0;
  left: 0;
  right: 0;
  background: rgba(0, 0, 0, 0.85);
  border-top: 1px solid rgba(255, 255, 255, 0.1);
  color: #fff;
  font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif;
  font-size: 12px;
  display: flex;
  align-items: center;
  padding: 8px 16px;
  backdrop-filter: blur(10px);
  z-index: 1000;
}

.status-section {
  display: flex;
  align-items: center;
}

.status-divider {
  width: 1px;
  height: 20px;
  background: rgba(255, 255, 255, 0.1);
  margin: 0 16px;
}
</style>

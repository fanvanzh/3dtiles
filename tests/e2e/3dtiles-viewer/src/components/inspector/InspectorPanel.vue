<template>
  <div class="inspector-panel">
    <div class="inspector-header">
      <h3>Inspector</h3>
      <button class="close-btn" @click="closePanel">×</button>
    </div>
    <div class="inspector-content">
      <PerformanceMetrics :viewer="viewerRef" />
      <TilesetInfo :tileset-info="tilesetInfo" />
      <DebugControls
        :viewer="viewerRef"
        :tileset-layer="tilesetLayerRef"
        :options="debugOptions"
        @update:options="updateDebugOptions"
      />
    </div>
  </div>
</template>

<script setup lang="ts">
import { ref, inject, onMounted, onUnmounted, watch, type ComputedRef } from 'vue';
import PerformanceMetrics from './PerformanceMetrics.vue';
import TilesetInfo from './TilesetInfo.vue';
import DebugControls from './DebugControls.vue';
import type { InspectorState, TilesetInfo as TilesetInfoType } from '../../types';

const emit = defineEmits<{
  'update:visible': [value: boolean];
}>();

const viewerRef = inject<ComputedRef<any> | null>('viewer', null);
const tilesetLayerRef = inject<ComputedRef<any> | null>('tilesetLayer', null);
const tilesetInfo = inject<TilesetInfoType | null>('tilesetInfo', null);

const debugOptions = ref<InspectorState>({
  debugWireframe: false,
  debugShowBoundingVolume: false,
  debugShowContentBoundingVolume: false,
  debugShowViewerRequestVolume: false,
  debugColorizeTiles: false,
  debugShowMemoryUsage: false
});

function closePanel() {
  emit('update:visible', false);
}

function updateDebugOptions(options: InspectorState) {
  debugOptions.value = options;
}

function applyDebugOptions() {
  const tilesetLayer = tilesetLayerRef?.value;
  if (!tilesetLayer) return;

  const tileset = tilesetLayer.tileset;
  if (!tileset) return;

  console.log('Applying debug options from InspectorPanel:', debugOptions.value);

  tileset.debugWireframe = debugOptions.value.debugWireframe;
  tileset.debugShowBoundingVolume = debugOptions.value.debugShowBoundingVolume;
  tileset.debugShowContentBoundingVolume = debugOptions.value.debugShowContentBoundingVolume;
  tileset.debugShowViewerRequestVolume = debugOptions.value.debugShowViewerRequestVolume;
  tileset.debugColorizeTiles = debugOptions.value.debugColorizeTiles;
  tileset.debugShowMemoryUsage = debugOptions.value.debugShowMemoryUsage;
}

watch(debugOptions, () => {
  applyDebugOptions();
});

watch(() => tilesetLayerRef?.value, (newTilesetLayer) => {
  if (newTilesetLayer && newTilesetLayer.tileset) {
    console.log('InspectorPanel: Tileset layer updated:', newTilesetLayer);
    applyDebugOptions();
  }
});

onMounted(() => {
  console.log('InspectorPanel mounted, tilesetLayer:', tilesetLayerRef?.value);
  const tilesetLayer = tilesetLayerRef?.value;
  if (tilesetLayer && tilesetLayer.tileset) {
    applyDebugOptions();
  }
});

onUnmounted(() => {
  console.log('InspectorPanel unmounted');
});
</script>

<style scoped>
.inspector-panel {
  position: absolute;
  top: 20px;
  right: 20px;
  width: 320px;
  background: rgba(0, 0, 0, 0.85);
  border: 1px solid rgba(255, 255, 255, 0.2);
  border-radius: 8px;
  color: #fff;
  font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif;
  z-index: 1000;
  backdrop-filter: blur(10px);
}

.inspector-header {
  display: flex;
  justify-content: space-between;
  align-items: center;
  padding: 12px 16px;
  border-bottom: 1px solid rgba(255, 255, 255, 0.1);
}

.inspector-header h3 {
  margin: 0;
  font-size: 16px;
  font-weight: 600;
}

.close-btn {
  background: transparent;
  border: none;
  color: #fff;
  font-size: 20px;
  cursor: pointer;
  padding: 0;
  width: 24px;
  height: 24px;
  display: flex;
  align-items: center;
  justify-content: center;
  border-radius: 4px;
  transition: background 0.2s;
}

.close-btn:hover {
  background: rgba(255, 255, 255, 0.1);
}

.inspector-content {
  padding: 0;
  max-height: calc(100vh - 100px);
  overflow-y: auto;
}

.inspector-content::-webkit-scrollbar {
  width: 6px;
}

.inspector-content::-webkit-scrollbar-track {
  background: rgba(255, 255, 255, 0.05);
}

.inspector-content::-webkit-scrollbar-thumb {
  background: rgba(255, 255, 255, 0.2);
  border-radius: 3px;
}

.inspector-content::-webkit-scrollbar-thumb:hover {
  background: rgba(255, 255, 255, 0.3);
}
</style>

<template>
  <div class="debug-controls">
    <div class="controls-header">
      <span class="controls-title">Debug Options</span>
      <button class="reset-btn" @click="resetAll">Reset</button>
    </div>
    <div class="controls-content">
      <div class="control-item">
        <label class="control-label">
          <input
            type="checkbox"
            v-model="debugOptions.debugWireframe"
            @change="updateDebugOption('debugWireframe', ($event.target as HTMLInputElement).checked)"
          />
          <span>Wireframe</span>
        </label>
      </div>
      <div class="control-item">
        <label class="control-label">
          <input
            type="checkbox"
            v-model="debugOptions.debugShowBoundingVolume"
            @change="updateDebugOption('debugShowBoundingVolume', ($event.target as HTMLInputElement).checked)"
          />
          <span>Show Bounding Volume</span>
        </label>
      </div>
      <div class="control-item">
        <label class="control-label">
          <input
            type="checkbox"
            v-model="debugOptions.debugShowContentBoundingVolume"
            @change="updateDebugOption('debugShowContentBoundingVolume', ($event.target as HTMLInputElement).checked)"
          />
          <span>Show Content Bounding Volume</span>
        </label>
      </div>
      <div class="control-item">
        <label class="control-label">
          <input
            type="checkbox"
            v-model="debugOptions.debugShowViewerRequestVolume"
            @change="updateDebugOption('debugShowViewerRequestVolume', ($event.target as HTMLInputElement).checked)"
          />
          <span>Show Viewer Request Volume</span>
        </label>
      </div>
      <div class="control-item">
        <label class="control-label">
          <input
            type="checkbox"
            v-model="debugOptions.debugColorizeTiles"
            @change="updateDebugOption('debugColorizeTiles', ($event.target as HTMLInputElement).checked)"
          />
          <span>Colorize Tiles</span>
        </label>
      </div>
      <div class="control-item">
        <label class="control-label">
          <input
            type="checkbox"
            v-model="debugOptions.debugShowMemoryUsage"
            @change="updateDebugOption('debugShowMemoryUsage', ($event.target as HTMLInputElement).checked)"
          />
          <span>Show Memory Usage</span>
        </label>
      </div>
    </div>
  </div>
</template>

<script setup lang="ts">
import { ref, watch, type ComputedRef } from 'vue';
import type { InspectorState } from '../../types';

const props = defineProps<{
  viewer: ComputedRef<any> | null;
  tilesetLayer: ComputedRef<any> | null;
  options: InspectorState;
}>();

const emit = defineEmits<{
  'update:options': [options: InspectorState];
}>();

const debugOptions = ref<InspectorState>({ ...props.options });

watch(() => props.options, (newOptions) => {
  debugOptions.value = { ...newOptions };
}, { deep: true });

function updateDebugOption(key: keyof InspectorState, value: boolean) {
  debugOptions.value[key] = value;
  emit('update:options', { ...debugOptions.value });
}

function resetAll() {
  debugOptions.value = {
    debugWireframe: false,
    debugShowBoundingVolume: false,
    debugShowContentBoundingVolume: false,
    debugShowViewerRequestVolume: false,
    debugColorizeTiles: false,
    debugShowMemoryUsage: false
  };
  emit('update:options', { ...debugOptions.value });
}
</script>

<style scoped>
.debug-controls {
  padding: 12px 0;
}

.controls-header {
  display: flex;
  justify-content: space-between;
  align-items: center;
  padding: 0 16px 12px;
}

.controls-title {
  font-size: 14px;
  font-weight: 600;
  color: #fff;
}

.reset-btn {
  background: rgba(255, 255, 255, 0.1);
  border: 1px solid rgba(255, 255, 255, 0.2);
  border-radius: 4px;
  color: #fff;
  font-size: 11px;
  padding: 4px 12px;
  cursor: pointer;
  transition: all 0.2s;
}

.reset-btn:hover {
  background: rgba(255, 255, 255, 0.2);
}

.controls-content {
  display: flex;
  flex-direction: column;
  gap: 8px;
  padding: 0 16px;
}

.control-item {
  display: flex;
  align-items: center;
}

.control-label {
  display: flex;
  align-items: center;
  gap: 8px;
  cursor: pointer;
  font-size: 12px;
  color: #fff;
  user-select: none;
}

.control-label input[type="checkbox"] {
  width: 16px;
  height: 16px;
  cursor: pointer;
  accent-color: #3b82f6;
}

.control-label span {
  flex: 1;
}
</style>

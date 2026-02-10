<template>
  <div class="tileset-info">
    <div class="info-header">
      <span class="info-title">Tileset Info</span>
    </div>
    <div class="info-content">
      <div v-if="tilesetInfo" class="info-items">
        <div class="info-item">
          <span class="info-label">URL:</span>
          <span class="info-value">{{ tilesetInfo.url }}</span>
        </div>
        <div v-if="tilesetInfo.version" class="info-item">
          <span class="info-label">Version:</span>
          <span class="info-value">{{ tilesetInfo.version }}</span>
        </div>
        <div v-if="tilesetInfo.gltfUpAxis" class="info-item">
          <span class="info-label">GLTF Up Axis:</span>
          <span class="info-value">{{ tilesetInfo.gltfUpAxis }}</span>
        </div>
        <div v-if="tilesetInfo.geometricError !== undefined" class="info-item">
          <span class="info-label">Geometric Error:</span>
          <span class="info-value">{{ tilesetInfo.geometricError }}</span>
        </div>
      </div>
      <div v-else class="info-empty">
        <span class="empty-text">No tileset loaded</span>
      </div>
    </div>
  </div>
</template>

<script setup lang="ts">
import { computed } from 'vue';
import type { TilesetInfo } from '../../types';

const props = defineProps<{
  tilesetInfo: TilesetInfo | null;
}>();
</script>

<style scoped>
.tileset-info {
  padding: 12px 0;
  border-bottom: 1px solid rgba(255, 255, 255, 0.1);
}

.info-header {
  padding: 0 16px 12px;
}

.info-title {
  font-size: 14px;
  font-weight: 600;
  color: #fff;
}

.info-content {
  padding: 0 16px;
}

.info-items {
  display: flex;
  flex-direction: column;
  gap: 8px;
}

.info-item {
  display: flex;
  flex-direction: column;
  gap: 4px;
}

.info-label {
  font-size: 11px;
  color: rgba(255, 255, 255, 0.5);
  font-weight: 500;
}

.info-value {
  font-size: 12px;
  color: #fff;
  word-break: break-all;
  font-family: 'SF Mono', Monaco, 'Cascadia Code', 'Roboto Mono', Consolas, monospace;
}

.info-empty {
  display: flex;
  justify-content: center;
  align-items: center;
  padding: 20px 0;
}

.empty-text {
  font-size: 12px;
  color: rgba(255, 255, 255, 0.4);
  font-style: italic;
}
</style>

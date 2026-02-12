<template>
  <div class="properties-panel">
    <div class="properties-header">
      <h3>Properties</h3>
      <button class="close-btn" @click="closePanel">×</button>
    </div>
    <div class="properties-content">
      <div v-if="selectedFeature" class="feature-info">
        <div class="feature-header">
          <span class="feature-id">{{ selectedFeature.id }}</span>
          <span class="feature-type">{{ selectedFeature.type }}</span>
        </div>
        <PropertyTable :properties="properties" />
      </div>
      <div v-else class="empty-state">
        <span class="empty-text">Select a feature to view properties</span>
      </div>
    </div>
  </div>
</template>

<script setup lang="ts">
import { computed } from 'vue';
import PropertyTable from './PropertyTable.vue';
import type { FeatureInfo } from '../../types';

const props = defineProps<{
  selectedFeature: FeatureInfo | null;
}>();

const emit = defineEmits<{
  'update:visible': [value: boolean];
}>();

const properties = computed(() => {
  if (!props.selectedFeature?.properties) return [];

  return Object.entries(props.selectedFeature.properties).map(([key, value]) => ({
    key,
    value
  }));
});

function closePanel() {
  emit('update:visible', false);
}
</script>

<style scoped>
.properties-panel {
  position: absolute;
  top: 20px;
  left: 20px;
  width: 280px;
  background: rgba(0, 0, 0, 0.85);
  border: 1px solid rgba(255, 255, 255, 0.2);
  border-radius: 8px;
  color: #fff;
  font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif;
  z-index: 1000;
  backdrop-filter: blur(10px);
}

.properties-header {
  display: flex;
  justify-content: space-between;
  align-items: center;
  padding: 12px 16px;
  border-bottom: 1px solid rgba(255, 255, 255, 0.1);
}

.properties-header h3 {
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

.properties-content {
  padding: 0;
  max-height: calc(100vh - 100px);
  overflow-y: auto;
}

.properties-content::-webkit-scrollbar {
  width: 6px;
}

.properties-content::-webkit-scrollbar-track {
  background: rgba(255, 255, 255, 0.05);
}

.properties-content::-webkit-scrollbar-thumb {
  background: rgba(255, 255, 255, 0.2);
  border-radius: 3px;
}

.properties-content::-webkit-scrollbar-thumb:hover {
  background: rgba(255, 255, 255, 0.3);
}

.feature-info {
  padding: 16px;
}

.feature-header {
  display: flex;
  justify-content: space-between;
  align-items: center;
  padding-bottom: 12px;
  margin-bottom: 12px;
  border-bottom: 1px solid rgba(255, 255, 255, 0.1);
}

.feature-id {
  font-size: 13px;
  font-weight: 600;
  color: #fff;
  font-family: 'SF Mono', Monaco, 'Cascadia Code', 'Roboto Mono', Consolas, monospace;
}

.feature-type {
  font-size: 11px;
  background: rgba(59, 130, 246, 0.3);
  border: 1px solid rgba(59, 130, 246, 0.5);
  border-radius: 4px;
  padding: 2px 8px;
  color: #93c5fd;
  font-weight: 500;
}

.empty-state {
  display: flex;
  justify-content: center;
  align-items: center;
  padding: 40px 20px;
}

.empty-text {
  font-size: 12px;
  color: rgba(255, 255, 255, 0.4);
  font-style: italic;
}
</style>

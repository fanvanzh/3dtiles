<template>
  <div
    class="tileset-card"
    :class="{ active: isActive, loading: isLoading, error: hasError }"
    @click="onClick"
  >
    <div class="card-header">
      <span class="card-title">{{ title }}</span>
      <div class="card-actions">
        <button
          v-if="canRemove"
          class="action-btn remove-btn"
          @click.stop="onRemove"
          title="Remove tileset"
        >
          ×
        </button>
        <button
          v-if="canFlyTo"
          class="action-btn flyto-btn"
          @click.stop="onFlyTo"
          title="Fly to tileset"
        >
          📍
        </button>
      </div>
    </div>
    <div class="card-body">
      <p class="card-url">{{ url }}</p>
      <div v-if="info" class="card-info">
        <span v-if="info.version" class="info-tag">v{{ info.version }}</span>
        <span v-if="info.geometricError" class="info-tag">GE: {{ info.geometricError }}</span>
      </div>
    </div>
    <div v-if="isLoading" class="card-loading">
      <span>Loading...</span>
    </div>
    <div v-if="hasError" class="card-error">
      <span>{{ errorMessage }}</span>
    </div>
  </div>
</template>

<script setup lang="ts">
import { computed } from 'vue';
import type { TilesetInfo } from '../../types';

const props = withDefaults(
  defineProps<{
    id: string;
    title: string;
    url: string;
    isActive?: boolean;
    isLoading?: boolean;
    hasError?: boolean;
    errorMessage?: string;
    info?: TilesetInfo | null;
    canRemove?: boolean;
    canFlyTo?: boolean;
  }>(),
  {
    isActive: false,
    isLoading: false,
    hasError: false,
    errorMessage: 'Failed to load',
    info: null,
    canRemove: true,
    canFlyTo: true
  }
);

const emit = defineEmits<{
  'click': [id: string];
  'remove': [id: string];
  'flyto': [id: string];
}>();

function onClick() {
  emit('click', props.id);
}

function onRemove() {
  emit('remove', props.id);
}

function onFlyTo() {
  emit('flyto', props.id);
}
</script>

<style scoped>
.tileset-card {
  background: rgba(0, 0, 0, 0.6);
  border: 1px solid rgba(255, 255, 255, 0.2);
  border-radius: 8px;
  padding: 12px;
  cursor: pointer;
  transition: all 0.2s ease;
  backdrop-filter: blur(10px);
}

.tileset-card:hover {
  background: rgba(0, 0, 0, 0.8);
  border-color: rgba(255, 255, 255, 0.3);
}

.tileset-card.active {
  background: rgba(59, 130, 246, 0.2);
  border-color: rgba(59, 130, 246, 0.6);
}

.tileset-card.loading {
  opacity: 0.7;
}

.tileset-card.error {
  border-color: rgba(239, 68, 68, 0.6);
}

.card-header {
  display: flex;
  justify-content: space-between;
  align-items: center;
  margin-bottom: 8px;
}

.card-title {
  font-size: 14px;
  font-weight: 600;
  color: #fff;
  overflow: hidden;
  text-overflow: ellipsis;
  white-space: nowrap;
  flex: 1;
}

.card-actions {
  display: flex;
  gap: 4px;
  margin-left: 8px;
}

.action-btn {
  background: rgba(255, 255, 255, 0.1);
  border: 1px solid rgba(255, 255, 255, 0.2);
  border-radius: 4px;
  color: #fff;
  font-size: 14px;
  cursor: pointer;
  padding: 4px 8px;
  display: flex;
  align-items: center;
  justify-content: center;
  transition: all 0.2s;
}

.action-btn:hover {
  background: rgba(255, 255, 255, 0.2);
}

.remove-btn:hover {
  background: rgba(239, 68, 68, 0.3);
  border-color: rgba(239, 68, 68, 0.6);
}

.flyto-btn:hover {
  background: rgba(34, 197, 94, 0.3);
  border-color: rgba(34, 197, 94, 0.6);
}

.card-body {
  margin-bottom: 8px;
}

.card-url {
  font-size: 11px;
  color: rgba(255, 255, 255, 0.6);
  margin: 0 0 8px 0;
  overflow: hidden;
  text-overflow: ellipsis;
  white-space: nowrap;
  font-family: 'SF Mono', Monaco, 'Cascadia Code', 'Roboto Mono', Consolas, monospace;
}

.card-info {
  display: flex;
  gap: 6px;
  flex-wrap: wrap;
}

.info-tag {
  font-size: 10px;
  background: rgba(255, 255, 255, 0.1);
  border: 1px solid rgba(255, 255, 255, 0.2);
  border-radius: 3px;
  padding: 2px 6px;
  color: rgba(255, 255, 255, 0.8);
}

.card-loading,
.card-error {
  padding: 8px;
  border-radius: 4px;
  text-align: center;
  font-size: 12px;
}

.card-loading {
  background: rgba(59, 130, 246, 0.2);
  color: #93c5fd;
}

.card-error {
  background: rgba(239, 68, 68, 0.2);
  color: #fca5a5;
}
</style>

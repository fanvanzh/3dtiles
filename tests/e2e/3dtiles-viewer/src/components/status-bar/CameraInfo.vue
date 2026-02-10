<template>
  <div class="camera-info">
    <div class="camera-item">
      <span class="camera-label">Heading:</span>
      <span class="camera-value">{{ formatNumber(heading, 1) }}°</span>
    </div>
    <div class="camera-item">
      <span class="camera-label">Pitch:</span>
      <span class="camera-value">{{ formatNumber(pitch, 1) }}°</span>
    </div>
    <div class="camera-item">
      <span class="camera-label">Roll:</span>
      <span class="camera-value">{{ formatNumber(roll, 1) }}°</span>
    </div>
    <div class="camera-item">
      <span class="camera-label">FOV:</span>
      <span class="camera-value">{{ formatNumber(fov, 1) }}°</span>
    </div>
  </div>
</template>

<script setup lang="ts">
import { computed } from 'vue';

const props = defineProps<{
  heading: number;
  pitch: number;
  roll: number;
  fov: number;
}>();

const heading = computed(() => props.heading);
const pitch = computed(() => props.pitch);
const roll = computed(() => props.roll);
const fov = computed(() => props.fov);

function formatNumber(value: number, decimals: number): string {
  return value.toFixed(decimals);
}
</script>

<style scoped>
.camera-info {
  display: flex;
  gap: 16px;
  align-items: center;
}

.camera-item {
  display: flex;
  align-items: center;
  gap: 6px;
}

.camera-label {
  font-size: 11px;
  color: rgba(255, 255, 255, 0.5);
  font-weight: 500;
  text-transform: uppercase;
}

.camera-value {
  font-size: 12px;
  color: #fff;
  font-weight: 600;
  font-variant-numeric: tabular-nums;
  font-family: 'SF Mono', Monaco, 'Cascadia Code', 'Roboto Mono', Consolas, monospace;
}
</style>

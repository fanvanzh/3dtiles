<template>
  <div class="coordinate-display">
    <div class="coordinate-item">
      <span class="coordinate-label">Lng:</span>
      <span class="coordinate-value">{{ formatNumber(longitude, 6) }}°</span>
    </div>
    <div class="coordinate-item">
      <span class="coordinate-label">Lat:</span>
      <span class="coordinate-value">{{ formatNumber(latitude, 6) }}°</span>
    </div>
    <div class="coordinate-item">
      <span class="coordinate-label">Alt:</span>
      <span class="coordinate-value">{{ formatNumber(height, 1) }}m</span>
    </div>
  </div>
</template>

<script setup lang="ts">
import { computed } from 'vue';

const props = defineProps<{
  position: {
    longitude: number;
    latitude: number;
    height: number;
  };
}>();

const longitude = computed(() => props.position.longitude);
const latitude = computed(() => props.position.latitude);
const height = computed(() => props.position.height);

function formatNumber(value: number, decimals: number): string {
  return value.toFixed(decimals);
}
</script>

<style scoped>
.coordinate-display {
  display: flex;
  gap: 16px;
  align-items: center;
}

.coordinate-item {
  display: flex;
  align-items: center;
  gap: 6px;
}

.coordinate-label {
  font-size: 11px;
  color: rgba(255, 255, 255, 0.5);
  font-weight: 500;
  text-transform: uppercase;
}

.coordinate-value {
  font-size: 12px;
  color: #fff;
  font-weight: 600;
  font-variant-numeric: tabular-nums;
  font-family: 'SF Mono', Monaco, 'Cascadia Code', 'Roboto Mono', Consolas, monospace;
}
</style>

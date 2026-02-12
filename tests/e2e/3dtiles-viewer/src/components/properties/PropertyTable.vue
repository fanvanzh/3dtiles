<template>
  <div class="property-table">
    <div v-if="properties.length === 0" class="empty-state">
      <span class="empty-text">No properties available</span>
    </div>
    <div v-else class="table-container">
      <div
        v-for="(property, index) in properties"
        :key="index"
        class="table-row"
        :class="{ highlighted: index === highlightedIndex }"
      >
        <span class="row-key">{{ property.key }}</span>
        <span class="row-value" :title="String(property.value)">
          {{ formatValue(property.value) }}
        </span>
      </div>
    </div>
  </div>
</template>

<script setup lang="ts">
import { ref } from 'vue';

interface Property {
  key: string;
  value: any;
}

const props = withDefaults(
  defineProps<{
    properties: Property[];
    highlightedIndex?: number;
  }>(),
  {
    properties: () => [],
    highlightedIndex: -1
  }
);

function formatValue(value: any): string {
  if (value === null || value === undefined) {
    return 'N/A';
  }

  if (typeof value === 'object') {
    return JSON.stringify(value);
  }

  if (typeof value === 'number') {
    if (Number.isInteger(value)) {
      return value.toString();
    }
    return value.toFixed(6);
  }

  if (typeof value === 'boolean') {
    return value ? 'true' : 'false';
  }

  return String(value);
}
</script>

<style scoped>
.property-table {
  display: flex;
  flex-direction: column;
}

.empty-state {
  display: flex;
  justify-content: center;
  align-items: center;
  padding: 20px;
}

.empty-text {
  font-size: 12px;
  color: rgba(255, 255, 255, 0.4);
  font-style: italic;
}

.table-container {
  display: flex;
  flex-direction: column;
  gap: 4px;
}

.table-row {
  display: flex;
  justify-content: space-between;
  align-items: center;
  padding: 8px 12px;
  background: rgba(255, 255, 255, 0.05);
  border-radius: 4px;
  transition: background 0.2s;
}

.table-row:hover {
  background: rgba(255, 255, 255, 0.1);
}

.table-row.highlighted {
  background: rgba(59, 130, 246, 0.2);
  border: 1px solid rgba(59, 130, 246, 0.4);
}

.row-key {
  font-size: 11px;
  color: rgba(255, 255, 255, 0.6);
  font-weight: 500;
  text-transform: uppercase;
  flex-shrink: 0;
  margin-right: 12px;
}

.row-value {
  font-size: 12px;
  color: #fff;
  font-weight: 400;
  overflow: hidden;
  text-overflow: ellipsis;
  white-space: nowrap;
  font-family: 'SF Mono', Monaco, 'Cascadia Code', 'Roboto Mono', Consolas, monospace;
  text-align: right;
  flex: 1;
}
</style>

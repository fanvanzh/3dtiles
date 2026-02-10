<template>
  <div class="collapsible-panel" :class="{ collapsed: !isExpanded }">
    <div class="collapse-header" @click="toggle">
      <slot name="header">
        <span class="header-title">{{ title }}</span>
      </slot>
      <button class="collapse-btn">
        <span class="collapse-icon" :class="{ expanded: isExpanded }">▼</span>
      </button>
    </div>
    <div v-show="isExpanded" class="collapse-content">
      <slot></slot>
    </div>
  </div>
</template>

<script setup lang="ts">
import { ref } from 'vue';

const props = withDefaults(
  defineProps<{
    title?: string;
    defaultExpanded?: boolean;
  }>(),
  {
    title: '',
    defaultExpanded: true
  }
);

const emit = defineEmits<{
  'toggle': [isExpanded: boolean];
}>();

const isExpanded = ref(props.defaultExpanded);

function toggle() {
  isExpanded.value = !isExpanded.value;
  emit('toggle', isExpanded.value);
}

function expand() {
  isExpanded.value = true;
  emit('toggle', true);
}

function collapse() {
  isExpanded.value = false;
  emit('toggle', false);
}

defineExpose({
  expand,
  collapse,
  toggle
});
</script>

<style scoped>
.collapsible-panel {
  background: rgba(0, 0, 0, 0.85);
  border: 1px solid rgba(255, 255, 255, 0.2);
  border-radius: 8px;
  overflow: hidden;
  backdrop-filter: blur(10px);
}

.collapse-header {
  display: flex;
  justify-content: space-between;
  align-items: center;
  padding: 12px 16px;
  cursor: pointer;
  user-select: none;
  background: rgba(255, 255, 255, 0.05);
  border-bottom: 1px solid rgba(255, 255, 255, 0.1);
  transition: background 0.2s;
}

.collapse-header:hover {
  background: rgba(255, 255, 255, 0.1);
}

.header-title {
  font-size: 14px;
  font-weight: 600;
  color: #fff;
}

.collapse-btn {
  background: transparent;
  border: none;
  color: #fff;
  cursor: pointer;
  padding: 4px;
  display: flex;
  align-items: center;
  justify-content: center;
  border-radius: 4px;
  transition: background 0.2s;
}

.collapse-btn:hover {
  background: rgba(255, 255, 255, 0.1);
}

.collapse-icon {
  font-size: 10px;
  transition: transform 0.3s ease;
  display: inline-block;
}

.collapse-icon.expanded {
  transform: rotate(180deg);
}

.collapse-content {
  padding: 16px;
}

.collapsed .collapse-content {
  display: none;
}
</style>

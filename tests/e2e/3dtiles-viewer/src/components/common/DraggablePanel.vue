<template>
  <div
    ref="panelRef"
    class="draggable-panel"
    :style="{ left: position.x + 'px', top: position.y + 'px' }"
    @mousedown="onMouseDown"
  >
    <slot></slot>
  </div>
</template>

<script setup lang="ts">
import { ref, onMounted, onUnmounted } from 'vue';
import { useDraggable } from '../../composables/useDraggable';

const props = withDefaults(
  defineProps<{
    initialPosition?: { x: number; y: number };
  }>(),
  {
    initialPosition: () => ({ x: 20, y: 20 })
  }
);

const panelRef = ref<HTMLElement>();
const { position, onMouseDown } = useDraggable(panelRef, props.initialPosition);
</script>

<style scoped>
.draggable-panel {
  position: absolute;
  cursor: move;
  user-select: none;
}
</style>

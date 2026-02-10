<template>
  <div class="viewer-toolbar">
    <div class="toolbar-section">
      <button
        class="toolbar-btn"
        :class="{ active: showInspector }"
        title="Inspector"
        @click="toggleInspector"
      >
        🔍
      </button>
      <button
        class="toolbar-btn"
        :class="{ active: showProperties }"
        title="Properties"
        @click="toggleProperties"
      >
        📋
      </button>
    </div>
    <div class="toolbar-divider"></div>
    <div class="toolbar-section">
      <button
        class="toolbar-btn"
        :class="{ active: isFullscreen }"
        title="Fullscreen"
        @click="toggleFullscreen"
      >
        {{ isFullscreen ? '⛶' : '⛶' }}
      </button>
      <button
        class="toolbar-btn"
        title="Screenshot"
        @click="takeScreenshot"
      >
        📷
      </button>
      <button
        class="toolbar-btn"
        title="Settings"
        @click="toggleSettings"
      >
        ⚙️
      </button>
    </div>
  </div>
</template>

<script setup lang="ts">
import { ref, inject } from 'vue';

const viewer = inject<any | null>('viewer', null);

const showInspector = ref(true);
const showProperties = ref(false);
const isFullscreen = ref(false);

const emit = defineEmits<{
  'toggle-inspector': [visible: boolean];
  'toggle-properties': [visible: boolean];
  'toggle-settings': [];
}>();

function toggleInspector() {
  showInspector.value = !showInspector.value;
  emit('toggle-inspector', showInspector.value);
}

function toggleProperties() {
  showProperties.value = !showProperties.value;
  emit('toggle-properties', showProperties.value);
}

function toggleFullscreen() {
  if (!document.fullscreenElement) {
    document.documentElement.requestFullscreen();
    isFullscreen.value = true;
  } else {
    document.exitFullscreen();
    isFullscreen.value = false;
  }
}

function takeScreenshot() {
  if (!viewer) return;

  viewer.render();
  const imageData = viewer.canvas.toDataURL('image/png');

  const link = document.createElement('a');
  link.download = `screenshot-${Date.now()}.png`;
  link.href = imageData;
  link.click();
}

function toggleSettings() {
  emit('toggle-settings');
}
</script>

<style scoped>
.viewer-toolbar {
  display: flex;
  flex-direction: column;
  gap: 8px;
  background: rgba(0, 0, 0, 0.85);
  border: 1px solid rgba(255, 255, 255, 0.2);
  border-radius: 8px;
  padding: 8px;
  backdrop-filter: blur(10px);
}

.toolbar-section {
  display: flex;
  flex-direction: column;
  gap: 4px;
}

.toolbar-divider {
  height: 1px;
  background: rgba(255, 255, 255, 0.1);
  margin: 4px 0;
}

.toolbar-btn {
  width: 40px;
  height: 40px;
  background: rgba(255, 255, 255, 0.1);
  border: 1px solid rgba(255, 255, 255, 0.2);
  border-radius: 6px;
  color: #fff;
  font-size: 18px;
  cursor: pointer;
  display: flex;
  align-items: center;
  justify-content: center;
  transition: all 0.2s;
}

.toolbar-btn:hover {
  background: rgba(255, 255, 255, 0.2);
  border-color: rgba(255, 255, 255, 0.3);
  transform: scale(1.05);
}

.toolbar-btn:active {
  transform: scale(0.95);
}

.toolbar-btn.active {
  background: rgba(59, 130, 246, 0.6);
  border-color: rgba(59, 130, 246, 0.8);
}
</style>

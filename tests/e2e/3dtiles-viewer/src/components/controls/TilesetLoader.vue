<template>
  <div class="tileset-loader">
    <div class="loader-header">
      <h3>Tileset Loader</h3>
      <button class="close-btn" @click="$emit('close')">×</button>
    </div>
    <div class="loader-content">
      <div class="input-group">
        <label for="tileset-url">Tileset URL:</label>
        <input
          id="tileset-url"
          v-model="tilesetUrl"
          type="text"
          placeholder="Enter tileset URL..."
          @keyup.enter="loadTileset"
        />
      </div>
      <div class="button-group">
        <button class="load-btn" :disabled="isLoading" @click="loadTileset">
          <span v-if="!isLoading">Load Tileset</span>
          <span v-else>Loading...</span>
        </button>
        <button class="clear-btn" @click="clearInput">Clear</button>
      </div>
      <div v-if="error" class="error-message">
        {{ error }}
      </div>
      <div class="recent-tilesets">
        <h4>Recent Tilesets</h4>
        <ul v-if="recentTilesets.length > 0" class="recent-list">
          <li
            v-for="(item, index) in recentTilesets"
            :key="index"
            class="recent-item"
            @click="selectRecent(item)"
          >
            <span class="recent-url">{{ item }}</span>
            <button class="remove-btn" @click.stop="removeRecent(index)">×</button>
          </li>
        </ul>
        <p v-else class="no-recent">No recent tilesets</p>
      </div>
    </div>
  </div>
</template>

<script setup lang="ts">
import { ref, onMounted } from 'vue';

const emit = defineEmits<{
  'load': [url: string];
  'close': [];
}>();

const tilesetUrl = ref('');
const isLoading = ref(false);
const error = ref('');
const recentTilesets = ref<string[]>([]);

function loadTileset() {
  if (!tilesetUrl.value.trim()) {
    error.value = 'Please enter a tileset URL';
    return;
  }

  isLoading.value = true;
  error.value = '';

  emit('load', tilesetUrl.value);

  addToRecent(tilesetUrl.value);

  setTimeout(() => {
    isLoading.value = false;
  }, 1000);
}

function clearInput() {
  tilesetUrl.value = '';
  error.value = '';
}

function selectRecent(url: string) {
  tilesetUrl.value = url;
  loadTileset();
}

function removeRecent(index: number) {
  recentTilesets.value.splice(index, 1);
  saveRecent();
}

function addToRecent(url: string) {
  const index = recentTilesets.value.indexOf(url);
  if (index > -1) {
    recentTilesets.value.splice(index, 1);
  }
  recentTilesets.value.unshift(url);
  if (recentTilesets.value.length > 10) {
    recentTilesets.value.pop();
  }
  saveRecent();
}

function saveRecent() {
  localStorage.setItem('recent-tilesets', JSON.stringify(recentTilesets.value));
}

function loadRecent() {
  const saved = localStorage.getItem('recent-tilesets');
  if (saved) {
    try {
      recentTilesets.value = JSON.parse(saved);
    } catch (e) {
      recentTilesets.value = [];
    }
  }
}

onMounted(() => {
  loadRecent();
});
</script>

<style scoped>
.tileset-loader {
  background: rgba(0, 0, 0, 0.85);
  border: 1px solid rgba(255, 255, 255, 0.2);
  border-radius: 8px;
  color: #fff;
  font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif;
  backdrop-filter: blur(10px);
}

.loader-header {
  display: flex;
  justify-content: space-between;
  align-items: center;
  padding: 12px 16px;
  border-bottom: 1px solid rgba(255, 255, 255, 0.1);
}

.loader-header h3 {
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

.loader-content {
  padding: 16px;
}

.input-group {
  display: flex;
  flex-direction: column;
  gap: 8px;
  margin-bottom: 16px;
}

.input-group label {
  font-size: 12px;
  color: rgba(255, 255, 255, 0.6);
  font-weight: 500;
}

.input-group input {
  background: rgba(255, 255, 255, 0.1);
  border: 1px solid rgba(255, 255, 255, 0.2);
  border-radius: 6px;
  padding: 10px 12px;
  color: #fff;
  font-size: 13px;
  font-family: inherit;
  outline: none;
  transition: border-color 0.2s;
}

.input-group input:focus {
  border-color: #3b82f6;
}

.input-group input::placeholder {
  color: rgba(255, 255, 255, 0.4);
}

.button-group {
  display: flex;
  gap: 8px;
  margin-bottom: 12px;
}

.load-btn {
  flex: 1;
  background: #3b82f6;
  border: none;
  border-radius: 6px;
  padding: 10px 16px;
  color: #fff;
  font-size: 13px;
  font-weight: 600;
  cursor: pointer;
  transition: background 0.2s;
}

.load-btn:hover:not(:disabled) {
  background: #2563eb;
}

.load-btn:disabled {
  opacity: 0.6;
  cursor: not-allowed;
}

.clear-btn {
  background: rgba(255, 255, 255, 0.1);
  border: 1px solid rgba(255, 255, 255, 0.2);
  border-radius: 6px;
  padding: 10px 16px;
  color: #fff;
  font-size: 13px;
  font-weight: 500;
  cursor: pointer;
  transition: background 0.2s;
}

.clear-btn:hover {
  background: rgba(255, 255, 255, 0.2);
}

.error-message {
  background: rgba(239, 68, 68, 0.2);
  border: 1px solid rgba(239, 68, 68, 0.4);
  border-radius: 6px;
  padding: 10px 12px;
  font-size: 12px;
  color: #fca5a5;
  margin-bottom: 16px;
}

.recent-tilesets {
  border-top: 1px solid rgba(255, 255, 255, 0.1);
  padding-top: 12px;
}

.recent-tilesets h4 {
  margin: 0 0 12px 0;
  font-size: 13px;
  font-weight: 600;
  color: rgba(255, 255, 255, 0.8);
}

.recent-list {
  list-style: none;
  padding: 0;
  margin: 0;
  display: flex;
  flex-direction: column;
  gap: 4px;
}

.recent-item {
  display: flex;
  justify-content: space-between;
  align-items: center;
  padding: 8px 12px;
  background: rgba(255, 255, 255, 0.05);
  border-radius: 4px;
  cursor: pointer;
  transition: background 0.2s;
}

.recent-item:hover {
  background: rgba(255, 255, 255, 0.1);
}

.recent-url {
  font-size: 12px;
  color: rgba(255, 255, 255, 0.8);
  overflow: hidden;
  text-overflow: ellipsis;
  white-space: nowrap;
  flex: 1;
  margin-right: 8px;
}

.remove-btn {
  background: transparent;
  border: none;
  color: rgba(255, 255, 255, 0.5);
  font-size: 16px;
  cursor: pointer;
  padding: 0;
  width: 20px;
  height: 20px;
  display: flex;
  align-items: center;
  justify-content: center;
  border-radius: 4px;
  transition: all 0.2s;
}

.remove-btn:hover {
  background: rgba(239, 68, 68, 0.2);
  color: #fca5a5;
}

.no-recent {
  font-size: 12px;
  color: rgba(255, 255, 255, 0.4);
  font-style: italic;
  margin: 0;
}
</style>

<template>
  <div id="viewer3dContainer" class="viewer3d-container">
    <slot></slot>
  </div>
</template>

<script setup lang="ts">
import { onMounted, onUnmounted } from 'vue';
import { useViewer3D } from '../../composables/useViewer3D';
import { useTileset } from '../../composables/useTileset';
import { TILESETS, DEFAULT_TILESET } from '../../config/tilesets.config';

const props = defineProps<{
  config?: any;
  autoLoadTileset?: boolean;
  tilesetUrl?: string;
}>();

const emit = defineEmits<{
  (e: 'viewer-ready', viewer: any): void;
  (e: 'map-ready', map: mars3d.Map): void;
  (e: 'tileset-loaded', tileset: any): void;
  (e: 'tileset-error', error: Error): void;
}>();

const { viewer, map, isReady, initViewer, destroyViewer } = useViewer3D();
const { loadTileset, tilesetLayer, isLoading, error } = useTileset();

onMounted(async () => {
  await initViewer(props.config || {});

  if (viewer.value) {
    (emit as any)('viewer-ready', viewer.value);
  }

  if (map.value) {
    (emit as any)('map-ready', map.value);
  }

  if (props.autoLoadTileset !== false) {
    const url = props.tilesetUrl || (TILESETS[DEFAULT_TILESET]?.url || '');
    await loadTileset(map.value!, url, TILESETS[DEFAULT_TILESET]);

    if (tilesetLayer.value) {
      (emit as any)('tileset-loaded', tilesetLayer.value);
    } else if (error.value) {
      (emit as any)('tileset-error', error.value);
    }
  }
});

onUnmounted(() => {
  destroyViewer();
});

defineExpose({
  viewer,
  map,
  isReady
});
</script>

<style scoped>
.viewer3d-container {
  width: 100%;
  height: 100%;
  position: relative;
}
</style>

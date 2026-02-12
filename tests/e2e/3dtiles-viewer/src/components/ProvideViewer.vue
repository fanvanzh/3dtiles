<template>
  <div>
    <slot></slot>
  </div>
</template>

<script setup lang="ts">
import { provide, computed } from 'vue';

const props = defineProps<{
  viewer: any | null;
  map: mars3d.Map | null;
  tilesetLayer: mars3d.layer.TilesetLayer | null;
  tilesetInfo: any;
  geoJsonLayer: any;
}>();

provide('viewer', computed(() => props.viewer));
provide('map', computed(() => props.map));
provide('tilesetLayer', computed(() => props.tilesetLayer));
provide('tilesetInfo', computed(() => props.tilesetInfo));
provide('geoJsonLayer', computed(() => props.geoJsonLayer));
</script>

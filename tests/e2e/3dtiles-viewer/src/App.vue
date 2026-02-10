<template>
  <div id="app" class="app">
    <Viewer3D
      :auto-load-tileset="true"
      @viewer-ready="onViewerReady"
      @map-ready="onMapReady"
      @tileset-loaded="onTilesetLoaded"
      @tileset-error="onTilesetError"
    >
      <template #default>
        <ProvideViewer :viewer="viewer" :map="map" :tileset-layer="tilesetLayer" :tileset-info="tilesetInfo" :geo-json-layer="geoJsonLayer">
          <InspectorPanel v-if="showInspector" />
          <PropertiesPanel v-if="selectedFeature" :selected-feature="selectedFeature" />
          <StatusBar />
        </ProvideViewer>
      </template>
    </Viewer3D>
  </div>
</template>

<script setup lang="ts">
import { shallowRef } from 'vue';
import Viewer3D from './components/3d-viewer/Viewer3D.vue';
import ProvideViewer from './components/ProvideViewer.vue';
import InspectorPanel from './components/inspector/InspectorPanel.vue';
import PropertiesPanel from './components/properties/PropertiesPanel.vue';
import StatusBar from './components/status-bar/StatusBar.vue';
import { useGeoJSON } from './composables/useGeoJSON';
import type { FeatureInfo, TilesetInfo } from './types';

const viewer = shallowRef<any | null>(null);
const map = shallowRef<mars3d.Map | null>(null);
const tilesetLayer = shallowRef<any | null>(null);
const tilesetInfo = shallowRef<TilesetInfo | null>(null);

const showInspector = shallowRef(true);
const selectedFeature = shallowRef<FeatureInfo | null>(null);

const { loadGeoJSON, geoJsonLayer } = useGeoJSON();

function onViewerReady(viewerInstance: any) {
  viewer.value = viewerInstance;
  console.log('Viewer ready:', viewerInstance);
}

function onMapReady(mapInstance: mars3d.Map) {
  map.value = mapInstance;
  console.log('Map ready:', mapInstance);

  loadGeoJSON(mapInstance as any, '/output/QGIS.geojson');
}

function onTilesetLoaded(tileset: any) {
  tilesetLayer.value = tileset;
  tilesetInfo.value = {
    url: tileset.url,
    version: (tileset.tileset as any)?.version,
    gltfUpAxis: (tileset.tileset as any)?.gltfUpAxis,
    geometricError: (tileset.tileset as any)?.geometricError
  };
  console.log('Tileset loaded:', tileset);

  if (map.value) {
    setTimeout(() => {
      tileset.flyTo({ duration: 2 });
    }, 500);
  }
}

function onTilesetError(error: Error) {
  console.error('Tileset error:', error);
}
</script>

<style>
* {
  margin: 0;
  padding: 0;
  box-sizing: border-box;
}

html, body {
  width: 100%;
  height: 100%;
  overflow: hidden;
}

#app {
  width: 100vw;
  height: 100vh;
  position: relative;
}

.app {
  width: 100%;
  height: 100%;
}
</style>

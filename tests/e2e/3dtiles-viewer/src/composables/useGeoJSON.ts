import { shallowRef } from "vue";
import type { Ref } from "vue";
import * as mars3d from "mars3d";

export function useGeoJSON() {
  const geoJsonLayer = shallowRef<any | null>(null);
  const isLoading = shallowRef<boolean>(false);
  const error = shallowRef<Error | null>(null);

  async function loadGeoJSON(map: mars3d.Map, url: string, options?: any) {
    isLoading.value = true;
    error.value = null;

    try {
      const layer = new mars3d.layer.GeoJsonLayer({
        name: "QGIS数据",
        url: url,
        symbol: {
          type: "polylineC",
          styleOptions: {
            width: 3,
            color: "#00ff00",
              opacity: 0.8,
            clampToGround: true
          }
        },
        popup: "all"
      });

      map.addLayer(layer);
      geoJsonLayer.value = layer;
    } catch (err) {
      error.value = err as Error;
    } finally {
      isLoading.value = false;
    }
  }

  function unloadGeoJSON() {
    if (geoJsonLayer.value) {
      geoJsonLayer.value.destroy();
      geoJsonLayer.value = null;
    }
  }

  return {
    geoJsonLayer,
    isLoading,
    error,
    loadGeoJSON,
    unloadGeoJSON
  };
}

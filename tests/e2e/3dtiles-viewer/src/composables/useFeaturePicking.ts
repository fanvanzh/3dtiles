import { shallowRef, onUnmounted } from 'vue';
import type { Ref } from 'vue';
import type { FeatureInfo } from '../types';

export function useFeaturePicking(viewer: Ref<Cesium.Viewer | null>) {
  const selectedFeature = shallowRef<FeatureInfo | null>(null);
  const hoveredFeature = shallowRef<FeatureInfo | null>(null);
  const isPickingEnabled = shallowRef<boolean>(true);

  let clickHandler: Cesium.ScreenSpaceEventHandler | null = null;
  let hoverHandler: Cesium.ScreenSpaceEventHandler | null = null;

  function createFeatureInfo(feature: any): FeatureInfo | null {
    if (!feature) return null;

    return {
      id: feature.id || feature.getProperty('id') || '',
      type: feature.type || feature.getProperty('type') || 'Unknown',
      properties: feature.properties || {},
      geometry: feature.geometry
    };
  }

  function onLeftClick(movement: any) {
    if (!viewer.value || !isPickingEnabled.value) return;

    const pickedObject = viewer.value.scene.pick(movement.position);

    if (pickedObject && pickedObject.primitive) {
      const feature = (pickedObject.primitive as any).feature || pickedObject;
      selectedFeature.value = createFeatureInfo(feature);
    } else {
      selectedFeature.value = null;
    }
  }

  function onMouseMove(movement: any) {
    if (!viewer.value || !isPickingEnabled.value) return;

    const pickedObject = viewer.value.scene.pick(movement.position);

    if (pickedObject && pickedObject.primitive) {
      const feature = (pickedObject.primitive as any).feature || pickedObject;
      hoveredFeature.value = createFeatureInfo(feature);
    } else {
      hoveredFeature.value = null;
    }
  }

  function enablePicking() {
    if (!viewer.value) return;

    if (!clickHandler) {
      clickHandler = new Cesium.ScreenSpaceEventHandler(viewer.value.canvas);
      clickHandler.setInputAction(onLeftClick, Cesium.ScreenSpaceEventType.LEFT_CLICK);
    }

    if (!hoverHandler) {
      hoverHandler = new Cesium.ScreenSpaceEventHandler(viewer.value.canvas);
      hoverHandler.setInputAction(onMouseMove, Cesium.ScreenSpaceEventType.MOUSE_MOVE);
    }

    isPickingEnabled.value = true;
  }

  function disablePicking() {
    if (clickHandler) {
      clickHandler.destroy();
      clickHandler = null;
    }

    if (hoverHandler) {
      hoverHandler.destroy();
      hoverHandler = null;
    }

    isPickingEnabled.value = false;
  }

  function clearSelection() {
    selectedFeature.value = null;
  }

  function clearHover() {
    hoveredFeature.value = null;
  }

  function selectFeature(feature: FeatureInfo) {
    selectedFeature.value = feature;
  }

  function highlightFeature(feature: FeatureInfo) {
    if (!viewer.value) return;

    const primitives = viewer.value.scene.primitives;
    for (let i = 0; i < primitives.length; i++) {
      const primitive = primitives.get(i);
      if ((primitive as any).feature && (primitive as any).feature.id === feature.id) {
        (primitive as any).color = Cesium.Color.YELLOW;
        break;
      }
    }
  }

  function unhighlightFeature(feature: FeatureInfo) {
    if (!viewer.value) return;

    const primitives = viewer.value.scene.primitives;
    for (let i = 0; i < primitives.length; i++) {
      const primitive = primitives.get(i);
      if ((primitive as any).feature && (primitive as any).feature.id === feature.id) {
        (primitive as any).color = Cesium.Color.WHITE;
        break;
      }
    }
  }

  function flyToFeature(feature: FeatureInfo, duration = 2) {
    if (!viewer.value) return;

    const primitives = viewer.value.scene.primitives;
    for (let i = 0; i < primitives.length; i++) {
      const primitive = primitives.get(i);
      if ((primitive as any).feature && (primitive as any).feature.id === feature.id) {
        const boundingSphere = (primitive as any).boundingSphere;
        if (boundingSphere) {
          viewer.value.camera.flyToBoundingSphere(boundingSphere, {
            duration: duration
          });
        }
        break;
      }
    }
  }

  onUnmounted(() => {
    disablePicking();
  });

  return {
    selectedFeature,
    hoveredFeature,
    isPickingEnabled,
    enablePicking,
    disablePicking,
    clearSelection,
    clearHover,
    selectFeature,
    highlightFeature,
    unhighlightFeature,
    flyToFeature
  };
}

import { shallowRef, onUnmounted } from 'vue';
import type { Ref } from 'vue';
import * as mars3d from 'mars3d';
import type { Viewer3DConfig } from '../types';

export function useViewer3D() {
  const viewer = shallowRef<any | null>(null);
  const map = shallowRef<mars3d.Map | null>(null);
  const isReady = shallowRef<boolean>(false);

  async function initViewer(config: Viewer3DConfig) {
    const container = config.container || 'viewer3dContainer';

    map.value = new mars3d.Map(container, {
      scene: {
        center: { lat: 30.054604, lng: 108.885436, alt: 17036414, heading: 0, pitch: -90 },
        fxaa: true,
        globe: {
          depthTestAgainstTerrain: false
        }
      },
      basemaps: [
        {
          name: '天地图影像',
          type: 'tdt',
          layer: 'img_d',
          show: true
        },
        {
          name: '天地图矢量',
          type: 'tdt',
          layer: 'vec_d',
          show: false
        }
      ],
      terrain: {
        url: 'https://data.mars3d.cn/terrain',
        show: true
      },
      control: {
        contextmenu: { hasDefault: true }
      }
    });

    viewer.value = map.value.viewer;

    // Disable collision detection and configure camera for indoor navigation
    // This prevents camera drift issues with 3D tiles
    viewer.value.scene.screenSpaceCameraController.enableCollisionDetection = false;
    viewer.value.scene.logarithmicDepthBuffer = true;
    viewer.value.camera.frustum.near = 0.1;

    isReady.value = true;
  }

  function destroyViewer() {
    if (map.value) {
      map.value.destroy();
      map.value = null;
    }
    viewer.value = null;
    isReady.value = false;
  }

  onUnmounted(() => {
    destroyViewer();
  });

  return {
    viewer,
    map,
    isReady,
    initViewer,
    destroyViewer
  };
}

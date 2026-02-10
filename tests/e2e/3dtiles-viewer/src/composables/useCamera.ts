import { shallowRef, computed } from 'vue';
import type { Ref } from 'vue';
import type { CameraInfo } from '../types';

export function useCamera(viewer: Ref<Cesium.Viewer | null>) {
  const cameraInfo = shallowRef<CameraInfo>({
    position: { longitude: 0, latitude: 0, height: 0 },
    heading: 0,
    pitch: 0,
    roll: 0,
    fov: 60
  });

  function updateCameraInfo() {
    if (!viewer.value) return;

    const camera = viewer.value.camera;
    const position = camera.positionCartographic;

    cameraInfo.value = {
      position: {
        longitude: (window as any).Cesium.toDegrees(position.longitude),
        latitude: (window as any).Cesium.toDegrees(position.latitude),
        height: position.height
      },
      heading: (window as any).Cesium.toDegrees(camera.heading),
      pitch: (window as any).Cesium.toDegrees(camera.pitch),
      roll: (window as any).Cesium.toDegrees(camera.roll),
      fov: (window as any).Cesium.toDegrees(camera.frustum.fov)
    };
  }

  function flyTo(destination: Cesium.Cartesian3, duration = 3) {
    if (!viewer.value) return;
    viewer.value.camera.flyTo({
      destination: destination,
      duration: duration
    });
  }

  function lookAt(target: Cesium.Cartesian3, offset?: Cesium.HeadingPitchRange) {
    if (!viewer.value) return;
    viewer.value.camera.lookAt(target, offset);
  }

  function resetView() {
    if (!viewer.value) return;
    viewer.value.camera.flyHome();
  }

  function setView(position: Cesium.Cartographic, heading = 0, pitch = -90, roll = 0) {
    if (!viewer.value) return;
    viewer.value.camera.setView({
      destination: Cesium.Cartesian3.fromRadians(position.longitude, position.latitude, position.height),
      orientation: {
        heading: (window as any).Cesium.toRadians(heading),
        pitch: (window as any).Cesium.toRadians(pitch),
        roll: (window as any).Cesium.toRadians(roll)
      }
    });
  }

  return {
    cameraInfo,
    updateCameraInfo,
    flyTo,
    lookAt,
    resetView,
    setView
  };
}

import { defineStore } from 'pinia';
import type { FeatureInfo } from '../types';

interface PropertiesState {
  selectedFeature: FeatureInfo | null;
  isVisible: boolean;
  position: { x: number; y: number };
}

export const usePropertiesStore = defineStore('properties', {
  state: (): PropertiesState => ({
    selectedFeature: null,
    isVisible: false,
    position: { x: 20, y: 20 }
  }),

  actions: {
    setSelectedFeature(feature: FeatureInfo | null) {
      this.selectedFeature = feature;
    },
    setIsVisible(isVisible: boolean) {
      this.isVisible = isVisible;
    },
    setPosition(position: { x: number; y: number }) {
      this.position = position;
    },
    clearSelectedFeature() {
      this.selectedFeature = null;
    }
  }
});

import { defineConfig } from "vite";
import vue from "@vitejs/plugin-vue";
import { mars3dPlugin } from "vite-plugin-mars3d";
import path from "path";

export default defineConfig({
  plugins: [
    vue(),
    mars3dPlugin() as any
  ],
  resolve: {
    alias: {
      '@': path.resolve(__dirname, './src'),
    },
  },
  server: {
    port: 5173,
    open: true
  },
  build: {
    rollupOptions: {
      output: {
        manualChunks: {
          'cesium': ['mars3d-cesium'],
          'mars3d': ['mars3d']
        }
      }
    },
    chunkSizeWarningLimit: 10000
  }
});

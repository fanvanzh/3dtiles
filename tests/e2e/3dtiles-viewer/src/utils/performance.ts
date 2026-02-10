export interface PerformanceData {
  fps: number;
  frameTime: number;
  drawCalls: number;
  triangles: number;
  vertices: number;
  textures: number;
  materials: number;
  memory: {
    used: number;
    total: number;
    limit: number;
  };
  tiles: number;
  cache: {
    bytes: number;
    items: number;
  };
}

export function calculateFps(frameCount: number, timeElapsed: number): number {
  if (timeElapsed === 0) return 0;
  return (frameCount / timeElapsed) * 1000;
}

export function calculateFrameTime(fps: number): number {
  if (fps === 0) return 0;
  return 1000 / fps;
}

export function formatFps(fps: number): string {
  return Math.round(fps).toString();
}

export function getFpsRating(fps: number): 'good' | 'warning' | 'poor' {
  if (fps >= 30) return 'good';
  if (fps >= 15) return 'warning';
  return 'poor';
}

export function getFpsColor(fps: number): string {
  const rating = getFpsRating(fps);
  switch (rating) {
    case 'good':
      return '#4ade80';
    case 'warning':
      return '#fbbf24';
    case 'poor':
      return '#f87171';
    default:
      return '#fff';
  }
}

export function formatMemoryBytes(bytes: number): string {
  if (bytes === 0) return '0 B';

  const k = 1024;
  const sizes = ['B', 'KB', 'MB', 'GB', 'TB'];
  const i = Math.floor(Math.log(bytes) / Math.log(k));

  return `${(bytes / Math.pow(k, i)).toFixed(2)} ${sizes[i]}`;
}

export function formatMemoryPercentage(used: number, total: number): string {
  if (total === 0) return '0%';
  return `${((used / total) * 100).toFixed(1)}%`;
}

export function getMemoryUsageRating(percentage: number): 'good' | 'warning' | 'poor' {
  if (percentage < 50) return 'good';
  if (percentage < 80) return 'warning';
  return 'poor';
}

export function getMemoryUsageColor(percentage: number): string {
  const rating = getMemoryUsageRating(percentage);
  switch (rating) {
    case 'good':
      return '#4ade80';
    case 'warning':
      return '#fbbf24';
    case 'poor':
      return '#f87171';
    default:
      return '#fff';
  }
}

export function formatDrawCalls(drawCalls: number): string {
  return drawCalls.toLocaleString();
}

export function formatTriangles(triangles: number): string {
  if (triangles >= 1000000) {
    return `${(triangles / 1000000).toFixed(2)}M`;
  }
  if (triangles >= 1000) {
    return `${(triangles / 1000).toFixed(2)}K`;
  }
  return triangles.toString();
}

export function formatVertices(vertices: number): string {
  if (vertices >= 1000000) {
    return `${(vertices / 1000000).toFixed(2)}M`;
  }
  if (vertices >= 1000) {
    return `${(vertices / 1000).toFixed(2)}K`;
  }
  return vertices.toString();
}

export function formatTextures(textures: number): string {
  return textures.toString();
}

export function formatMaterials(materials: number): string {
  return materials.toString();
}

export function formatTiles(tiles: number): string {
  return tiles.toString();
}

export function formatCacheBytes(bytes: number): string {
  return formatMemoryBytes(bytes);
}

export function formatCacheItems(items: number): string {
  return items.toString();
}

export function createPerformanceReport(data: PerformanceData): string {
  const lines: string[] = [];
  
  lines.push('=== Performance Report ===');
  lines.push(`FPS: ${formatFps(data.fps)} (${getFpsRating(data.fps)})`);
  lines.push(`Frame Time: ${data.frameTime.toFixed(2)}ms`);
  lines.push(`Draw Calls: ${formatDrawCalls(data.drawCalls)}`);
  lines.push(`Triangles: ${formatTriangles(data.triangles)}`);
  lines.push(`Vertices: ${formatVertices(data.vertices)}`);
  lines.push(`Textures: ${formatTextures(data.textures)}`);
  lines.push(`Materials: ${formatMaterials(data.materials)}`);
  lines.push(`Memory: ${formatMemoryBytes(data.memory.used)} / ${formatMemoryBytes(data.memory.total)} (${formatMemoryPercentage(data.memory.used, data.memory.total)})`);
  lines.push(`Tiles: ${formatTiles(data.tiles)}`);
  lines.push(`Cache: ${formatCacheBytes(data.cache.bytes)} (${formatCacheItems(data.cache.items)} items)`);
  
  return lines.join('\n');
}

export function exportPerformanceData(data: PerformanceData): string {
  return JSON.stringify(data, null, 2);
}

export function comparePerformance(
  current: PerformanceData,
  previous: PerformanceData
): {
  fps: number;
  drawCalls: number;
  triangles: number;
  memory: number;
} {
  return {
    fps: current.fps - previous.fps,
    drawCalls: current.drawCalls - previous.drawCalls,
    triangles: current.triangles - previous.triangles,
    memory: current.memory.used - previous.memory.used
  };
}

export function formatPerformanceDelta(
  delta: number,
  formatFn: (value: number) => string
): string {
  const sign = delta > 0 ? '+' : '';
  return `${sign}${formatFn(delta)}`;
}

export function getPerformanceScore(data: PerformanceData): number {
  let score = 100;
  
  const fpsRating = getFpsRating(data.fps);
  if (fpsRating === 'warning') score -= 20;
  if (fpsRating === 'poor') score -= 40;
  
  const memoryPercentage = (data.memory.used / data.memory.total) * 100;
  const memoryRating = getMemoryUsageRating(memoryPercentage);
  if (memoryRating === 'warning') score -= 15;
  if (memoryRating === 'poor') score -= 30;
  
  return Math.max(0, score);
}

export function getPerformanceGrade(score: number): 'A' | 'B' | 'C' | 'D' | 'F' {
  if (score >= 90) return 'A';
  if (score >= 75) return 'B';
  if (score >= 60) return 'C';
  if (score >= 40) return 'D';
  return 'F';
}

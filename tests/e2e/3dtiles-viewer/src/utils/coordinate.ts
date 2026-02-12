export function degreesToRadians(degrees: number): number {
  return (window as any).Cesium.toRadians(degrees);
}

export function radiansToDegrees(radians: number): number {
  return (window as any).Cesium.toDegrees(radians);
}

export function formatLongitude(longitude: number, decimals = 6): string {
  const normalized = ((longitude + 540) % 360) - 180;
  const direction = normalized >= 0 ? 'E' : 'W';
  return `${Math.abs(normalized).toFixed(decimals)}°${direction}`;
}

export function formatLatitude(latitude: number, decimals = 6): string {
  const direction = latitude >= 0 ? 'N' : 'S';
  return `${Math.abs(latitude).toFixed(decimals)}°${direction}`;
}

export function formatAltitude(altitude: number, unit: 'm' | 'km' | 'ft' = 'm', decimals = 1): string {
  let value = altitude;

  if (unit === 'km') {
    value = altitude / 1000;
  } else if (unit === 'ft') {
    value = altitude * 3.28084;
  }

  return `${value.toFixed(decimals)}${unit}`;
}

export function formatCoordinate(longitude: number, latitude: number, altitude?: number): string {
  const lng = formatLongitude(longitude);
  const lat = formatLatitude(latitude);

  if (altitude !== undefined) {
    const alt = formatAltitude(altitude);
    return `${lat}, ${lng}, ${alt}`;
  }

  return `${lat}, ${lng}`;
}

export function cartesian3ToCartographic(cartesian: any): any {
  return (window as any).Cesium.Cartographic.fromCartesian(cartesian);
}

export function cartographicToDegrees(cartographic: any): {
  longitude: number;
  latitude: number;
  height: number;
} {
  return {
    longitude: radiansToDegrees(cartographic.longitude),
    latitude: radiansToDegrees(cartographic.latitude),
    height: cartographic.height
  };
}

export function degreesToCartesian3(
  longitude: number,
  latitude: number,
  height = 0
): any {
  return (window as any).Cesium.Cartesian3.fromRadians(
    degreesToRadians(longitude),
    degreesToRadians(latitude),
    height
  );
}

export function calculateDistance(
  point1: any,
  point2: any
): number {
  return (window as any).Cesium.Cartesian3.distance(point1, point2);
}

export function calculateBearing(
  start: any,
  end: any
): number {
  const startLon = start.longitude;
  const startLat = start.latitude;
  const endLon = end.longitude;
  const endLat = end.latitude;

  const y = Math.sin(endLon - startLon) * Math.cos(endLat);
  const x =
    Math.cos(startLat) * Math.sin(endLat) -
    Math.sin(startLat) * Math.cos(endLat) * Math.cos(endLon - startLon);

  const bearing = Math.atan2(y, x);
  return radiansToDegrees(bearing);
}

export function formatBearing(bearing: number): string {
  const normalized = ((bearing % 360) + 360) % 360;
  return `${normalized.toFixed(1)}°`;
}

export function formatHeading(heading: number): string {
  return formatBearing(heading);
}

export function formatPitch(pitch: number, decimals = 1): string {
  return `${pitch.toFixed(decimals)}°`;
}

export function formatRoll(roll: number, decimals = 1): string {
  return `${roll.toFixed(decimals)}°`;
}

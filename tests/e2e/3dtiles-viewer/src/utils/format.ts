export function formatNumber(value: number, decimals = 2): string {
  return value.toFixed(decimals);
}

export function formatInteger(value: number): string {
  return Math.round(value).toString();
}

export function formatPercentage(value: number, decimals = 1): string {
  return `${value.toFixed(decimals)}%`;
}

export function formatBytes(bytes: number): string {
  if (bytes === 0) return '0 B';

  const k = 1024;
  const sizes = ['B', 'KB', 'MB', 'GB', 'TB'];
  const i = Math.floor(Math.log(bytes) / Math.log(k));

  return `${(bytes / Math.pow(k, i)).toFixed(2)} ${sizes[i]}`;
}

export function formatMemory(bytes: number): string {
  return formatBytes(bytes);
}

export function formatFps(fps: number): string {
  return Math.round(fps).toString();
}

export function formatDuration(seconds: number): string {
  if (seconds < 60) {
    return `${seconds.toFixed(1)}s`;
  }
  if (seconds < 3600) {
    const minutes = Math.floor(seconds / 60);
    const remainingSeconds = (seconds % 60).toFixed(0);
    return `${minutes}m ${remainingSeconds}s`;
  }
  const hours = Math.floor(seconds / 3600);
  const minutes = Math.floor((seconds % 3600) / 60);
  return `${hours}h ${minutes}m`;
}

export function formatTimestamp(timestamp: number): string {
  const date = new Date(timestamp);
  return date.toLocaleString();
}

export function formatIsoTimestamp(timestamp: number): string {
  const date = new Date(timestamp);
  return date.toISOString();
}

export function truncateString(str: string, maxLength: number, suffix = '...'): string {
  if (str.length <= maxLength) return str;
  return str.substring(0, maxLength - suffix.length) + suffix;
}

export function capitalizeFirst(str: string): string {
  if (!str) return str;
  return str.charAt(0).toUpperCase() + str.slice(1);
}

export function camelCaseToTitle(str: string): string {
  return str
    .replace(/([A-Z])/g, ' $1')
    .replace(/^./, (match) => match.toUpperCase())
    .trim();
}

export function snakeCaseToTitle(str: string): string {
  return str
    .split('_')
    .map((word) => capitalizeFirst(word))
    .join(' ');
}

export function kebabCaseToTitle(str: string): string {
  return str
    .split('-')
    .map((word) => capitalizeFirst(word))
    .join(' ');
}

export function formatJson(obj: any, indent = 2): string {
  return JSON.stringify(obj, null, indent);
}

export function formatUrl(url: string, maxLength = 50): string {
  return truncateString(url, maxLength);
}

export function formatVersion(version: string): string {
  return `v${version}`;
}

export function formatBoolean(value: boolean): string {
  return value ? 'true' : 'false';
}

export function formatYesNo(value: boolean): string {
  return value ? 'Yes' : 'No';
}

export function formatOnOff(value: boolean): string {
  return value ? 'On' : 'Off';
}

export function formatEnabledDisabled(value: boolean): string {
  return value ? 'Enabled' : 'Disabled';
}

export function formatArray(arr: any[], separator = ', '): string {
  return arr.join(separator);
}

export function formatObject(obj: Record<string, any>): string {
  return Object.entries(obj)
    .map(([key, value]) => `${key}: ${value}`)
    .join(', ');
}

export function formatNumberWithCommas(value: number): string {
  return value.toLocaleString();
}

export function formatScientific(value: number, decimals = 2): string {
  return value.toExponential(decimals);
}

export function formatFixed(value: number, decimals = 2): string {
  return value.toFixed(decimals);
}

export function formatPrecision(value: number, precision: number): string {
  return value.toPrecision(precision);
}

export function formatCurrency(value: number, currency = 'USD', locale = 'en-US'): string {
  return new Intl.NumberFormat(locale, {
    style: 'currency',
    currency: currency
  }).format(value);
}

export function formatDate(date: Date, options?: Intl.DateTimeFormatOptions): string {
  return date.toLocaleDateString(undefined, options);
}

export function formatTime(date: Date, options?: Intl.DateTimeFormatOptions): string {
  return date.toLocaleTimeString(undefined, options);
}

export function formatDateTime(date: Date, options?: Intl.DateTimeFormatOptions): string {
  return date.toLocaleString(undefined, options);
}

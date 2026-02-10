import type { TilesetConfig } from '../types';

export const TILESETS: Record<string, TilesetConfig> = {
  'default': {
    url: '/output/tileset.json',
    maximumScreenSpaceError: 16,
    skipLevelOfDetail: true,
    skipLevels: 0,
    immediatelyLoadDesiredLevelOfDetail: false,
    loadSiblings: false,
    cullWithChildrenBounds: true
  },
  'block-rab': {
    url: '/output/Data/BlockRAB/tileset.json',
    maximumScreenSpaceError: 16,
    skipLevelOfDetail: true,
    skipLevels: 0,
    immediatelyLoadDesiredLevelOfDetail: false,
    loadSiblings: false,
    cullWithChildrenBounds: true
  },
  'block-ray': {
    url: '/output/Data/BlockRAY/tileset.json',
    maximumScreenSpaceError: 16,
    skipLevelOfDetail: true,
    skipLevels: 0,
    immediatelyLoadDesiredLevelOfDetail: false,
    loadSiblings: false,
    cullWithChildrenBounds: true
  },
  'block-rxa': {
    url: '/output/Data/BlockRXA/tileset.json',
    maximumScreenSpaceError: 16,
    skipLevelOfDetail: true,
    skipLevels: 0,
    immediatelyLoadDesiredLevelOfDetail: false,
    loadSiblings: false,
    cullWithChildrenBounds: true
  },
  'block-rxx': {
    url: '/output/Data/BlockRXX/tileset.json',
    maximumScreenSpaceError: 16,
    skipLevelOfDetail: true,
    skipLevels: 0,
    immediatelyLoadDesiredLevelOfDetail: false,
    loadSiblings: false,
    cullWithChildrenBounds: true
  }
};

export const DEFAULT_TILESET = 'default';

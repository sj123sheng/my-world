// 单区环境视觉资产生成：从 world.json 生成自然地表手绘控制图。

import { deflateSync } from 'node:zlib';
import { mkdir, readFile, writeFile } from 'node:fs/promises';
import { dirname, join, resolve } from 'node:path';
import { fileURLToPath, pathToFileURL } from 'node:url';

function clamp01(value) {
  return Number.isFinite(value) ? Math.max(0, Math.min(1, value)) : 0;
}

function smoothstep(value) {
  const t = clamp01(value);
  return t * t * (3 - 2 * t);
}

function distanceToSegment(x, y, ax, ay, bx, by) {
  const abx = bx - ax;
  const aby = by - ay;
  const length2 = abx * abx + aby * aby;
  const t = length2 > 1e-12
    ? clamp01(((x - ax) * abx + (y - ay) * aby) / length2)
    : 0;
  return Math.hypot(x - (ax + abx * t), y - (ay + aby * t));
}

function featureMask(feature, x, y) {
  const dx = (x - feature.x) / feature.radiusX;
  const dy = (y - feature.y) / feature.radiusY;
  const radial = Math.hypot(dx, dy);
  if (radial >= 1) return 0;
  return smoothstep((1 - radial) / Math.max(feature.feather, 1e-6));
}

function normalizeBytes(weights) {
  const safe = weights.map((weight) => clamp01(weight));
  const sum = safe.reduce((total, weight) => total + weight, 0) || 1;
  const bytes = safe.map((weight) => Math.floor(weight / sum * 255));
  let remainder = 255 - bytes.reduce((total, value) => total + value, 0);
  const order = safe.map((weight, index) => ({ weight, index }))
    .sort((left, right) => right.weight - left.weight);
  let cursor = 0;
  while (remainder > 0) {
    bytes[order[cursor % order.length].index] += 1;
    cursor += 1;
    remainder -= 1;
  }
  return bytes;
}

export function terrainControlPixel(world, x, y) {
  const poiById = new Map(world.pointsOfInterest.map((poi) => [poi.id, poi]));
  let routeDistance = Infinity;
  for (const route of world.routes) {
    const from = poiById.get(route.fromPoiId);
    const to = poiById.get(route.toPoiId);
    routeDistance = Math.min(routeDistance,
      distanceToSegment(x, y, from.x, from.y, to.x, to.y));
  }
  const path = 1 - smoothstep((routeDistance - 0.006) / 0.014);

  let rockMask = 0;
  let shoreMask = 0;
  for (const feature of world.terrainFeatures) {
    const mask = featureMask(feature, x, y);
    if (feature.kind === 'ridge' || feature.featureId.includes('cliff') ||
        feature.featureId.includes('mesa') || feature.featureId.includes('skyline')) {
      rockMask = Math.max(rockMask, mask);
    }
    if (feature.featureId.includes('lake') || feature.featureId.includes('shore')) {
      shoreMask = Math.max(shoreMask, mask);
    }
  }

  let weights = [0.68, 0.22 + shoreMask * 0.42, 0.10 + rockMask * 1.4, 0];
  if (path > 0) {
    const override = path * 0.88;
    weights = weights.map((weight, index) => index === 3
      ? Math.max(weight, override)
      : weight * (1 - override));
  }
  return normalizeBytes(weights);
}

const CRC_TABLE = (() => {
  const table = new Uint32Array(256);
  for (let n = 0; n < 256; ++n) {
    let value = n;
    for (let bit = 0; bit < 8; ++bit) {
      value = (value & 1) ? 0xedb88320 ^ (value >>> 1) : value >>> 1;
    }
    table[n] = value >>> 0;
  }
  return table;
})();

function crc32(buffer) {
  let crc = 0xffffffff;
  for (const byte of buffer) crc = CRC_TABLE[(crc ^ byte) & 0xff] ^ (crc >>> 8);
  return (crc ^ 0xffffffff) >>> 0;
}

function pngChunk(type, data) {
  const typeBytes = Buffer.from(type, 'ascii');
  const body = Buffer.concat([typeBytes, data]);
  const output = Buffer.alloc(12 + data.length);
  output.writeUInt32BE(data.length, 0);
  body.copy(output, 4);
  output.writeUInt32BE(crc32(body), 8 + data.length);
  return output;
}

export function generateControlMapPng(world, size = 256) {
  if (!Number.isInteger(size) || size < 2 || size > 2048) {
    throw new Error('control map size must be an integer in [2, 2048]');
  }
  const rows = Buffer.alloc((size * 4 + 1) * size);
  for (let row = 0; row < size; ++row) {
    const rowOffset = row * (size * 4 + 1);
    rows[rowOffset] = 0;
    for (let column = 0; column < size; ++column) {
      const pixel = terrainControlPixel(world, column / (size - 1), row / (size - 1));
      const offset = rowOffset + 1 + column * 4;
      for (let channel = 0; channel < 4; ++channel) rows[offset + channel] = pixel[channel];
    }
  }
  const ihdr = Buffer.alloc(13);
  ihdr.writeUInt32BE(size, 0);
  ihdr.writeUInt32BE(size, 4);
  ihdr[8] = 8;
  ihdr[9] = 6;
  return Buffer.concat([
    Buffer.from([137, 80, 78, 71, 13, 10, 26, 10]),
    pngChunk('IHDR', ihdr),
    pngChunk('IDAT', deflateSync(rows, { level: 9 })),
    pngChunk('IEND', Buffer.alloc(0)),
  ]);
}

export async function writeEnvironmentVisualAssets(root) {
  const world = JSON.parse(await readFile(join(root, 'assets/world/world.json'), 'utf8'));
  const rawRoot = join(root, 'entry/src/main/resources/rawfile');
  const controlPath = join(rawRoot, world.environmentVisual.terrainMaterial.controlAsset);
  await mkdir(dirname(controlPath), { recursive: true });
  await writeFile(controlPath, generateControlMapPng(world, 256));
}

if (process.argv[1] && pathToFileURL(resolve(process.argv[1])).href === import.meta.url) {
  const root = resolve(dirname(fileURLToPath(import.meta.url)), '../..');
  await writeEnvironmentVisualAssets(root);
  console.log('ENVIRONMENT VISUALS: natural terrain control map written');
}

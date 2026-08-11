// 单区环境视觉资产生成：从 world.json 生成手绘控制图和三段视觉地貌 LOD。
// 资产全部使用世界归一化坐标；玩法碰撞仍由 TerrainHeightfield/显式 OBB 负责。

import { deflateSync } from 'node:zlib';
import { mkdir, readFile, writeFile } from 'node:fs/promises';
import { dirname, join, resolve } from 'node:path';
import { fileURLToPath, pathToFileURL } from 'node:url';

import { writeGlb } from './fetch_environment_assets.mjs';

const TWO_PI = Math.PI * 2;

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

function edgeMountainMask(x, y) {
  const distance = Math.hypot(x - 0.5, y - 0.5);
  return smoothstep((distance - 0.42) / (0.78 - 0.42));
}

function terrainHeight(world, x, y) {
  x = clamp01(x);
  y = clamp01(y);
  let height = 0.016 * Math.sin(TWO_PI * 2 * x) * Math.sin(TWO_PI * 2 * y) +
    0.003 * Math.sin(TWO_PI * 7 * x + 1.3) * Math.cos(TWO_PI * 7 * y + 0.7) +
    0.006 * Math.sin(TWO_PI * 5 * x + 2.1) * Math.sin(TWO_PI * 5 * y * 0.8 + 0.4) +
    0.09 * edgeMountainMask(x, y);
  for (const feature of world.terrainFeatures) {
    const mask = featureMask(feature, x, y);
    if (feature.kind === 'hill') height += mask * feature.amplitude;
    if (feature.kind === 'basin') height += mask * (feature.targetHeight - height);
    if (feature.kind === 'terrace') height += mask * Math.max(0, feature.targetHeight - height);
    if (feature.kind === 'ridge') {
      const u = x * Math.cos(feature.angleRadians) + y * Math.sin(feature.angleRadians);
      const v = -x * Math.sin(feature.angleRadians) + y * Math.cos(feature.angleRadians);
      height += mask * feature.amplitude * Math.sin(TWO_PI * feature.frequency * u + 2.1) *
        Math.sin(TWO_PI * feature.frequency * 0.7 * v + 0.4);
    }
  }
  return height;
}

function addBox(mesh, center, half) {
  const faces = [
    [[1, 0, 0], [1, -1, -1], [1, -1, 1], [1, 1, 1], [1, 1, -1]],
    [[-1, 0, 0], [-1, -1, 1], [-1, -1, -1], [-1, 1, -1], [-1, 1, 1]],
    [[0, 1, 0], [-1, 1, -1], [1, 1, -1], [1, 1, 1], [-1, 1, 1]],
    [[0, -1, 0], [-1, -1, 1], [1, -1, 1], [1, -1, -1], [-1, -1, -1]],
    [[0, 0, 1], [1, -1, 1], [-1, -1, 1], [-1, 1, 1], [1, 1, 1]],
    [[0, 0, -1], [-1, -1, -1], [1, -1, -1], [1, 1, -1], [-1, 1, -1]],
  ];
  for (const [normal, ...corners] of faces) {
    const base = mesh.positions.length / 3;
    for (let index = 0; index < 4; ++index) {
      const corner = corners[index];
      mesh.positions.push(center[0] + corner[0] * half[0],
        center[1] + corner[1] * half[1], center[2] + corner[2] * half[2]);
      mesh.normals.push(...normal);
      mesh.uvs.push(index === 0 || index === 3 ? 0 : 0.5,
        index < 2 ? 0.5 : 1.0);
    }
    mesh.indices.push(base, base + 1, base + 2, base, base + 2, base + 3);
  }
}

function addRock(mesh, world, x, z, radius, height, segments) {
  const ground = terrainHeight(world, x, z);
  const bottomStart = mesh.positions.length / 3;
  for (let ring = 0; ring < 2; ++ring) {
    for (let index = 0; index < segments; ++index) {
      const angle = TWO_PI * index / segments;
      const irregular = 0.82 + 0.18 * Math.sin(index * 2.71 + x * 91 + z * 57);
      const ringRadius = radius * irregular * (ring === 0 ? 1 : 0.58);
      mesh.positions.push(x + Math.cos(angle) * ringRadius,
        ground + (ring === 0 ? 0 : height * 0.68),
        z + Math.sin(angle) * ringRadius);
      const normalLength = Math.hypot(Math.cos(angle), 0.35, Math.sin(angle));
      mesh.normals.push(Math.cos(angle) / normalLength, 0.35 / normalLength,
        Math.sin(angle) / normalLength);
      mesh.uvs.push(index / segments * 0.5, ring === 0 ? 0.5 : 1.0);
    }
  }
  const top = mesh.positions.length / 3;
  mesh.positions.push(x, ground + height, z);
  mesh.normals.push(0, 1, 0);
  mesh.uvs.push(0.25, 0.75);
  for (let index = 0; index < segments; ++index) {
    const next = (index + 1) % segments;
    mesh.indices.push(bottomStart + index, bottomStart + next,
      bottomStart + segments + next);
    mesh.indices.push(bottomStart + index, bottomStart + segments + next,
      bottomStart + segments + index);
    mesh.indices.push(bottomStart + segments + index,
      bottomStart + segments + next, top);
  }
}

function authoredPlacements(cell, lod) {
  const centerX = (cell.bounds[0] + cell.bounds[2]) * 0.5;
  const centerZ = (cell.bounds[1] + cell.bounds[3]) * 0.5;
  const count = [7, 4, 2][lod];
  const placements = [];
  for (let index = 0; index < count; ++index) {
    const side = index % 2 === 0 ? 1 : -1;
    placements.push({
      x: centerX + side * (0.040 + (index % 3) * 0.008),
      z: cell.bounds[1] + 0.018 + index / Math.max(1, count - 1) *
        (cell.bounds[3] - cell.bounds[1] - 0.036),
      radius: 0.010 + (index % 3) * 0.003,
      height: 0.025 + (index % 2) * 0.018,
    });
  }
  if (cell.blockId === 20) {
    placements.push({ x: centerX + 0.035, z: centerZ + 0.035, radius: 0.021, height: 0.075 });
  }
  return placements;
}

export function buildVisualTerrainCellGlb(world, cell, lod) {
  if (![0, 1, 2].includes(lod)) throw new Error('visual terrain LOD must be 0..2');
  const mesh = { positions: [], normals: [], uvs: [], indices: [] };
  const segments = [8, 6, 4][lod];
  for (const placement of authoredPlacements(cell, lod)) {
    addRock(mesh, world, placement.x, placement.z, placement.radius,
      placement.height, segments);
  }
  if (cell.blockId === 20) {
    const x = 0.548;
    const z = 0.342;
    const ground = terrainHeight(world, x, z);
    addBox(mesh, [x - 0.022, ground + 0.035, z], [0.006, 0.035, 0.008]);
    addBox(mesh, [x + 0.022, ground + 0.035, z], [0.006, 0.035, 0.008]);
    if (lod < 2) addBox(mesh, [x, ground + 0.067, z], [0.028, 0.006, 0.008]);
  }

  const positions = Buffer.from(new Float32Array(mesh.positions).buffer);
  const normals = Buffer.from(new Float32Array(mesh.normals).buffer);
  const uvs = Buffer.from(new Float32Array(mesh.uvs).buffer);
  const useUint32 = mesh.positions.length / 3 > 65535;
  const indexArray = useUint32 ? new Uint32Array(mesh.indices) : new Uint16Array(mesh.indices);
  const indices = Buffer.from(indexArray.buffer);
  const chunks = [];
  const views = [];
  let byteOffset = 0;
  for (const buffer of [positions, normals, uvs, indices]) {
    const aligned = (byteOffset + 3) & ~3;
    if (aligned > byteOffset) chunks.push(Buffer.alloc(aligned - byteOffset));
    byteOffset = aligned;
    views.push({ buffer: 0, byteOffset, byteLength: buffer.length });
    chunks.push(buffer);
    byteOffset += buffer.length;
  }
  const bin = Buffer.concat(chunks);
  const vertexCount = mesh.positions.length / 3;
  const axes = [0, 1, 2];
  const minimum = axes.map((axis) => Math.min(...Array.from({ length: vertexCount },
    (_, index) => mesh.positions[index * 3 + axis])));
  const maximum = axes.map((axis) => Math.max(...Array.from({ length: vertexCount },
    (_, index) => mesh.positions[index * 3 + axis])));
  return writeGlb({
    json: {
      asset: { version: '2.0', generator: 'Ethelan authored terrain visual generator' },
      scene: 0,
      scenes: [{ nodes: [0] }],
      nodes: [{ name: `visual_block_${cell.blockId}_lod${lod}`, mesh: 0 }],
      meshes: [{ primitives: [{ attributes: { POSITION: 0, NORMAL: 1, TEXCOORD_0: 2 }, indices: 3 }] }],
      accessors: [
        { bufferView: 0, componentType: 5126, count: vertexCount, type: 'VEC3', min: minimum, max: maximum },
        { bufferView: 1, componentType: 5126, count: vertexCount, type: 'VEC3' },
        { bufferView: 2, componentType: 5126, count: vertexCount, type: 'VEC2' },
        { bufferView: 3, componentType: useUint32 ? 5125 : 5123, count: mesh.indices.length, type: 'SCALAR' },
      ],
      bufferViews: views,
      buffers: [{ byteLength: bin.length }],
    },
    bin,
  });
}

export async function writeEnvironmentVisualAssets(root) {
  const world = JSON.parse(await readFile(join(root, 'assets/world/world.json'), 'utf8'));
  const rawRoot = join(root, 'entry/src/main/resources/rawfile');
  const controlPath = join(rawRoot, world.environmentVisual.terrainMaterial.controlAsset);
  await mkdir(dirname(controlPath), { recursive: true });
  await writeFile(controlPath, generateControlMapPng(world, 256));
  for (const cell of world.environmentVisual.visualTerrainCells) {
    for (let lod = 0; lod < 3; ++lod) {
      const key = ['nearAsset', 'midAsset', 'farAsset'][lod];
      const outputPath = join(rawRoot, cell[key]);
      await mkdir(dirname(outputPath), { recursive: true });
      await writeFile(outputPath, buildVisualTerrainCellGlb(world, cell, lod));
    }
  }
}

if (process.argv[1] && pathToFileURL(resolve(process.argv[1])).href === import.meta.url) {
  const root = resolve(dirname(fileURLToPath(import.meta.url)), '../..');
  await writeEnvironmentVisualAssets(root);
  console.log('ENVIRONMENT VISUALS: control map + 3 authored cells x 3 LOD written');
}

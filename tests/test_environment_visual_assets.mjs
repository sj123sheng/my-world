import assert from 'node:assert/strict';
import { readFile, readdir, stat } from 'node:fs/promises';

import { readGlb } from '../automation/assets/fetch_environment_assets.mjs';
import {
  buildVisualTerrainCellGlb,
  generateControlMapPng,
  terrainControlPixel,
} from '../automation/assets/generate_environment_visuals.mjs';

const world = JSON.parse(await readFile(new URL('../assets/world/world.json', import.meta.url)));

const route = terrainControlPixel(world, 0.51, 0.20);
assert.ok(route[3] > 180, 'main route must dominate the path channel');

const meadow = terrainControlPixel(world, 0.58, 0.08);
assert.ok(meadow[0] > meadow[1], 'spawn meadow must favor grass over soil');
assert.ok(meadow[0] > meadow[2], 'spawn meadow must favor grass over rock');

const cliff = terrainControlPixel(world, 0.56, 0.44);
assert.ok(cliff[2] > cliff[0], 'corridor cliff must favor rock over grass');

for (const pixel of [route, meadow, cliff]) {
  assert.equal(pixel.length, 4);
  assert.ok(pixel.every((value) => Number.isInteger(value) && value >= 0 && value <= 255));
  assert.ok(Math.abs(pixel.reduce((sum, value) => sum + value, 0) - 255) <= 2);
}

const controlPng = generateControlMapPng(world, 64);
assert.deepEqual([...controlPng.subarray(0, 8)], [137, 80, 78, 71, 13, 10, 26, 10]);
assert.ok(controlPng.length > 256);

for (const cell of world.environmentVisual.visualTerrainCells) {
  for (let lod = 0; lod < 3; ++lod) {
    const glb = buildVisualTerrainCellGlb(world, cell, lod);
    const parsed = readGlb(glb);
    assert.equal(parsed.json.asset.version, '2.0');
    assert.equal(parsed.json.scene, 0);
    assert.ok(parsed.json.meshes.length >= 1);
    assert.ok(parsed.json.accessors.some((accessor) => accessor.type === 'VEC3'));
    assert.ok(parsed.json.meshes[0].primitives[0].indices !== undefined);
    assert.ok(glb.length < 1024 * 1024, 'each authored visual LOD stays below 1 MiB');
  }
}

const rawEnvironment = new URL(
  '../entry/src/main/resources/rawfile/environment/', import.meta.url);
const visualFiles = await readdir(new URL('visual_terrain/', rawEnvironment));
const resourceUrls = [
  new URL('terrain_material_atlas.png', rawEnvironment),
  new URL('terrain_control_spawn.png', rawEnvironment),
  new URL('foliage_atlas.png', rawEnvironment),
  ...visualFiles.map((name) => new URL(`visual_terrain/${name}`, rawEnvironment)),
];
assert.equal(world.environmentVisual.foliageAtlasAsset,
  'environment/foliage_atlas.png');
let resourceBytes = 0;
for (const resource of resourceUrls) resourceBytes += (await stat(resource)).size;
assert.ok(resourceBytes <= 80 * 1024 * 1024,
  'single-zone environment visual resources must stay within the 80 MiB ceiling');

console.log('test_environment_visual_assets ok');

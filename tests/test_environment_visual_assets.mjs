import assert from 'node:assert/strict';
import { execFile } from 'node:child_process';
import { access, readFile, stat } from 'node:fs/promises';
import { fileURLToPath } from 'node:url';
import { promisify } from 'node:util';

import { generateControlMapPng, terrainControlPixel } from
  '../automation/assets/generate_environment_visuals.mjs';
import { validateWorldLayout } from '../automation/assets/generate_world_layout.mjs';

const world = JSON.parse(await readFile(new URL('../assets/world/world.json', import.meta.url)));
const schema = JSON.parse(await readFile(new URL(
  '../config/schema/world.schema.json', import.meta.url)));

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

assert.ok(!('environmentBatches' in world));
assert.ok(!('traversalGates' in world));
assert.ok(!('visualTerrainCells' in world.environmentVisual));

const generatedManifest = await readFile(new URL(
  '../entry/src/main/ets/generated/EnvironmentVisualManifest.ets', import.meta.url),
  'utf8');
assert.ok(!generatedManifest.includes('VisualTerrainResource'),
  'runtime manifest must not retain the old visual terrain resource type');
assert.ok(!generatedManifest.includes('VISUAL_TERRAIN_RESOURCES'),
  'runtime manifest must not retain the old visual terrain resource constant');
assert.ok(!generatedManifest.includes('.glb'),
  'runtime visual manifest must not reference authored structure GLBs');
const generatedHeader = await readFile(new URL(
  '../native/generated/world_layout.gen.h', import.meta.url), 'utf8');
assert.ok(!generatedHeader.includes('kTraversalGates'));
assert.ok(!generatedHeader.includes('WorldTraversalGateDef'));
assert.ok(!generatedHeader.includes('WorldVisualTerrainCellDef'));
assert.ok(!generatedHeader.includes('.glb'));

const invalidReward = structuredClone(world);
invalidReward.naturalNodes[0].rewardId = 999;
assert.ok(validateWorldLayout(invalidReward, schema).some((error) =>
  error.includes('unknown reward 999')));
const invalidPrerequisite = structuredClone(world);
invalidPrerequisite.regionTriggers[0].prerequisiteNodeId = 999;
assert.ok(validateWorldLayout(invalidPrerequisite, schema).some((error) =>
  error.includes('unknown prerequisite node 999')));
const artificialField = structuredClone(world);
artificialField.traversalGates = [];
assert.ok(validateWorldLayout(artificialField, schema).some((error) =>
  error.includes('forbidden artificial top-level field: traversalGates')));
for (const key of ['environmentBatches', 'visualTerrainBlocks', 'puzzles']) {
  const invalidTopLevel = structuredClone(world);
  invalidTopLevel[key] = [];
  assert.ok(validateWorldLayout(invalidTopLevel, schema).some((error) =>
    error.includes(`forbidden artificial top-level field: ${key}`)), key);
}
for (const [collection, field, value] of [
  ['naturalNodes', 'halfExtents', [0.01, 0.01]],
  ['naturalNodes', 'yaw', 0],
  ['naturalNodes', 'top', 0.5],
  ['regionTriggers', 'halfExtents', [0.01, 0.01]],
  ['regionTriggers', 'yaw', 0],
  ['regionTriggers', 'top', 0.5],
]) {
  const invalidNaturalObjective = structuredClone(world);
  invalidNaturalObjective[collection][0][field] = value;
  assert.ok(validateWorldLayout(invalidNaturalObjective, schema).some((error) =>
    error.includes(`unexpected property \"${field}\"`)), `${collection}.${field}`);
}

const rawEnvironment = new URL(
  '../entry/src/main/resources/rawfile/environment/', import.meta.url);
const artificialResources = [
  'outer_ring.glb', 'center_rift.glb', 'backdrop.glb', 'decoration.glb',
  ...[9, 11, 12, 14, 18, 19, 21, 28, 34, 41, 44, 54].map((id) =>
    `block_${id}.glb`),
  ...[4, 12, 20].flatMap((id) => [0, 1, 2].map((lod) =>
    `visual_terrain/block_${id}_lod${lod}.glb`)),
];
for (const resource of artificialResources) {
  await assert.rejects(access(new URL(resource, rawEnvironment)),
    { code: 'ENOENT' }, `${resource} must not ship in rawfile`);
}
for (const removedInput of [
  '../assets/environment/manifest.json',
  '../automation/assets/fetch_environment_assets.mjs',
  '../automation/assets/validate_environment_assets.mjs',
]) {
  await assert.rejects(access(new URL(removedInput, import.meta.url)),
    { code: 'ENOENT' }, `${removedInput} must not recreate artificial GLBs`);
}
const visualGenerator = await readFile(new URL(
  '../automation/assets/generate_environment_visuals.mjs', import.meta.url), 'utf8');
for (const obsoleteToken of ['writeGlb', 'visualTerrainCells',
  'buildVisualTerrainCellGlb']) {
  assert.ok(!visualGenerator.includes(obsoleteToken),
    `natural visual generator must not retain ${obsoleteToken}`);
}
const resourceUrls = [
  new URL('terrain_material_atlas.png', rawEnvironment),
  new URL('terrain_control_spawn.png', rawEnvironment),
  new URL('foliage_atlas.png', rawEnvironment),
];
assert.equal(world.environmentVisual.foliageAtlasAsset,
  'environment/foliage_atlas.png');
let resourceBytes = 0;
for (const resource of resourceUrls) resourceBytes += (await stat(resource)).size;
assert.ok(resourceBytes <= 80 * 1024 * 1024,
  'single-zone environment visual resources must stay within the 80 MiB ceiling');

const generatedOutputs = [
  new URL('../native/generated/world_layout.gen.h', import.meta.url),
  new URL('../entry/src/main/ets/generated/EnvironmentVisualManifest.ets', import.meta.url),
  new URL('../entry/src/main/resources/rawfile/environment/terrain_control_spawn.png',
    import.meta.url),
];
const beforeGeneration = await Promise.all(generatedOutputs.map((path) => readFile(path)));
const runGenerator = promisify(execFile);
const root = fileURLToPath(new URL('..', import.meta.url));
await runGenerator(process.execPath, ['automation/assets/generate_world_layout.mjs'], { cwd: root });
await runGenerator(process.execPath,
  ['automation/assets/generate_environment_visuals.mjs'], { cwd: root });
const afterFirstGeneration = await Promise.all(generatedOutputs.map((path) => readFile(path)));
await runGenerator(process.execPath, ['automation/assets/generate_world_layout.mjs'], { cwd: root });
await runGenerator(process.execPath,
  ['automation/assets/generate_environment_visuals.mjs'], { cwd: root });
const afterSecondGeneration = await Promise.all(generatedOutputs.map((path) => readFile(path)));
assert.deepEqual(afterFirstGeneration, beforeGeneration,
  'first generator run must leave tracked outputs byte-identical');
assert.deepEqual(afterSecondGeneration, afterFirstGeneration,
  'second generator run must be byte-identical to the first');

console.log('test_environment_visual_assets ok');

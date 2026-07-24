import assert from 'node:assert/strict';
import { createHash } from 'node:crypto';
import { readFile } from 'node:fs/promises';
import {
  bakeNodeTransforms,
  chooseDiffuseDependency,
  chooseGltfDownload,
  embedTextureLevels,
  mergePrimitivesByMaterial,
  partitionNodes,
  readGlb,
  sha256,
  writeGlb,
} from '../automation/assets/fetch_environment_assets.mjs';

const triangleBin = Buffer.alloc(78);
new Float32Array(triangleBin.buffer, triangleBin.byteOffset, 9).set([
  0, 0, 0, 1, 0, 0, 0, 1, 0,
]);
new Float32Array(triangleBin.buffer, triangleBin.byteOffset + 36, 9).set([
  0, 0, 1, 0, 0, 1, 0, 0, 1,
]);
new Uint16Array(triangleBin.buffer, triangleBin.byteOffset + 72, 3).set([0, 1, 2]);
const triangleDocument = {
  json: {
    asset: { version: '2.0' },
    scene: 0,
    scenes: [{ nodes: [0] }],
    nodes: [{ name: 'triangle', mesh: 0 }],
    meshes: [{ primitives: [{
      attributes: { POSITION: 0, NORMAL: 1 },
      indices: 2,
      material: 0,
    }] }],
    materials: [{ name: 'stone' }],
    accessors: [
      { bufferView: 0, componentType: 5126, count: 3, type: 'VEC3' },
      { bufferView: 1, componentType: 5126, count: 3, type: 'VEC3' },
      { bufferView: 2, componentType: 5123, count: 3, type: 'SCALAR' },
    ],
    bufferViews: [
      { buffer: 0, byteOffset: 0, byteLength: 36 },
      { buffer: 0, byteOffset: 36, byteLength: 36 },
      { buffer: 0, byteOffset: 72, byteLength: 6 },
    ],
    buffers: [{ byteLength: triangleBin.length }],
  },
  bin: triangleBin,
};

assert.equal(sha256(Buffer.from('ruins')), createHash('sha256').update('ruins').digest('hex'));
assert.equal(chooseGltfDownload({
  z: { url: 'https://dl.polyhaven.org/z_2k.glb' },
  a: { url: 'https://dl.polyhaven.org/a_2k.gltf' },
}), 'https://dl.polyhaven.org/a_2k.gltf');
assert.throws(() => chooseGltfDownload({}, '2k'), /no 2k glTF download/);
assert.equal(chooseDiffuseDependency([
  { path: 'textures/ruins_nor_2k.jpg' },
  { path: 'textures/ruins_diff_2k.jpg' },
], 'ruins').path, 'textures/ruins_diff_2k.jpg');
assert.throws(() => chooseDiffuseDependency([], 'ruins'), /no ruins 2K diffuse dependency/);
assert.throws(() => chooseDiffuseDependency([
  { path: 'textures/ruins_diff_2k.jpg' },
  { path: 'textures/ruins_diff_2k.png' },
], 'ruins'), /ambiguous ruins 2K diffuse dependencies/);

const roundTrip = readGlb(writeGlb(triangleDocument));
assert.equal(roundTrip.json.asset.version, '2.0');
assert.deepEqual(roundTrip.bin.subarray(0, triangleBin.length), triangleBin);
assert.throws(() => readGlb(Buffer.alloc(20)), /magic/);

const partitioned = partitionNodes(triangleDocument, [{
  id: 'placed-triangle',
  sourceNode: 'triangle',
  region: 'outerRing',
  translation: [2, 0, 0],
  rotation: [0, 0, 0, 1],
  scale: [2, 1, 1],
}]);
assert.equal(partitioned.outerRing.json.nodes[0].name, 'placed-triangle');
assert.throws(() => partitionNodes(triangleDocument, [{
  id: 'bad-region', sourceNode: 'triangle', region: 'unknown',
  translation: [0, 0, 0], rotation: [0, 0, 0, 1], scale: [1, 1, 1],
}]), /unknown region/);

bakeNodeTransforms(partitioned.outerRing);
assert.deepEqual(partitioned.outerRing.json.nodes[0].matrix,
  [1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1]);
mergePrimitivesByMaterial(partitioned.outerRing);
assert.equal(partitioned.outerRing.json.meshes[0].primitives.length, 1);
embedTextureLevels(partitioned.outerRing, Buffer.from([0xff, 0xd8, 0xff, 0xd9]),
  Buffer.from([0x89, 0x50, 0x4e, 0x47]));
assert.deepEqual(partitioned.outerRing.json.images.map((image) => image.name),
  ['diffuse_full', 'diffuse_half']);

const manifestUrl = new URL('../assets/environment/manifest.json', import.meta.url);
const manifest = JSON.parse(await readFile(manifestUrl, 'utf8'));
const layoutUrl = new URL('../assets/environment/layout.json', import.meta.url);
const layout = JSON.parse(await readFile(layoutUrl, 'utf8'));

assert.equal(manifest.license, 'CC0-1.0');
assert.equal(manifest.licenseUrl, 'https://polyhaven.com/license');
assert.deepEqual(manifest.sourceAssets.map((asset) => asset.id),
  ['modular_fort_01', 'rabdentse_ruins_wall']);

const findDownloadRecord = (node, url) => {
  if (!node || typeof node !== 'object') return undefined;
  if (node.url === url) return node;
  for (const value of Object.values(node)) {
    const found = findDownloadRecord(value, url);
    if (found) return found;
  }
  return undefined;
};

for (const source of manifest.sourceAssets) {
  assert.match(source.author, /\S/);
  assert.match(source.pageUrl, /^https:\/\/polyhaven\.com\/a\//);
  assert.match(source.downloadUrl, /^https:\/\/dl\.polyhaven\.org\//);
  assert.match(source.sha256, /^[a-f0-9]{64}$/);
  assert.match(source.downloadedAt, /^\d{4}-\d{2}-\d{2}$/);
  assert.ok(Array.isArray(source.sourceFiles), `${source.id}: sourceFiles`);
  assert.ok(source.sourceFiles.length > 1, `${source.id}: sourceFiles`);
  assert.deepEqual(source.sourceFiles.map((file) => file.path),
    [...source.sourceFiles.map((file) => file.path)].sort(),
    `${source.id}: sourceFiles must be sorted by path`);
  const indexResponse = await fetch(`https://api.polyhaven.com/files/${source.id}`);
  assert.equal(indexResponse.ok, true, `${source.id}: metadata HTTP ${indexResponse.status}`);
  const index = await indexResponse.json();
  const downloadRecord = findDownloadRecord(index, source.downloadUrl);
  assert.ok(downloadRecord, `${source.id}: download metadata record`);
  const primaryPath = new URL(source.downloadUrl).pathname.split('/').at(-1);
  const expectedFiles = [
    { path: primaryPath, url: source.downloadUrl },
    ...Object.entries(downloadRecord.include ?? {}).map(([path, entry]) =>
      ({ path, url: entry.url })),
  ].sort((left, right) => left.path < right.path ? -1 : left.path > right.path ? 1 : 0);
  assert.deepEqual(source.sourceFiles.map(({ path, url }) => ({ path, url })),
    expectedFiles, `${source.id}: all API include files must be recorded`);
  await Promise.all(source.sourceFiles.map(async (file) => {
    assert.match(file.path, /^(?!\/)(?!.*(?:^|\/)\.\.(?:\/|$)).+\S$/);
    assert.match(file.url, /^https:\/\/dl\.polyhaven\.org\//);
    assert.match(file.downloadedAt, /^\d{4}-\d{2}-\d{2}$/);
    assert.match(file.sha256, /^[a-f0-9]{64}$/);
    const response = await fetch(file.url);
    assert.equal(response.ok, true, `${file.path}: HTTP ${response.status}`);
    const bytes = Buffer.from(await response.arrayBuffer());
    assert.equal(createHash('sha256').update(bytes).digest('hex'), file.sha256,
      `${source.id}/${file.path}: downloaded SHA-256`);
  }));
  const primary = source.sourceFiles.find((file) => file.url === source.downloadUrl);
  assert.ok(primary, `${source.id}: primary glTF sourceFiles entry`);
  assert.equal(primary.sha256, source.sha256, `${source.id}: primary SHA-256`);
  assert.equal(primary.downloadedAt, source.downloadedAt, `${source.id}: primary date`);
}

const outerRingPillars = layout.filter((placement) =>
  placement.region === 'outerRing' &&
  placement.sourceNode === 'modular_fort_01_tower_round' &&
  /pillar/.test(placement.id));
assert.ok(outerRingPillars.length > 0,
  'outerRing must place modular_fort_01_tower_round (Cylinder) as an explicit pillar');

for (const asset of manifest.derivedAssets) {
  const bytes = await readFile(asset.path);
  const digest = createHash('sha256').update(bytes).digest('hex');
  assert.equal(digest, asset.sha256, asset.path);
  assert.ok(bytes.length > 20, asset.path);
  assert.equal(bytes.subarray(0, 4).toString('ascii'), 'glTF', asset.path);
}

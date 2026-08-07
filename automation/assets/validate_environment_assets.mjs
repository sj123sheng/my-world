import { createHash } from 'node:crypto';
import { readFile } from 'node:fs/promises';

const manifest = JSON.parse(await readFile('assets/environment/manifest.json', 'utf8'));
if (manifest.license !== 'CC0-1.0' ||
    manifest.licenseUrl !== 'https://polyhaven.com/license') {
  throw new Error('environment manifest must declare Poly Haven CC0-1.0');
}
for (const entry of [...manifest.sourceAssets, ...manifest.derivedAssets]) {
  if (!/^[a-f0-9]{64}$/.test(entry.sha256)) {
    throw new Error(`${entry.id ?? entry.path}: invalid SHA-256`);
  }
}
const comparePaths = (left, right) => left < right ? -1 : left > right ? 1 : 0;
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
  if (!Array.isArray(source.sourceFiles) || source.sourceFiles.length < 2) {
    throw new Error(`${source.id}: sourceFiles must include the primary glTF and dependencies`);
  }
  const paths = source.sourceFiles.map((file) => file.path);
  if (new Set(paths).size !== paths.length ||
      paths.some((path) => typeof path !== 'string' || path.startsWith('/') ||
        path.split('/').some((part) => part.length === 0 || part === '.' || part === '..'))) {
    throw new Error(`${source.id}: invalid or duplicate sourceFiles path`);
  }
  if (paths.some((path, index) => index > 0 && comparePaths(paths[index - 1], path) > 0)) {
    throw new Error(`${source.id}: sourceFiles must be sorted by path`);
  }
  const metadataResponse = await fetch(`https://api.polyhaven.com/files/${source.id}`);
  if (!metadataResponse.ok) {
    throw new Error(`${source.id}: metadata HTTP ${metadataResponse.status}`);
  }
  const downloadRecord = findDownloadRecord(await metadataResponse.json(), source.downloadUrl);
  if (!downloadRecord) throw new Error(`${source.id}: download metadata record missing`);
  const expectedFiles = [
    {
      path: new URL(source.downloadUrl).pathname.split('/').at(-1),
      url: source.downloadUrl,
    },
    ...Object.entries(downloadRecord.include ?? {}).map(([path, entry]) =>
      ({ path, url: entry.url })),
  ].sort((left, right) => comparePaths(left.path, right.path));
  const recordedFiles = source.sourceFiles.map(({ path, url }) => ({ path, url }));
  if (JSON.stringify(recordedFiles) !== JSON.stringify(expectedFiles)) {
    throw new Error(`${source.id}: sourceFiles do not cover the selected glTF package`);
  }
  await Promise.all(source.sourceFiles.map(async (file) => {
    if (!file.url.startsWith('https://dl.polyhaven.org/') ||
        !/^\d{4}-\d{2}-\d{2}$/.test(file.downloadedAt) ||
        !/^[a-f0-9]{64}$/.test(file.sha256)) {
      throw new Error(`${source.id}/${file.path}: invalid source file evidence`);
    }
    const response = await fetch(file.url);
    if (!response.ok) throw new Error(`${file.path}: download HTTP ${response.status}`);
    const bytes = Buffer.from(await response.arrayBuffer());
    const actual = createHash('sha256').update(bytes).digest('hex');
    if (actual !== file.sha256) {
      throw new Error(`${source.id}/${file.path}: downloaded SHA-256 mismatch`);
    }
  }));
  const primary = source.sourceFiles.find((file) => file.url === source.downloadUrl);
  if (!primary || primary.sha256 !== source.sha256 ||
      primary.downloadedAt !== source.downloadedAt) {
    throw new Error(`${source.id}: primary source evidence mismatch`);
  }
}
for (const derived of manifest.derivedAssets) {
  const bytes = await readFile(derived.path);
  const actual = createHash('sha256').update(bytes).digest('hex');
  if (actual !== derived.sha256) throw new Error(`${derived.path}: SHA-256 mismatch`);
}

// Phase 2 追加校验：manifest 中登记的 block_<id> 产物必须与 layout.json
// 的 blockId 分组对应（未烘焙的区块允许缺席 manifest，但已登记的不能虚构）。
const BLOCK_COUNT = 64;
const layout = JSON.parse(await readFile('assets/environment/layout.json', 'utf8'));
const placements = Array.isArray(layout) ? layout : layout?.placements;
if (!Array.isArray(placements)) throw new Error('layout.json must contain placements');
const layoutBlockIds = new Set(placements.map((entry) => entry.blockId ?? -1));
for (const blockId of layoutBlockIds) {
  if (!Number.isInteger(blockId) || blockId < -1 || blockId >= BLOCK_COUNT) {
    throw new Error(`layout.json: blockId must be an integer in [-1, ${BLOCK_COUNT})`);
  }
}
for (const derived of manifest.derivedAssets) {
  const match = /^block_(\d+)$/.exec(derived.id ?? '');
  if (!match) continue;
  const blockId = Number(match[1]);
  if (blockId >= BLOCK_COUNT) throw new Error(`${derived.id}: block id out of range`);
  if (!derived.path.endsWith(`/environment/block_${blockId}.glb`)) {
    throw new Error(`${derived.id}: path must be environment/block_${blockId}.glb`);
  }
  if (!layoutBlockIds.has(blockId)) {
    throw new Error(`${derived.id}: no layout placement references this blockId`);
  }
}

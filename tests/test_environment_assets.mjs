import assert from 'node:assert/strict';
import { access, readFile, readdir } from 'node:fs/promises';

for (const removedPath of [
  '../assets/environment/manifest.json',
  '../automation/assets/fetch_environment_assets.mjs',
  '../automation/assets/validate_environment_assets.mjs',
]) {
  await assert.rejects(access(new URL(removedPath, import.meta.url)), { code: 'ENOENT' });
}

const rawEnvironment = new URL(
  '../entry/src/main/resources/rawfile/environment/', import.meta.url);
const rawNames = await readdir(rawEnvironment, { recursive: true });
assert.ok(rawNames.every((name) => !name.endsWith('.glb')),
  'rawfile environment tree must contain no authored GLB');

const generator = await readFile(new URL(
  '../automation/assets/generate_environment_visuals.mjs', import.meta.url), 'utf8');
assert.ok(!/fort|ruin|wall|writeGlb|visualTerrain/i.test(generator),
  'natural asset generator must not retain artificial asset generation');

const readme = await readFile(new URL('../README.md', import.meta.url), 'utf8');
assert.ok(!readme.includes('`assets/environment/manifest.json`'),
  'README must not link the removed authored-environment manifest');
assert.ok(!readme.includes('`outer_ring.glb`'),
  'README must not present removed authored GLBs as current assets');
assert.match(readme, /历史验收[\s\S]*assets\/environment\/LICENSES\.md/,
  'README must label the old visual benchmark as history and retain license provenance');

console.log('test_environment_assets ok');

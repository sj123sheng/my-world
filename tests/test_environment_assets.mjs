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

console.log('test_environment_assets ok');

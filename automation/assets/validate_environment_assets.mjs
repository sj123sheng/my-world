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
for (const derived of manifest.derivedAssets) {
  const bytes = await readFile(derived.path);
  const actual = createHash('sha256').update(bytes).digest('hex');
  if (actual !== derived.sha256) throw new Error(`${derived.path}: SHA-256 mismatch`);
}

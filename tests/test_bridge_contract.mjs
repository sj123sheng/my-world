import assert from 'node:assert/strict';
import fs from 'node:fs';

const bridge = fs.readFileSync('entry/src/main/ets/napi/Bridge.ets', 'utf8');
const page = fs.readFileSync('entry/src/main/ets/pages/GamePage.ets', 'utf8');
const nativeBridge = fs.readFileSync('entry/src/main/cpp/native_bridge.cpp', 'utf8');

const snapshotInterface = bridge.match(/export interface Snapshot \{([\s\S]*?)\n\}/);
assert(snapshotInterface, 'Bridge must declare Snapshot');
const fields = [...snapshotInterface[1].matchAll(/^\s{2}(\w+): [^;]+;/gm)].map((match) => match[1]);
const initializer = page.match(/private snapshot: Snapshot = \{([\s\S]*?)\};/);
assert(initializer, 'GamePage must initialize Snapshot');
for (const field of fields) {
  assert.match(initializer[1], new RegExp(`\\b${field}\\s*:`), `initial Snapshot missing ${field}`);
}

for (const field of ['tick', 'targetId', 'bossPhase']) {
  assert.match(page, new RegExp(`this\\.${field}\\s*=\\s*this\\.snapshot\\.${field}`),
    `GamePage polling must assign ${field}`);
}

assert.match(nativeBridge, /argc != 1/, 'NativePushInput must reject wrong argument count');
assert.match(nativeBridge, /argumentType != napi_object/, 'NativePushInput must require an object');
for (const field of ['type', 'x', 'y']) {
  assert.match(nativeBridge, new RegExp(`GetNumberProperty\\(env, args\\[0\\], "${field}", true`),
    `NativePushInput must require numeric ${field}`);
}
assert.match(nativeBridge, /GetNumberProperty\(env, args\[0\], "pointerId", false/,
  'NativePushInput must validate optional pointerId');
assert.match(nativeBridge, /napi_throw_type_error/, 'NativePushInput must throw a type error');

const surfaceChanged = nativeBridge.match(/static void OnSurfaceChanged[\s\S]*?\n\}/);
assert(surfaceChanged, 'native bridge must define OnSurfaceChanged');
assert.equal((surfaceChanged[0].match(/InvalidateSurfaceSnapshot\(\);/g) ?? []).length, 2,
  'surface init and resize failures must both invalidate the renderer snapshot');

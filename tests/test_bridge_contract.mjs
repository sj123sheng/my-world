import assert from 'node:assert/strict';
import fs from 'node:fs';

const bridge = fs.readFileSync('entry/src/main/ets/napi/Bridge.ets', 'utf8');
const page = fs.readFileSync('entry/src/main/ets/pages/GamePage.ets', 'utf8');
const ability = fs.readFileSync('entry/src/main/ets/EntryAbility.ets', 'utf8');
const nativeBridge = fs.readFileSync('entry/src/main/cpp/native_bridge.cpp', 'utf8');
const loop = fs.readFileSync('native/engine/core/loop.cpp', 'utf8');

function functionBody(source, signature) {
  const start = source.indexOf(signature);
  assert.notEqual(start, -1, `missing function: ${signature}`);
  const open = source.indexOf('{', start);
  let depth = 0;
  for (let index = open; index < source.length; index++) {
    if (source[index] === '{') depth++;
    if (source[index] === '}' && --depth === 0) return source.slice(open + 1, index);
  }
  assert.fail(`unterminated function: ${signature}`);
}

const snapshotInterface = bridge.match(/export interface Snapshot \{([\s\S]*?)\n\}/);
assert(snapshotInterface, 'Bridge must declare Snapshot');
const fields = [...snapshotInterface[1].matchAll(/^\s{2}(\w+): [^;]+;/gm)].map((match) => match[1]);
const initializer = page.match(/private snapshot: Snapshot = \{([\s\S]*?)\};/);
assert(initializer, 'GamePage must initialize Snapshot');
for (const field of fields) {
  assert.match(initializer[1], new RegExp(`\\b${field}\\s*:`), `initial Snapshot missing ${field}`);
}

for (const field of ['tick', 'moveX', 'moveY', 'cameraYaw', 'cameraPitch',
  'targetDist', 'targetId', 'bossPhase']) {
  assert.match(page, new RegExp(`this\\.${field}\\s*=\\s*this\\.snapshot\\.${field}`),
    `GamePage polling must assign ${field}`);
  assert.match(nativeBridge, new RegExp(`"${field}"`));
}

assert.match(page, /changedTouches/,
  'GamePage must forward every changed pointer');
assert.match(page, /pointerId:\s*touch\.id/,
  'GamePage must preserve pointer ids');

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

assert.match(ability, /import \{ nativeStart, nativeStop \} from ['"]\.\/napi\/Bridge['"];/,
  'EntryAbility must import native lifecycle controls');
assert.match(functionBody(ability, 'onForeground(): void'), /nativeStart\(\);/,
  'EntryAbility foreground must resume the native loop');
assert.match(functionBody(ability, 'onBackground(): void'), /nativeStop\(\);/,
  'EntryAbility background must stop the native loop');

assert.match(functionBody(loop, 'void Loop::start()'), /if \(!surface\.ready\)[\s\S]*?return;/,
  'Loop start must safely skip before the surface is ready');
assert.match(nativeBridge, /std::atomic_bool g_foregroundRequested\{false\};/,
  'native bridge must track the requested foreground state');
assert.match(functionBody(nativeBridge, 'static napi_value NativeStart'),
  /g_foregroundRequested\.store\(true\)[\s\S]*?g_loop\.start\(\);/,
  'NativeStart must request foreground before starting');
assert.match(functionBody(nativeBridge, 'static napi_value NativeStop'),
  /g_foregroundRequested\.store\(false\)[\s\S]*?g_loop\.stop\(\);/,
  'NativeStop must clear foreground before stopping');
for (const callback of ['OnSurfaceCreated', 'OnSurfaceChanged']) {
  assert.match(functionBody(nativeBridge, `static void ${callback}`),
    /if \(g_foregroundRequested\.load\(\)\)[\s\S]*?g_loop\.start\(\);/,
    `${callback} must start only while foreground is requested`);
}

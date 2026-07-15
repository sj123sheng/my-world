import assert from 'node:assert/strict';
import fs from 'node:fs';

const bridge = fs.readFileSync('entry/src/main/ets/napi/Bridge.ets', 'utf8');
const declarations = fs.readFileSync('entry/src/main/cpp/types/libnative_game/Index.d.ts', 'utf8');
const page = fs.readFileSync('entry/src/main/ets/pages/GamePage.ets', 'utf8');
const ability = fs.readFileSync('entry/src/main/ets/EntryAbility.ets', 'utf8');
const nativeBridge = fs.readFileSync('entry/src/main/cpp/native_bridge.cpp', 'utf8');
const loop = fs.readFileSync('native/engine/core/loop.cpp', 'utf8');

assert.doesNotMatch(nativeBridge, /OH_NativeXComponent_GetTouchEvent/,
  'Native XComponent must not produce touch input alongside ArkTS changedTouches');
assert.doesNotMatch(nativeBridge, /OnDispatchTouchEvent/,
  'Native XComponent touch dispatch callback must not be registered');
assert.match(nativeBridge, /\.DispatchTouchEvent\s*=\s*nullptr/,
  'XComponent touch callback must be explicitly disabled');
assert.equal((nativeBridge.match(/g_loop\.enqueueInput\(/g) ?? []).length, 1,
  'NativePushInput must be the only native enqueue site for ArkTS touch input');

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

function blockBody(source, marker) {
  const start = source.indexOf(marker);
  assert.notEqual(start, -1, `missing block: ${marker}`);
  const open = source.indexOf('{', start);
  assert.notEqual(open, -1, `missing opening brace after: ${marker}`);
  let depth = 0;
  for (let index = open; index < source.length; index++) {
    if (source[index] === '{') depth++;
    if (source[index] === '}' && --depth === 0) return source.slice(open + 1, index);
  }
  assert.fail(`unterminated block: ${marker}`);
}

const snapshotInterface = bridge.match(/export interface Snapshot \{([\s\S]*?)\n\}/);
assert(snapshotInterface, 'Bridge must declare Snapshot');
const fields = [...snapshotInterface[1].matchAll(/^\s{2}(\w+): [^;]+;/gm)].map((match) => match[1]);
const declarationSnapshot = declarations.match(/export const pullSnapshot: \(\) => \{([\s\S]*?)\n\};/);
assert(declarationSnapshot, 'Index.d.ts must declare pullSnapshot result');
const declarationFields = [...declarationSnapshot[1].matchAll(/^\s{2}(\w+): [^,]+,?$/gm)]
  .map((match) => match[1]);
const nativeSnapshotBody = functionBody(nativeBridge, 'static napi_value NativePullSnapshot');
const nativeFields = [...nativeSnapshotBody.matchAll(/napi_set_named_property\(env, result, "(\w+)",/g)]
  .map((match) => match[1]);
assert.deepEqual(declarationFields, fields,
  'Index.d.ts pullSnapshot fields must match Bridge Snapshot fields in order');
assert.deepEqual(nativeFields, fields,
  'NativePullSnapshot exported fields must match Bridge Snapshot fields in order');
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

for (const field of ['moveX', 'moveY', 'cameraYaw', 'cameraPitch', 'targetDist']) {
  const create = nativeSnapshotBody.match(new RegExp(
    `napi_create_double\\(env, snapshot\\.${field}, &(\\w+)\\);`));
  assert(create, `NativePullSnapshot must create ${field} from snapshot.${field}`);
  assert.match(nativeSnapshotBody,
    new RegExp(`napi_set_named_property\\(env, result, "${field}", ${create[1]}\\);`),
    `NativePullSnapshot must export ${field} using its created value`);
}

const changedTouchCallback = blockBody(page, 'event.changedTouches.forEach');
const pushInputCall = blockBody(changedTouchCallback, 'pushInput(');
assert.match(pushInputCall, /pointerId:\s*touch\.id/,
  'changedTouches callback pushInput must preserve pointer ids');

assert.match(bridge, /interface InputEvent \{[\s\S]*?pointerId:\s*number;/,
  'Bridge InputEvent must require pointerId');
assert.match(declarations, /pushInput:[\s\S]*?pointerId:\s*number/,
  'Index.d.ts pushInput must require pointerId');

assert.match(nativeBridge, /argc != 1/, 'NativePushInput must reject wrong argument count');
assert.match(nativeBridge, /argumentType != napi_object/, 'NativePushInput must require an object');
for (const field of ['type', 'x', 'y']) {
  assert.match(nativeBridge, new RegExp(`GetNumberProperty\\(env, args\\[0\\], "${field}", true`),
    `NativePushInput must require numeric ${field}`);
}
assert.match(nativeBridge, /GetNumberProperty\(env, args\[0\], "pointerId", true/,
  'NativePushInput must require numeric pointerId');
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
assert.match(functionBody(nativeBridge, 'static napi_value NativePushInput'),
  /g_loop\.enqueueInput\(/,
  'the single native enqueue site must belong to the N-API producer');
for (const callback of ['OnSurfaceCreated', 'OnSurfaceChanged']) {
  assert.match(functionBody(nativeBridge, `static void ${callback}`),
    /if \(g_foregroundRequested\.load\(\)\)[\s\S]*?g_loop\.start\(\);/,
    `${callback} must start only while foreground is requested`);
}

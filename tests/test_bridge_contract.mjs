import assert from 'node:assert/strict';
import fs from 'node:fs';

const bridge = fs.readFileSync('entry/src/main/ets/napi/Bridge.ets', 'utf8');
const declarations = fs.readFileSync('entry/src/main/cpp/types/libnative_game/Index.d.ts', 'utf8');
const page = fs.readFileSync('entry/src/main/ets/pages/GamePage.ets', 'utf8');
const hud = fs.readFileSync('entry/src/main/ets/ui/Hud.ets', 'utf8');
const ability = fs.readFileSync('entry/src/main/ets/EntryAbility.ets', 'utf8');
const nativeBridge = fs.readFileSync('entry/src/main/cpp/native_bridge.cpp', 'utf8');
const loop = fs.readFileSync('native/engine/core/loop.cpp', 'utf8');
const controls = fs.existsSync('entry/src/main/ets/ui/CombatControls.ets')
  ? fs.readFileSync('entry/src/main/ets/ui/CombatControls.ets', 'utf8') : '';

const buttonActions = [['普攻', 0], ['闪避', 1], ['辉印', 2], ['脉流', 3], ['蚀质', 4], ['终结', 5]];
for (const [label, type] of buttonActions) {
  assert.match(controls,
    new RegExp(`Button\\(['"]${label}['"]\\)(?:(?!Button\\().)*pushAction\\(${type}\\)`, 's'),
    `CombatControls must pair ${label} with pushAction(${type})`);
}
assert.match(bridge, /export const pushAction/, 'Bridge must export pushAction');
assert.doesNotMatch(page, /\.onTouch\s*\(/,
  'GamePage must not register an ArkTS touch producer');
for (const field of ['stamina', 'comboSegment', 'invulnerable', 'insightMs',
  'resonance', 'targetHp', 'targetPoise', 'pulseHitRemainingMs', 'lastRejectReason']) {
  assert.match(bridge, new RegExp(`\\b${field}\\b`), `Bridge Snapshot missing ${field}`);
}
for (const source of [bridge, declarations, page, nativeBridge, loop, hud]) {
  assert.doesNotMatch(source, /\bpulseWarningMs\b/,
    'production snapshot chain must not retain the misleading pulseWarningMs name');
}
assert.match(hud,
  /pulseHitRemainingMs\s*>=\s*100\s*&&\s*this\.pulseHitRemainingMs\s*<=\s*500/,
  'HUD must highlight exactly the closed 100..500ms precision window');

assert.doesNotMatch(page, /\.onTouch\s*\(/,
  'GamePage must not register an ArkTS touch producer for a library-backed XComponent');
assert.doesNotMatch(page, /\bpushInput\b/,
  'GamePage must not produce input through the N-API pushInput bridge');
assert.match(nativeBridge, /\.DispatchTouchEvent\s*=\s*OnDispatchTouchEvent/,
  'XComponent touch callback must be registered as the only production input source');
assert.match(nativeBridge, /OH_NativeXComponent_GetTouchEvent/,
  'Native XComponent callback must fetch its touch event');

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

const pushActionBody = functionBody(nativeBridge, 'static napi_value NativePushAction');
assert.match(pushActionBody, /argc != 1/, 'NativePushAction must require exactly one argument');
assert.match(pushActionBody, /argumentType != napi_number/, 'NativePushAction must require a number');
assert.match(pushActionBody, /!std::isfinite\(typeNumber\)/,
  'NativePushAction must reject non-finite numbers');
assert.match(pushActionBody, /!TryConvertInt32\(typeNumber, type\)/,
  'NativePushAction must reject fractional numbers');
assert.match(pushActionBody, /type < 0 \|\| type > 5/,
  'NativePushAction must reject action types outside 0..5');
assert.match(pushActionBody,
  /kActions\[\]\s*=\s*\{\s*InputAction::Attack,\s*InputAction::Dodge,\s*InputAction::Radiance,\s*InputAction::Current,\s*InputAction::Corruption,\s*InputAction::Ultimate\s*\}/,
  'NativePushAction mapping order must be 0 Attack, 1 Dodge, 2 Radiance, 3 Current, 4 Corruption, 5 Ultimate');
assert.match(pushActionBody, /g_loop\.enqueueInput\(action, -1, 0\.0f, 0\.0f\)/,
  'NativePushAction must enqueue through Loop');

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

const dispatchTouchBody = functionBody(nativeBridge, 'static void OnDispatchTouchEvent');
assert.match(dispatchTouchBody,
  /OH_NativeXComponent_GetTouchEvent\(component, window, &touchEvent\)/,
  'Native callback must read the touch event for this component and window');
assert.doesNotMatch(dispatchTouchBody, /touchEvent\.touchPoints|\bpointCount\b/,
  'Native callback must not reinterpret the active-point snapshot as changed pointers');
assert.doesNotMatch(dispatchTouchBody, /touchEvent\.numPoints\s*==/,
  'Native callback must not branch input semantics on snapshot point count');
assert.match(dispatchTouchBody,
  /ForwardChangedPointer\([\s\S]*?touchEvent\.type[\s\S]*?touchEvent\.id[\s\S]*?touchEvent\.x[\s\S]*?touchEvent\.y/,
  'Native callback must forward only the top-level changed pointer fields');

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
  'pushInput remains a validated external/test bridge even though GamePage does not call it');
for (const callback of ['OnSurfaceCreated', 'OnSurfaceChanged']) {
  assert.match(functionBody(nativeBridge, `static void ${callback}`),
    /if \(g_foregroundRequested\.load\(\)\)[\s\S]*?g_loop\.start\(\);/,
    `${callback} must start only while foreground is requested`);
}

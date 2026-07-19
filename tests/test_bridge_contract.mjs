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
assert.match(bridge, /export const startEncounter/, 'Bridge must export startEncounter');
assert.match(declarations, /startEncounter: \(mode: number\) => boolean;/,
  'Index.d.ts must declare startEncounter(mode)');
assert.doesNotMatch(page, /\.onTouch\s*\(/,
  'GamePage must not register an ArkTS touch producer');
for (const field of ['stamina', 'comboSegment', 'invulnerable', 'insightMs',
  'resonance', 'targetHp', 'targetPoise', 'pulseHitRemainingMs', 'lastRejectReason',
  'encounterMode', 'encounterState']) {
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

const startEncounterBody = functionBody(nativeBridge, 'static napi_value NativeStartEncounter');
assert.match(startEncounterBody, /argc != 1/,
  'NativeStartEncounter must require exactly one argument');
assert.match(startEncounterBody, /argumentType != napi_number/,
  'NativeStartEncounter must require a number');
assert.match(startEncounterBody, /!std::isfinite\(modeNumber\)/,
  'NativeStartEncounter must reject non-finite numbers');
assert.match(startEncounterBody, /!TryConvertInt32\(modeNumber, mode\)/,
  'NativeStartEncounter must reject fractional numbers');
assert.match(startEncounterBody, /mode < 0 \|\| mode > 5/,
  'NativeStartEncounter must reject encounter modes outside 0..5');
assert.match(startEncounterBody, /g_loop\.startEncounter\(static_cast<EncounterMode>\(mode\)\)/,
  'NativeStartEncounter must delegate to Loop::startEncounter');
assert.match(startEncounterBody, /napi_get_boolean\(env, started, &result\)/,
  'NativeStartEncounter must return whether the encounter started');
assert.match(nativeBridge, /"startEncounter", nullptr, NativeStartEncounter/,
  'native bridge must export startEncounter');

assert.match(bridge, /export const advanceLevel/, 'Bridge must export advanceLevel');
assert.match(bridge, /export const useSupply/, 'Bridge must export useSupply');
assert.match(bridge, /export const retryBoss/, 'Bridge must export retryBoss');
assert.match(declarations, /advanceLevel: \(\) => boolean;/,
  'Index.d.ts must declare advanceLevel');
assert.match(declarations, /useSupply: \(\) => boolean;/,
  'Index.d.ts must declare useSupply');
assert.match(declarations, /retryBoss: \(\) => boolean;/,
  'Index.d.ts must declare retryBoss');
assert.match(nativeBridge, /"advanceLevel", nullptr, NativeAdvanceLevel/,
  'native bridge must export advanceLevel');
assert.match(nativeBridge, /"useSupply", nullptr, NativeUseSupply/,
  'native bridge must export useSupply');
assert.match(nativeBridge, /"retryBoss", nullptr, NativeRetryBoss/,
  'native bridge must export retryBoss');

const advanceLevelBody = functionBody(nativeBridge, 'static napi_value NativeAdvanceLevel');
assert.match(advanceLevelBody, /g_loop\.advanceLevel\(\)/,
  'NativeAdvanceLevel must delegate to Loop');
assert.match(advanceLevelBody, /napi_get_boolean\(env, advanced, &result\)/,
  'NativeAdvanceLevel must return boolean');

const useSupplyBody = functionBody(nativeBridge, 'static napi_value NativeUseSupply');
assert.match(useSupplyBody, /g_loop\.useSupply\(\)/,
  'NativeUseSupply must delegate to Loop');
assert.match(useSupplyBody, /napi_get_boolean\(env, supplied, &result\)/,
  'NativeUseSupply must return boolean');

const retryBossBody = functionBody(nativeBridge, 'static napi_value NativeRetryBoss');
assert.match(retryBossBody, /g_loop\.retryBoss\(\)/,
  'NativeRetryBoss must delegate to Loop');
assert.match(retryBossBody, /napi_get_boolean\(env, retried, &result\)/,
  'NativeRetryBoss must return boolean');

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
  'targetDist', 'targetId', 'bossPhase', 'encounterMode', 'encounterState']) {
  assert.match(page, new RegExp(`this\\.${field}\\s*=\\s*this\\.snapshot\\.${field}`),
    `GamePage polling must assign ${field}`);
  assert.match(nativeBridge, new RegExp(`"${field}"`));
}
for (const field of ['levelStage', 'gateState', 'supplyState', 'bossHp',
  'bossPoise', 'bossMechanic', 'bossCastMs']) {
  assert.match(page, new RegExp(`this\\.${field}\\s*=\\s*this\\.snapshot\\.${field}`),
    `GamePage polling must assign ${field}`);
  assert.match(nativeBridge, new RegExp(`"${field}"`));
}

assert.match(hud, /@Prop encounterMode: number = 0;/,
  'HUD must accept encounterMode');
assert.match(hud, /@Prop encounterState: number = 0;/,
  'HUD must accept encounterState');
assert.match(hud, /遭遇.*\$\{this\.encounterMode\}.*状态.*\$\{this\.encounterState\}/,
  'HUD must render encounter mode and state');

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
assert.match(controls, /import \{[^}]*\bpushAction\b[^}]*\bstartEncounter\b[^}]*\} from ['"]\.\.\/napi\/Bridge['"];/,
  'CombatControls must import startEncounter with pushAction');
assert.match(controls,
  /import \{[^}]*\bpushAction\b[^}]*\bstartEncounter\b[^}]*\badvanceLevel\b[^}]*\buseSupply\b[^}]*\bretryBoss\b[^}]*\} from ['"]\.\.\/napi\/Bridge['"];/,
  'CombatControls must import stage 5 methods');
for (const [label, mode] of [['训练', 0], ['兽群', 1], ['混战', 2], ['守卫', 3]]) {
  assert.match(controls,
    new RegExp(`Button\\(['"]${label}['"]\\)(?:(?!Button\\().)*startEncounter\\(${mode}\\)`, 's'),
    `CombatControls must pair ${label} with startEncounter(${mode})`);
}
for (const callback of ['OnSurfaceCreated', 'OnSurfaceChanged']) {
  assert.match(functionBody(nativeBridge, `static void ${callback}`),
    /if \(g_foregroundRequested\.load\(\)\)[\s\S]*?g_loop\.start\(\);/,
    `${callback} must start only while foreground is requested`);
}

for (const [label, mode] of [['流程', 4], ['首领', 5]]) {
  assert.match(controls,
    new RegExp(`Button\\(['"]${label}['"]\\)(?:(?!Button\\().)*startEncounter\\(${mode}\\)`, 's'),
    `CombatControls must pair ${label} with startEncounter(${mode})`);
}
assert.match(controls, new RegExp(`Button\\(['"]推进['"]\\)(?:(?!Button\\().)*advanceLevel\\(\\)`, 's'),
  'CombatControls must pair 推进 with advanceLevel()');
assert.match(controls, new RegExp(`Button\\(['"]补给['"]\\)(?:(?!Button\\().)*useSupply\\(\\)`, 's'),
  'CombatControls must pair 补给 with useSupply()');
assert.match(controls, new RegExp(`Button\\(['"]重试['"]\\)(?:(?!Button\\().)*retryBoss\\(\\)`, 's'),
  'CombatControls must pair 重试 with retryBoss()');

assert.match(hud, /@Prop levelStage: number = 0;/, 'HUD must accept levelStage');
assert.match(hud, /@Prop gateState: number = 0;/, 'HUD must accept gateState');
assert.match(hud, /@Prop supplyState: number = 0;/, 'HUD must accept supplyState');
assert.match(hud, /@Prop bossHp: number = 1000;/, 'HUD must accept bossHp');
assert.match(hud, /@Prop bossPoise: number = 300;/, 'HUD must accept bossPoise');
assert.match(hud, /@Prop bossMechanic: number = 0;/, 'HUD must accept bossMechanic');
assert.match(hud, /@Prop bossCastMs: number = 0;/, 'HUD must accept bossCastMs');
assert.match(hud,
  /关卡.*\$\{this\.levelStage\}.*门.*\$\{this\.gateState\}.*补给.*\$\{this\.supplyState\}/,
  'HUD must render level stage, gate and supply state');
assert.match(hud,
  /首领.*阶段.*\$\{this\.bossPhase\}.*机制.*\$\{this\.bossMechanic\}.*读条.*\$\{this\.bossCastMs\}/,
  'HUD must render boss HP, poise, phase, mechanic and cast');

// ---- Stage 6: toggleDebugHud, perfLevel, vfx fields ----
assert.match(bridge, /export const toggleDebugHud/, 'Bridge must export toggleDebugHud');
assert.match(declarations, /toggleDebugHud: \(\) => void;/, 'Index.d.ts must declare toggleDebugHud');
assert.match(nativeBridge, /"toggleDebugHud", nullptr, NativeToggleDebugHud/,
  'native bridge must export toggleDebugHud');

const toggleBody = functionBody(nativeBridge, 'static napi_value NativeToggleDebugHud');
assert.match(toggleBody, /g_loop\.toggleDebugHud\(\)/,
  'NativeToggleDebugHud must delegate to Loop');

for (const field of ['perfLevel', 'vfxFlags', 'cameraShakeX', 'cameraShakeY',
  'bossHpRatio', 'bossCastRatio', 'debugHud']) {
  assert.match(page, new RegExp(`this\\.${field}\\s*=\\s*this\\.snapshot\\.${field}`),
    `GamePage polling must assign ${field}`);
  assert.match(nativeBridge, new RegExp(`"${field}"`), `NativePullSnapshot must export ${field}`);
}

assert.match(controls, /import[^]*\btoggleDebugHud\b/,
  'CombatControls must import toggleDebugHud');
assert.match(controls, new RegExp(`Button\\(['"]调试['"]\\)(?:(?!Button\\().)*toggleDebugHud\\(\\)`, 's'),
  'CombatControls must pair 调试 with toggleDebugHud()');

// ---- Stage 6: Mobile HUD with Progress bars and debug toggle ----
assert.match(hud, /@Prop debugHud: boolean = false;/, 'HUD must accept debugHud');
assert.match(hud, /Progress\(\s*\{[^}]*value:\s*this\.hp/, 'HUD must render HP bar with Progress');
assert.match(hud, /Progress\(\s*\{[^}]*value:\s*this\.poise/, 'HUD must render poise bar with Progress');
assert.match(hud, /Progress\(\s*\{[^}]*value:\s*this\.stamina/, 'HUD must render stamina bar with Progress');
assert.match(hud, /if\s*\(\s*this\.debugHud\s*\)/, 'HUD must gate debug overlay on debugHud prop');
assert.match(hud, /Progress\(\s*\{[^}]*value:\s*this\.bossHpRatio/, 'HUD must render boss HP ratio bar with Progress');

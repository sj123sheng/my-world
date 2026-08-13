import assert from 'node:assert/strict';
import fs from 'node:fs';

function allMatches(source, pattern) {
  const flags = pattern.flags.includes('g') ? pattern.flags : `${pattern.flags}g`;
  return [...source.matchAll(new RegExp(pattern.source, flags))];
}

function assertSourceSequence(
  source, startPattern, endPattern, expressions, fieldReferencePattern) {
  const start = source.search(startPattern);
  assert.notEqual(start, -1, 'source sequence start marker must exist');
  const tail = source.slice(start);
  const end = tail.search(endPattern);
  assert.ok(end > 0, 'source sequence end marker must follow its start');
  const section = tail.slice(0, end);
  let previousEnd = 0;
  for (const { name, pattern } of expressions) {
    const matches = allMatches(section, pattern);
    assert.equal(matches.length, 1,
      `${name} expression must appear exactly once in its source section`);
    const match = matches[0];
    assert.ok(match.index >= previousEnd,
      `${name} expression must follow the preceding expression`);
    assert.doesNotMatch(section.slice(previousEnd, match.index),
      fieldReferencePattern,
      `${name} must directly follow the preceding field expression`);
    previousEnd = match.index + match[0].length;
  }
  assert.doesNotMatch(section.slice(previousEnd), fieldReferencePattern,
    'source section must not contain an unexpected trailing field expression');
}

const sequenceFixture = 'BEGIN out << state.a; out << state.b; END';
const sequenceExpressions = [
  { name: 'a', pattern: /out\s*<<\s*state\.a/ },
  { name: 'b', pattern: /out\s*<<\s*state\.b/ },
];
assert.doesNotThrow(() => assertSourceSequence(
  sequenceFixture, /BEGIN/, /END/, sequenceExpressions, /\bstate\.\w+\b/));
assert.throws(() => assertSourceSequence(
  'BEGIN out << state.a; out << state.a; out << state.b; END',
  /BEGIN/, /END/, sequenceExpressions, /\bstate\.\w+\b/));
assert.throws(() => assertSourceSequence(
  'BEGIN out << state.b; out << state.a; END',
  /BEGIN/, /END/, sequenceExpressions, /\bstate\.\w+\b/));
assert.throws(() => assertSourceSequence(
  'BEGIN out << state.a; END', /BEGIN/, /END/, sequenceExpressions,
  /\bstate\.\w+\b/));

const tokenParseExpressions = [
  { name: 'seed parse', pattern: /parse\(seedToken,\s*o\.seed\)/ },
  { name: 'chunk X parse', pattern: /parse\(chunkXToken,\s*o\.chunkX\)/ },
  { name: 'chunk Y parse', pattern: /parse\(chunkYToken,\s*o\.chunkY\)/ },
  { name: 'local X parse', pattern: /parse\(localXToken,\s*o\.localX\)/ },
  { name: 'local Y parse', pattern: /parse\(localYToken,\s*o\.localY\)/ },
];
const tokenReadExpressions = [
  { name: 'seed token read', pattern: /f\s*>>\s*seedToken/ },
  { name: 'chunk X token read', pattern: />>\s*chunkXToken/ },
  { name: 'chunk Y token read', pattern: />>\s*chunkYToken/ },
  { name: 'local X token read', pattern: />>\s*localXToken/ },
  { name: 'local Y token read', pattern: />>\s*localYToken/ },
];
function assertReaderWorldFixture(source) {
  assertSourceSequence(source, /BEGIN/, /parse\(/, tokenReadExpressions,
    /\b(?:seed|chunkX|chunkY|localX|localY)Token\b/);
  assertSourceSequence(source, /BEGIN/, /END/, tokenParseExpressions,
    /\bo\.\w+\b/);
}
const correctReaderFixture = `BEGIN
  f >> seedToken >> chunkXToken >> chunkYToken >> localXToken >> localYToken;
  parse(seedToken, o.seed); parse(chunkXToken, o.chunkX);
  parse(chunkYToken, o.chunkY); parse(localXToken, o.localX);
  parse(localYToken, o.localY);
END`;
assert.doesNotThrow(() => assertReaderWorldFixture(correctReaderFixture));
assert.throws(() => assertReaderWorldFixture(correctReaderFixture.replace(
  'seedToken >> chunkXToken', 'chunkXToken >> seedToken')));
assert.throws(() => assertReaderWorldFixture(correctReaderFixture.replace(
  'chunkYToken >> localXToken', 'chunkYToken >> chunkYToken >> localXToken')));
assert.throws(() => assertReaderWorldFixture(correctReaderFixture.replace(
  ' >> localXToken', '')));

const bridge = fs.readFileSync('entry/src/main/ets/napi/Bridge.ets', 'utf8');
const declarations = fs.readFileSync('entry/src/main/cpp/types/libnative_game/Index.d.ts', 'utf8');
const page = fs.readFileSync('entry/src/main/ets/pages/GamePage.ets', 'utf8');
const environmentManifest = fs.readFileSync(
  'entry/src/main/ets/generated/EnvironmentVisualManifest.ets', 'utf8');
const hud = fs.readFileSync('entry/src/main/ets/ui/Hud.ets', 'utf8');
const ability = fs.readFileSync('entry/src/main/ets/EntryAbility.ets', 'utf8');
const nativeBridge = fs.readFileSync('entry/src/main/cpp/native_bridge.cpp', 'utf8');
const loop = fs.readFileSync('native/engine/core/loop.cpp', 'utf8');
const controls = fs.existsSync('entry/src/main/ets/ui/CombatControls.ets')
  ? fs.readFileSync('entry/src/main/ets/ui/CombatControls.ets', 'utf8') : '';
const joystick = fs.existsSync('entry/src/main/ets/ui/Joystick.ets')
  ? fs.readFileSync('entry/src/main/ets/ui/Joystick.ets', 'utf8') : '';

assert.doesNotMatch(joystick, /\bpushInput\b/,
  'Visual Joystick must not duplicate the native XComponent input stream');
assert.match(joystick, /\.hitTestBehavior\(HitTestMode\.Transparent\)/,
  'Visual Joystick must let the XComponent remain the only production input source');
assert.match(page, /import \{ Joystick \} from ['"]\.\.\/ui\/Joystick['"];/,
  'GamePage must import the Joystick component');
assert.match(page, /Joystick\(\)/, 'GamePage must mount the Joystick layer');
assert.match(controls, /@Prop (@Watch\('[^']+'\) )?radianceCooldownMs: number = 0;/,
  'CombatControls must accept radiance cooldown prop');
assert.match(controls, /@Prop ultimateWindowMs: number = 0;/,
  'CombatControls must accept ultimate window prop');
assert.match(page, /CombatControls\(\{[\s\S]*?radianceCooldownMs: this\.radianceCooldownMs/,
  'GamePage must feed cooldown props into CombatControls');
assert.doesNotMatch(page,
  /CombatControls\(\{[\s\S]*?ultimateWindowMs:\s*this\.ultimateWindowMs\s*\}\)\s*\.hitTestBehavior\(HitTestMode\.Transparent\)/,
  'GamePage must not override button-level blocking with an outer transparent hit-test mode');

const buttonActions = [['普攻', 0], ['闪避', 1], ['辉印', 2], ['脉流', 3], ['蚀质', 4], ['终结', 5], ['跳跃', 6]];
for (const [label, type] of buttonActions) {
  assert.match(controls,
    new RegExp(`Button\\(['"]${label}['"]\\)(?:(?!Button\\().)*pushAction\\(${type}\\)`, 's'),
    `CombatControls must pair ${label} with pushAction(${type})`);
}
// 探索动作 7（交互）由 ExplorationHud 发出，8/9（滑翔按下/松开）由 CombatControls 发出。
const explorationHud = fs.existsSync('entry/src/main/ets/ui/ExplorationHud.ets')
  ? fs.readFileSync('entry/src/main/ets/ui/ExplorationHud.ets', 'utf8') : '';
assert.match(loop,
  /const auto enemyPositionResolver[\s\S]*buildingCollision\.resolve[\s\S]*explorationGateCollision\.resolve/,
  'enemy and boss resolver must apply static buildings before dynamic gates');
assert.match(explorationHud, /pushAction\(7\);/,
  'ExplorationHud must issue the interact action');
assert.match(controls, /pushAction\(8\);/,
  'CombatControls must issue GlidePress on touch down');
assert.match(controls, /pushAction\(9\);/,
  'CombatControls must issue GlideRelease on touch up');
const blockingButtons = [
  '普攻', '闪避', '辉印', '脉流', '蚀质', '终结', '跳跃', '☰',
  '训练', '兽群', '混战', '守卫', '流程', '首领', '推进', '补给', '重试', '调试'
];
for (const label of blockingButtons) {
  assert.match(controls,
    new RegExp(`Button\\(['"]${label}['"]\\)(?:(?!Button\\().)*` +
      `\\.hitTestBehavior\\(HitTestMode\\.Block\\)(?:(?!Button\\().)*\\.onClick`, 's'),
    `${label} must block its pointer before invoking the action`);
}
assert.match(controls,
  /\.hitTestBehavior\(HitTestMode\.None\)\s*\n\s*}\s*\n}/,
  'CombatControls root must skip itself while preserving child button hit testing');
assert.match(bridge, /export const pushAction/, 'Bridge must export pushAction');
assert.match(bridge, /export const startEncounter/, 'Bridge must export startEncounter');
assert.match(declarations, /startEncounter: \(mode: number\) => boolean;/,
  'Index.d.ts must declare startEncounter(mode)');
assert.doesNotMatch(page, /\.onTouch\s*\(/,
  'GamePage must not register an ArkTS touch producer');
for (const field of ['stamina', 'comboSegment', 'invulnerable', 'insightMs',
  'resonance', 'targetHp', 'targetPoise', 'pulseHitRemainingMs', 'lastRejectReason',
  'encounterMode', 'encounterState', 'objectiveLabel', 'resonanceSlots',
  'showDebugHud', 'bossCinematicProgress', 'bossShardCount',
  'bossSourceColor', 'bossRingBroken']) {
  assert.match(bridge, new RegExp(`\\b${field}\\b`), `Bridge Snapshot missing ${field}`);
}
for (const source of [bridge, declarations, page, nativeBridge, loop, hud]) {
  assert.doesNotMatch(source, /\bpulseWarningMs\b/,
    'production snapshot chain must not retain the misleading pulseWarningMs name');
}
assert.match(hud,
  /pulseHitRemainingMs\s*>=\s*100\s*&&\s*this\.pulseHitRemainingMs\s*<=\s*500/,
  'HUD must highlight exactly the closed 100..500ms precision window');
assert.match(hud, /@Prop objectiveLabel: string = '';/,
  'HUD must accept the current objective label');
assert.match(hud, /@Prop (@Watch\('[^']+'\) )?resonanceSlots: number\[\] = \[0, 0, 0\];/,
  'HUD must accept exactly three resonance slots');
assert.match(hud, /if \(this\.showDebugHud\)/,
  'HUD debug diagnostics must be opt-in');

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
assert.match(pushActionBody, /type < 0 \|\| type > 10/,
  'NativePushAction must reject action types outside 0..10');
assert.match(pushActionBody,
  /kActions\[\]\s*=\s*\{\s*InputAction::Attack,\s*InputAction::Dodge,\s*InputAction::Radiance,\s*InputAction::Current,\s*InputAction::Corruption,\s*InputAction::Ultimate,\s*InputAction::Jump,\s*InputAction::Interact,\s*InputAction::GlidePress,\s*InputAction::GlideRelease,\s*InputAction::SwitchCharacter\s*\}/,
  'NativePushAction mapping order must be 0..5 combat then 6 Jump, 7 Interact, 8 GlidePress, 9 GlideRelease, 10 SwitchCharacter');
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

// ---- Stage 7: skipDemoPhase for demo phase navigation ----
assert.match(bridge, /export const skipDemoPhase/, 'Bridge must export skipDemoPhase');
assert.match(declarations, /skipDemoPhase: \(phase: number\) => void;/,
  'Index.d.ts must declare skipDemoPhase');
assert.match(nativeBridge, /"skipDemoPhase", nullptr, NativeSkipDemoPhase/,
  'native bridge must export skipDemoPhase');

const skipBody = functionBody(nativeBridge, 'static napi_value NativeSkipDemoPhase');
assert.match(skipBody, /argc != 1/, 'NativeSkipDemoPhase must require exactly one argument');
assert.match(skipBody, /!std::isfinite\(phaseNumber\)/,
  'NativeSkipDemoPhase must reject non-finite numbers');
assert.match(skipBody, /!TryConvertInt32\(phaseNumber, phase\)/,
  'NativeSkipDemoPhase must reject fractional numbers');
assert.match(skipBody, /phase < 0 \|\| phase > 6/,
  'NativeSkipDemoPhase must reject phases outside 0..6');
assert.match(skipBody, /g_loop\.skipDemoPhase\(static_cast<DemoPhase>\(phase\)\)/,
  'NativeSkipDemoPhase must delegate to Loop');

// ---- Stage 6: Mobile HUD with Progress bars and debug toggle ----
assert.match(hud, /@Prop debugHud: boolean = false;/, 'HUD must accept debugHud');
assert.match(hud, /Progress\(\s*\{[^}]*value:\s*this\.hp/, 'HUD must render HP bar with Progress');
assert.match(hud, /Progress\(\s*\{[^}]*value:\s*this\.poise/, 'HUD must render poise bar with Progress');
assert.match(hud, /Progress\(\s*\{[^}]*value:\s*this\.stamina/, 'HUD must render stamina bar with Progress');
assert.match(hud, /if\s*\(\s*this\.debugHud\s*\)/, 'HUD must gate debug overlay on debugHud prop');
assert.match(hud, /Progress\(\s*\{[^}]*value:\s*this\.bossHpRatio/, 'HUD must render boss HP ratio bar with Progress');

// ---- M3-2 Task 2: GLB rawfile loading and ArrayBuffer bridge ----
assert.match(bridge, /export const nativeSetModelAssets/,
  'Bridge must export nativeSetModelAssets');
assert.match(declarations,
  /nativeSetModelAssets: \(player: ArrayBuffer, enemy: ArrayBuffer, boss: ArrayBuffer\) => boolean;/,
  'Index.d.ts must declare the three-model ArrayBuffer bridge');
assert.match(nativeBridge, /static bool CopyArrayBuffer/,
  'native bridge must copy ArrayBuffer bytes into owned storage');
assert.match(nativeBridge, /napi_is_arraybuffer/,
  'native bridge must validate ArrayBuffer arguments');
assert.match(nativeBridge, /std::vector<uint8_t>/,
  'native bridge must own copied model bytes');
assert.match(nativeBridge, /"nativeSetModelAssets", nullptr, NativeSetModelAssets/,
  'native bridge must export nativeSetModelAssets');

const setModelAssetsBody = functionBody(nativeBridge,
  'static napi_value NativeSetModelAssets');
assert.match(setModelAssetsBody, /argc != 3/,
  'NativeSetModelAssets must require exactly three arguments');
assert.match(setModelAssetsBody, /CopyAndCommitModelAssets/,
  'NativeSetModelAssets must delegate copy and commit ordering to the atomic helper');
assert.match(setModelAssetsBody, /CopyArrayBuffer\(env, args\[static_cast<size_t>\(slot\)\], out\)/,
  'each helper slot must independently copy its corresponding ArrayBuffer');
assert.match(setModelAssetsBody, /napi_get_boolean\(env, true, &result\)/,
  'NativeSetModelAssets must return true after atomic injection');

for (const model of ['player', 'enemy', 'boss']) {
  assert.match(page, new RegExp(`getRawFileContent\\(['"]models/${model}\\.glb['"]\\)`),
    `GamePage must read models/${model}.glb`);
}
assert.match(page, /Promise\.all\s*\(/,
  'GamePage must read the three model assets concurrently');
assert.match(page, /nativeSetModelAssets\s*\(/,
  'GamePage must inject all model assets in one bridge call');
const aboutToAppearBody = functionBody(page, 'aboutToAppear()');
assert.match(aboutToAppearBody, /this\.pageActive\s*=\s*true/,
  'GamePage aboutToAppear must make the page active before loading');
assert.match(aboutToAppearBody, /\+\+this\.modelLoadGeneration/,
  'GamePage aboutToAppear must start a new model-load generation');
assert.match(aboutToAppearBody, /this\.loadModelAssets\(generation\);/,
  'GamePage aboutToAppear must load models for its generation');
const disappearBody = functionBody(page, 'aboutToDisappear()');
assert.match(disappearBody, /this\.pageActive\s*=\s*false/,
  'GamePage disappearance must invalidate page activity');
assert.match(disappearBody, /\+\+this\.modelLoadGeneration/,
  'GamePage disappearance must invalidate pending model-load generations');
const loadModelAssetsBody = functionBody(page, 'private async loadModelAssets(generation: number)');
assert.match(loadModelAssetsBody, /catch\s*\([^)]+\)[\s\S]*?console\.error/,
  'GamePage must record rawfile loading failures');
assert.match(loadModelAssetsBody, /this\.isActiveModelLoad\(generation\)/,
  'GamePage must gate model completion by page activity and generation');
assert.match(loadModelAssetsBody,
  /finally\s*\{\s*if\s*\(this\.isActiveModelLoad\(generation\)\)\s*\{\s*nativeStartIfForeground\(\);/,
  'only the active generation may start native rendering, and only while foregrounded');
assert.match(bridge, /export const nativeStartIfForeground/,
  'Bridge must export a foreground-preserving native start');
assert.match(nativeBridge, /static napi_value NativeStartIfForeground/,
  'native bridge must implement foreground-preserving native start');
const nativeStartIfForegroundBody = functionBody(nativeBridge,
  'static napi_value NativeStartIfForeground');
assert.match(nativeStartIfForegroundBody, /g_foregroundRequested\.load\(\)/,
  'foreground-preserving start must not override a nativeStop background request');

assert.match(nativeBridge, /CopyAndCommitModelAssets/,
  'NativeSetModelAssets must use the independently testable atomic batch helper');
assert.match(nativeBridge,
  /CopyAndCommitModelAssets[\s\S]*?g_loop\.withLifecycle[\s\S]*?setModelAsset\(ModelKind::Player[\s\S]*?setModelAsset\(ModelKind::Enemy[\s\S]*?setModelAsset\(ModelKind::Boss/,
  'all copies must finish before one lifecycle-held, three-asset commit');

// ---- M4 Task 2: four-batch environment bridge ----
for (const batch of ['outer_ring', 'center_rift', 'backdrop', 'decoration']) {
  assert.match(page,
    new RegExp(`getRawFileContent\\(['"]environment/${batch}\\.glb['"]\\)`));
}
assert.match(bridge, /export const nativeSetEnvironmentAssets/);
assert.match(declarations,
  /nativeSetEnvironmentAssets: \(outer: ArrayBuffer, center: ArrayBuffer,[\s\S]*?backdrop: ArrayBuffer, decoration: ArrayBuffer\) => boolean;/);
assert.match(nativeBridge, /"nativeSetEnvironmentAssets", nullptr, NativeSetEnvironmentAssets/);
const setEnvironmentAssetsBody = functionBody(nativeBridge,
  'static napi_value NativeSetEnvironmentAssets');
assert.match(setEnvironmentAssetsBody, /argc != 4/,
  'NativeSetEnvironmentAssets must require exactly four arguments');
assert.match(setEnvironmentAssetsBody, /CopyAndCommitEnvironmentAssets/);
assert.match(setEnvironmentAssetsBody,
  /CopyArrayBuffer\(env, args\[static_cast<size_t>\(slot\)\], out\)/);
assert.match(setEnvironmentAssetsBody, /g_loop\.withLifecycle/);
assert.match(loadModelAssetsBody,
  /nativeSetModelAssets[\s\S]*?try[\s\S]*?nativeSetEnvironmentAssets[\s\S]*?catch/,
  'character and environment assets must use separate failure domains');

// ---- Environment vertical slice: natural terrain textures, no authored structures ----
assert.match(bridge, /export const nativeSetTerrainAssets/,
  'Bridge must export terrain material asset upload');
assert.match(bridge, /export const nativeSetVisualTerrainAsset/,
  'Bridge must export authored visual terrain upload');
assert.match(declarations,
  /nativeSetTerrainAssets: \(atlas: ArrayBuffer, control: ArrayBuffer\) => boolean;/,
  'native declarations must expose the terrain atlas/control pair');
assert.match(declarations,
  /nativeSetVisualTerrainAsset: \(blockId: number, lod: number, bytes: ArrayBuffer\) => boolean;/,
  'native declarations must expose block and LOD addressed visual terrain');
assert.match(nativeBridge, /"nativeSetTerrainAssets", nullptr, NativeSetTerrainAssets/);
assert.match(nativeBridge, /"nativeSetFoliageAtlas", nullptr, NativeSetFoliageAtlas/);
assert.match(nativeBridge, /"nativeSetVisualTerrainAsset", nullptr, NativeSetVisualTerrainAsset/);
assert.match(page, /TERRAIN_ATLAS_ASSET/,
  'GamePage must load the schema-generated terrain atlas path');
assert.match(page, /TERRAIN_CONTROL_ASSET/,
  'GamePage must load the schema-generated control map path');
assert.match(page, /FOLIAGE_ATLAS_ASSET/,
  'GamePage must load the schema-generated foliage atlas path');
assert.match(page, /VISUAL_TERRAIN_RESOURCES/,
  'GamePage must load schema-generated visual terrain block/LOD resources');
assert.match(environmentManifest,
  /VISUAL_TERRAIN_RESOURCES: VisualTerrainResource\[\] = \[\];/,
  'natural world manifest must publish no authored structure resources');
assert.doesNotMatch(environmentManifest, /\.glb/,
  'natural world manifest must not reference artificial GLBs');
const loadVisualTerrainBody = functionBody(page,
  'private async loadVisualTerrainSliceAssets(generation: number)');
assert.match(loadVisualTerrainBody, /this\.isActiveModelLoad\(generation\)/,
  'late visual terrain reads must not commit after the page generation is invalidated');

// ---- Stage 8: cooldown totals in snapshot, haptics and hit feedback ----
const gameSnapshot = fs.readFileSync('native/engine/core/game_snapshot.h', 'utf8');
const combatController = fs.readFileSync('native/gameplay/combat/combat_controller.cpp', 'utf8');
const combatControllerHeader = fs.readFileSync('native/gameplay/combat/combat_controller.h', 'utf8');
const moduleJson = fs.readFileSync('entry/src/main/module.json5', 'utf8');
const haptics = fs.existsSync('entry/src/main/ets/ui/Haptics.ets')
  ? fs.readFileSync('entry/src/main/ets/ui/Haptics.ets', 'utf8') : '';
const hitFeedback = fs.existsSync('entry/src/main/ets/ui/HitFeedback.ets')
  ? fs.readFileSync('entry/src/main/ets/ui/HitFeedback.ets', 'utf8') : '';

for (const total of ['radianceCooldownTotalMs', 'currentCooldownTotalMs', 'corruptionCooldownTotalMs']) {
  assert.match(combatControllerHeader, new RegExp(`Tick ${total} = 0;`),
    `CombatSnapshot must expose ${total}`);
  assert.match(gameSnapshot, new RegExp(`Tick ${total} = 0;`),
    `GameSnapshot must expose ${total}`);
  assert.match(combatController, new RegExp(`snapshot_\\.${total} = config_\\.sourceCooldownMs\\[`),
    `combat controller must fill ${total} from config`);
  assert.match(declarations, new RegExp(`${total}: number,`),
    `Index.d.ts must declare ${total}`);
  assert.match(bridge, new RegExp(`${total}: number;`),
    `Bridge Snapshot must declare ${total}`);
  assert.match(nativeBridge, new RegExp(`"${total}", extra\\[1[345]\\]`),
    `native bridge must marshal ${total}`);
  assert.match(page, new RegExp(`this\\.${total} = this\\.snapshot\\.${total};`),
    `GamePage must copy ${total} from the snapshot`);
}
assert.match(controls, /@Prop radianceCooldownTotalMs: number = 3000;/,
  'CombatControls must accept cooldown total props');
assert.match(controls, /this\.cooldownDraw\(this\.radianceCooldownMs, this\.radianceCooldownTotalMs/,
  'CombatControls must use snapshot cooldown totals, not hardcoded constants');
assert.doesNotMatch(controls, /RADIANCE_COOLDOWN_TOTAL_MS/,
  'CombatControls must not keep hardcoded cooldown constants');

assert.match(moduleJson, /"name": "ohos\.permission\.VIBRATE"/,
  'module.json5 must request the VIBRATE permission');
assert.match(haptics, /import \{ vibrator \} from ['"]@kit\.SensorServiceKit['"];/,
  'Haptics must use the SensorServiceKit vibrator API');
assert.match(haptics, /startVibration\(/, 'Haptics must trigger vibration');
assert.match(haptics, /catch/, 'Haptics must degrade silently on failure');
for (const method of ['light', 'sharp', 'hit', 'heavy', 'ultimateReady']) {
  assert.match(haptics, new RegExp(`static ${method}\\(\\): void`),
    `Haptics must expose ${method}()`);
}
assert.match(page, /import \{ Haptics \} from ['"]\.\.\/ui\/Haptics['"];/,
  'GamePage must import Haptics');
assert.match(page, /detectCombatEvents\(\);/,
  'GamePage must run snapshot edge detection each poll');
assert.match(page, /Haptics\.heavy\(\);[\s\S]*?Haptics\.hit\(\);/,
  'GamePage must distinguish heavy and normal hits');
assert.match(page, /snap\.insightMs > 0 && this\.prevInsightMs <= 0/,
  'GamePage must detect precision dodge via insight rising edge');
assert.match(page, /snap\.ultimateWindowMs > 0 && this\.prevUltimateWindowMs <= 0/,
  'GamePage must detect ultimate readiness via rising edge');

assert.match(hitFeedback, /@Watch\('onHpChanged'\) hp: number/,
  'HitFeedback must watch hp changes');
assert.match(hitFeedback, /radialGradient\(/,
  'HitFeedback must render a vignette via radial gradient');
assert.match(hitFeedback, /LOW_HP_THRESHOLD: number = 30;/,
  'HitFeedback must pulse at low hp');
assert.match(page, /import \{ HitFeedback \} from ['"]\.\.\/ui\/HitFeedback['"];/,
  'GamePage must import HitFeedback');
assert.match(page, /HitFeedback\(\{ hp: this\.hp \}\)/,
  'GamePage must mount HitFeedback with hp');

// ---- Stage 9: action reject toast ----
const actionToast = fs.existsSync('entry/src/main/ets/ui/ActionToast.ets')
  ? fs.readFileSync('entry/src/main/ets/ui/ActionToast.ets', 'utf8') : '';
assert.match(actionToast, /@Watch\('onRejectChanged'\) lastRejectReason: number/,
  'ActionToast must watch lastRejectReason');
for (const [code, label] of [[3, '技能冷却中'], [4, '体力不足'], [5, '共鸣不足']]) {
  assert.match(actionToast, new RegExp(`case ${code}: return ['"]${label}['"];`),
    `ActionToast must map reject reason ${code} to ${label}`);
}
assert.match(page, /import \{ ActionToast \} from ['"]\.\.\/ui\/ActionToast['"];/,
  'GamePage must import ActionToast');
assert.match(page, /ActionToast\(\{ lastRejectReason: this\.lastRejectReason,\s*questAcceptMessage: this\.questAcceptMessage \}\)/,
  'GamePage must mount ActionToast with lastRejectReason and questAcceptMessage');

// ---- Stage 10: lock-on target marker and combo counter ----
const surfaceHeader = fs.readFileSync('native/engine/render/surface.h', 'utf8');
const surfaceImpl = fs.readFileSync('native/engine/render/surface.cpp', 'utf8');
const comboCounter = fs.existsSync('entry/src/main/ets/ui/ComboCounter.ets')
  ? fs.readFileSync('entry/src/main/ets/ui/ComboCounter.ets', 'utf8') : '';

const shadowTargetBody = functionBody(surfaceImpl,
  'static bool ensureDirectionalShadowTarget(Surface& s, int32_t size)');
assert.match(shadowTargetBody, /GL_FRAMEBUFFER_BINDING/,
  'shadow target creation must preserve the caller framebuffer');
assert.match(shadowTargetBody,
  /glBindFramebuffer\(GL_FRAMEBUFFER, static_cast<GLuint>\(previousFramebuffer\)\)/,
  'shadow target creation must restore bloom/default framebuffer');
assert.doesNotMatch(surfaceHeader,
  /setVisualTerrainAsset[\s\S]{0,500}visualTerrainStatuses\[key\]/,
  'bridge thread must not mutate the renderer-owned visual status map');
assert.match(loop, /EffectiveEnvironmentQualityLevel\(/,
  'manual and automatic quality must produce one effective environment level');

assert.match(surfaceHeader, /struct TargetMarkerRenderState/,
  'Surface must declare TargetMarkerRenderState');
assert.match(surfaceHeader, /TargetMarkerRenderState targetMarker3d;/,
  'Surface must hold target marker state');
assert.match(surfaceHeader, /Mesh targetRingMesh;/,
  'Surface must hold the target ring mesh');
assert.match(surfaceImpl, /s\.targetRingMesh = createRing\(/,
  'target ring mesh must be created');
assert.match(surfaceImpl, /static void drawTargetMarker\(Surface& s, const glm::mat4& vp\)/,
  'surface must implement drawTargetMarker');
assert.match(surfaceImpl, /drawTargetMarker\(s, vp\);/,
  '3D phase must draw the target marker');
assert.match(loop, /surface\.targetMarker3d\.active = currentTarget\.has_value\(\);/,
  'loop must publish lock-on marker activity from soft targeting');
assert.match(loop, /surface\.targetMarker3d\.pulsePhase =/,
  'loop must publish marker pulse phase');
assert.match(loop, /surface\.targetMarker3d\.targetId =/,
  'loop must publish the locked target id for rim highlighting');
assert.match(loop, /surface\.boss3d\.targeted =/,
  'loop must publish boss lock-on state for rim highlighting');
assert.match(loop, /surface\.player3dAnimation\.moveRatio =/,
  'loop must publish joystick magnitude to scale run stride rate');
assert.match(loop, /surface\.enemyDeathSeconds\[enemy\.id\] \+= dtSeconds/,
  'loop must advance per-enemy death fade timers');
assert.match(loop, /surface\.boss3d\.entranceSeconds \+= dtSeconds/,
  'loop must advance boss entrance reveal timer');
assert.match(loop, /surface\.enemyHitCounts\[/,
  'loop must count enemy hits to rotate reaction variants');
assert.match(loop, /state\.animation\.variant =/,
  'loop must publish hit/death animation variant per enemy');
assert.match(surfaceHeader, /enemyDeathSeconds/,
  'Surface must keep per-enemy death fade timers');
assert.match(surfaceImpl, /DeathFadeAlpha\(enemy\.deathSeconds\)/,
  'dead enemies must fade out via DeathFadeAlpha when drawn');
assert.match(surfaceImpl, /BossEntranceReveal\(s\.boss3d\.entranceSeconds\)/,
  'boss rim must ramp in with the entrance reveal curve');
assert.match(surfaceImpl, /setSpecular\(profile\.specularStrength, profile\.specularShininess\)/,
  'actors must use per-profile specular material when drawn');
assert.match(surfaceHeader, /uint32_t targetId = 0;/,
  'TargetMarkerRenderState must carry the locked target id');
assert.match(surfaceHeader, /bool targeted = false;/,
  'Boss3DRenderState must carry lock-on state');
assert.match(surfaceImpl, /enemy\.id == s\.targetMarker3d\.targetId/,
  'locked enemy must be identified by marker target id when drawing actors');
const skinnedModelImpl = fs.readFileSync('native/engine/render/skinned_model.cpp', 'utf8');
assert.match(skinnedModelImpl, /actor\.variant,\s*\n\s*actor\.moveRatio/,
  'clip resolution must consider hit variant and move ratio for gait layering');

assert.match(comboCounter, /@Watch\('onComboChanged'\) comboSegment: number/,
  'ComboCounter must watch combo segment');
assert.match(comboCounter, /comboWindowMs > 0/,
  'ComboCounter must hide outside the combo window');
assert.match(page, /import \{ ComboCounter \} from ['"]\.\.\/ui\/ComboCounter['"];/,
  'GamePage must import ComboCounter');
assert.match(page, /ComboCounter\(\{ comboSegment: this\.comboSegment,[\s\S]*?comboWindowMs: this\.comboWindowMs \}\)/,
  'GamePage must mount ComboCounter with combo props');

// ---- Stage 11: onboarding tutorial ----
const tutorial = fs.existsSync('entry/src/main/ets/ui/Tutorial.ets')
  ? fs.readFileSync('entry/src/main/ets/ui/Tutorial.ets', 'utf8') : '';
assert.match(tutorial, /@StorageLink\('tutorialDone'\)/,
  'Tutorial must persist completion via StorageLink');
assert.match(tutorial, /@Watch\('onMovingChanged'\) moving: boolean/,
  'Tutorial must watch movement for the move step');
for (const text of ['滑动屏幕左侧移动角色', '点击右下角普攻攻击敌人',
  '点击闪避躲开敌人攻击', '释放辉印，开启三源共鸣']) {
  assert.match(tutorial, new RegExp(text), `Tutorial must teach: ${text}`);
}
assert.match(page, /PersistentStorage\.persistProp<boolean>\('tutorialDone', false\);/,
  'GamePage must initialize persistent tutorial state');
assert.match(page, /import \{ Tutorial \} from ['"]\.\.\/ui\/Tutorial['"];/,
  'GamePage must import Tutorial');
assert.match(page, /Tutorial\(\{ moving: this\.moving/,
  'GamePage must mount Tutorial with gameplay props');

// ---- Stage 12: pause system and encounter result overlays ----
const pauseMenu = fs.existsSync('entry/src/main/ets/ui/PauseMenu.ets')
  ? fs.readFileSync('entry/src/main/ets/ui/PauseMenu.ets', 'utf8') : '';
const stateOverlay = fs.existsSync('entry/src/main/ets/ui/GameStateOverlay.ets')
  ? fs.readFileSync('entry/src/main/ets/ui/GameStateOverlay.ets', 'utf8') : '';

assert.match(loop, /void Loop::setPaused\(bool value\)/,
  'Loop must implement setPaused');
assert.match(loop, /if \(paused\.load\(\)\) \{/,
  'tickOnce must freeze while paused');
assert.match(nativeBridge, /"setPaused", nullptr, NativeSetPaused/,
  'native bridge must export setPaused');
assert.match(declarations, /setPaused: \(paused: boolean\) => void;/,
  'Index.d.ts must declare setPaused');
assert.match(bridge, /export const setPaused/, 'Bridge must export setPaused');

assert.match(pauseMenu, /setPaused\(true\)/, 'PauseMenu must pause the loop');
assert.match(pauseMenu, /setPaused\(false\)/, 'PauseMenu must resume the loop');
assert.match(pauseMenu, /@StorageLink\('hapticsEnabled'\)/,
  'PauseMenu must expose the haptics toggle');
assert.match(haptics, /AppStorage\.get<boolean>\('hapticsEnabled'\) === false/,
  'Haptics must respect the hapticsEnabled setting');
assert.match(page, /PersistentStorage\.persistProp<boolean>\('hapticsEnabled', true\);/,
  'GamePage must persist the haptics setting');

assert.match(stateOverlay, /ENCOUNTER_STATE_DEFEAT: number = 3;/,
  'GameStateOverlay must handle the Defeat state');
assert.match(stateOverlay, /retryBoss\(\);/,
  'GameStateOverlay must retry boss defeats via retryBoss');
assert.match(stateOverlay, /advanceLevel\(\);/,
  'GameStateOverlay must advance after level-flow victory');
assert.match(page, /import \{ PauseMenu \} from ['"]\.\.\/ui\/PauseMenu['"];/,
  'GamePage must import PauseMenu');
assert.match(page, /import \{ GameStateOverlay \} from ['"]\.\.\/ui\/GameStateOverlay['"];/,
  'GamePage must import GameStateOverlay');
assert.match(page, /GameStateOverlay\(\{ encounterState: this\.encounterState/,
  'GamePage must mount GameStateOverlay');
assert.match(page, /PauseMenu\(\{ encounterMode: this\.encounterMode,\s*\n?\s*qualityPreset: this\.qualityPreset \}\)/,
  'GamePage must mount PauseMenu with the quality preset');

// ---- Stage 13: enemy overhead health bars ----
assert.match(surfaceHeader, /struct EnemyHpBarRenderState/,
  'Surface must declare EnemyHpBarRenderState');
assert.match(surfaceHeader, /std::vector<EnemyHpBarRenderState> enemyHpBars3d;/,
  'Surface must hold enemy hp bar render states');
assert.match(surfaceHeader, /Mesh hpBarQuadMesh;/,
  'Surface must hold the hp bar quad mesh');
assert.match(surfaceImpl, /static void drawEnemyHpBars\(Surface& s, const glm::mat4& vp\)/,
  'surface must implement drawEnemyHpBars');
assert.match(surfaceImpl, /drawEnemyHpBars\(s, vp\);/,
  '3D phase must draw enemy hp bars');
assert.match(surfaceImpl, /static glm::mat4 cameraBillboard\(const Surface& s\)/,
  'surface must share a camera billboard helper');
assert.match(loop, /surface\.enemyHpBars3d\.clear\(\);/,
  'loop must publish enemy hp bars');
assert.match(loop, /bar\.ratio = static_cast<float>\(hp\) \/ static_cast<float>\(maxHp\);/,
  'loop must compute hp ratio from hp and maxHp');

// ---- Stage 14: lock-on target focus frame ----
const targetFrame = fs.existsSync('entry/src/main/ets/ui/TargetFrame.ets')
  ? fs.readFileSync('entry/src/main/ets/ui/TargetFrame.ets', 'utf8') : '';
assert.match(gameSnapshot, /int32_t targetArchetype = -1;/,
  'GameSnapshot must expose targetArchetype');
assert.match(gameSnapshot, /float targetHpRatio = 0\.0f;/,
  'GameSnapshot must expose targetHpRatio');
assert.match(loop, /snapshot\.targetArchetype = static_cast<int32_t>\(enemy\.archetype\);/,
  'loop must resolve locked enemy archetype');
assert.match(nativeBridge, /"targetArchetype", targetArchetypeVal/,
  'native bridge must marshal targetArchetype');
assert.match(declarations, /targetArchetype: number,/,
  'Index.d.ts must declare targetArchetype');
assert.match(bridge, /targetHpRatio: number;/,
  'Bridge Snapshot must declare targetHpRatio');
for (const name of ['裂隙之爪', '辉光祭司', '蚀壳守卫']) {
  assert.match(targetFrame, new RegExp(name), `TargetFrame must name archetype ${name}`);
}
assert.match(targetFrame, /targetArchetype >= 0/,
  'TargetFrame must hide without a locked enemy');
assert.match(page, /TargetFrame\(\{ targetId: this\.targetId/,
  'GamePage must mount TargetFrame');

// ---- Stage 15: low stamina warning on HUD ----
assert.match(hud, /this\.stamina < 30 \? '#E06A5E' : '#4FD4BB'/,
  'HUD stamina bar must turn red below dodge cost');

// ---- Stage 16: open-world exploration fields across the snapshot chain ----
for (const field of ['explorationStamina', 'motionState', 'playerHeight',
  'activeChunkCount', 'chunkLoadCount', 'interactionAnchorId',
  'interactionUnlocked', 'interactionLabel', 'unlockedAnchorCount',
  'cameraExploration', 'teleportFlashMs', 'minimapAnchorX',
  'minimapAnchorY', 'minimapAnchorUnlocked']) {
  assert.match(gameSnapshot, new RegExp(`\\b${field}\\b`),
    `GameSnapshot must expose ${field}`);
  assert.match(nativeBridge, new RegExp(`"${field}"`),
    `native bridge must marshal ${field}`);
  assert.match(page, new RegExp(`this\\.${field}\\s*=\\s*this\\.snapshot\\.${field}`),
    `GamePage polling must assign ${field}`);
}
assert.match(page, /ExplorationHud\(\{/,
  'GamePage must mount ExplorationHud');
assert.match(gameSnapshot, /int32_t interactionAnchorId = -1;/,
  'GameSnapshot must default to no interaction target');
assert.match(loop, /worldGrid\.updateStreaming/,
  'loop must stream world chunks around the player');
assert.match(loop, /explorationMotion\.update/,
  'loop must advance the exploration motion state machine');
assert.match(loop, /anchors\.nearestInteraction/,
  'loop must resolve the nearest teleport anchor interaction');
assert.match(loop, /camera\.setExploration\(!currentTarget\.has_value\(\)\);/,
  'camera must switch exploration mode without a locked target');

// ---- Stage 16b: closed traversal gate blocking feedback ----
for (const field of ['explorationBlockedGateId', 'explorationBlockedGateLabel',
  'explorationBlockedByPuzzleLabel']) {
  assert.match(gameSnapshot, new RegExp(`\\b${field}\\b`),
    `GameSnapshot must expose ${field}`);
  assert.match(nativeBridge, new RegExp(`"${field}"`),
    `native bridge must marshal ${field}`);
  assert.match(bridge, new RegExp(`${field}: (?:number|string);`),
    `Bridge Snapshot must declare ${field}`);
  assert.match(declarations, new RegExp(`${field}: (?:number|string),`),
    `Index.d.ts must declare ${field}`);
  assert.match(page, new RegExp(`this\\.${field}\\s*=\\s*this\\.snapshot\\.${field}`),
    `GamePage polling must assign ${field}`);
}
assert.match(loop, /explorationContent\.nearestTarget\(\s*\{loop\.surface\.player\.x, loop\.surface\.player\.y\}/,
  'loop must resolve nearby exploration targets for gate feedback');
assert.match(explorationHud, /explorationBlockedGateLabel/,
  'ExplorationHud must render blocked gate feedback');
assert.match(explorationHud, /路径受阻/,
  'ExplorationHud must identify a blocked traversal gate');

// ---- Stage 16c: unified exploration feedback ----
const explorationToast = fs.existsSync('entry/src/main/ets/ui/ExplorationToast.ets')
  ? fs.readFileSync('entry/src/main/ets/ui/ExplorationToast.ets', 'utf8') : '';
for (const field of ['explorationFeedbackType', 'explorationFeedbackId',
  'explorationFeedbackTitle', 'explorationFeedbackSubtitle',
  'explorationFeedbackRemainingMs']) {
  assert.match(gameSnapshot, new RegExp(`\\b${field}\\b`),
    `GameSnapshot must expose ${field}`);
  assert.match(nativeBridge, new RegExp(`"${field}"`),
    `native bridge must marshal ${field}`);
  assert.match(bridge, new RegExp(`${field}: (?:number|string);`),
    `Bridge Snapshot must declare ${field}`);
  assert.match(declarations, new RegExp(`${field}: (?:number|string),`),
    `Index.d.ts must declare ${field}`);
  assert.match(page, new RegExp(`this\\.${field}\\s*=\\s*this\\.snapshot\\.${field}`),
    `GamePage polling must assign ${field}`);
}
assert.match(page, /prevExplorationFeedbackId/, 'GamePage must edge-detect feedback');
assert.match(page, /Haptics\.(?:light|heavy)\(\)/,
  'GamePage must vibrate for exploration feedback');
assert.match(page, /ExplorationToast\(\{/, 'GamePage must mount ExplorationToast');
assert.match(explorationToast, /地标|机关|路径|奖励/,
  'ExplorationToast must map the four exploration feedback types');

// ---- Stage 17: content systems (quests, dialog, interactables, save) ----
for (const field of ['questId', 'questStatus', 'questTitle',
  'questObjectiveLabel', 'questObjectiveProgress', 'questObjectiveRequired',
  'completedQuestCount', 'dialogActive', 'dialogSpeaker', 'dialogText',
  'dialogLineIndex', 'dialogLineCount', 'interactionKind']) {
  assert.match(gameSnapshot, new RegExp(`\\b${field}\\b`),
    `GameSnapshot must expose ${field}`);
  assert.match(nativeBridge, new RegExp(`"${field}"`),
    `native bridge must marshal ${field}`);
  assert.match(page, new RegExp(`this\\.${field}\\s*=\\s*this\\.snapshot\\.${field}`),
    `GamePage polling must assign ${field}`);
}
assert.match(bridge, /export const advanceDialog/,
  'Bridge must export advanceDialog');
assert.match(bridge, /export const saveProgress/,
  'Bridge must export saveProgress');
assert.match(bridge, /export const loadProgress/,
  'Bridge must export loadProgress');
assert.match(declarations, /advanceDialog: \(\) => void;/,
  'Index.d.ts must declare advanceDialog');
assert.match(declarations, /saveProgress: \(path: string\) => boolean;/,
  'Index.d.ts must declare saveProgress(path)');
assert.match(declarations, /loadProgress: \(path: string\) => boolean;/,
  'Index.d.ts must declare loadProgress(path)');
assert.match(nativeBridge, /"advanceDialog", nullptr, NativeAdvanceDialog/,
  'native bridge must export advanceDialog');
assert.match(nativeBridge, /"saveProgress", nullptr, NativeSaveProgress/,
  'native bridge must export saveProgress');
assert.match(nativeBridge, /"loadProgress", nullptr, NativeLoadProgress/,
  'native bridge must export loadProgress');
assert.match(loop, /quests\.notifyEnemiesKilled/,
  'loop must feed enemy deaths into the quest system');
assert.match(loop, /quests\.notifyAnchorReached/,
  'loop must feed anchor unlocks into the quest system');
assert.match(loop, /dialogSession\.start/,
  'loop must start dialog sessions from NPC interaction');
assert.match(loop, /void Loop::advanceDialog\(\)/,
  'Loop must implement advanceDialog');
assert.match(loop, /bool Loop::saveProgress\(const std::string& path\)/,
  'Loop must implement saveProgress');
assert.match(loop, /bool Loop::loadProgress\(const std::string& path\)/,
  'Loop must implement loadProgress');
const dialogBox = fs.existsSync('entry/src/main/ets/ui/DialogBox.ets')
  ? fs.readFileSync('entry/src/main/ets/ui/DialogBox.ets', 'utf8') : '';
assert.match(dialogBox, /advanceDialog\(\);/,
  'DialogBox must advance the dialog session');
assert.match(dialogBox, /@Prop dialogActive: boolean = false;/,
  'DialogBox must gate on dialogActive');
assert.match(page, /DialogBox\(\{ dialogActive: this\.dialogActive/,
  'GamePage must mount DialogBox');
assert.match(page, /loadProgress\(this\.progressSavePath\(\)\);/,
  'GamePage must restore progress on appear');
assert.match(page, /saveProgress\(this\.progressSavePath\(\)\);/,
  'GamePage must persist progress');
assert.match(explorationHud, /@Prop interactionKind: number = 0;/,
  'ExplorationHud must accept interactionKind');
assert.match(page, /interactionKind: this\.interactionKind,/,
  'GamePage must feed interactionKind into ExplorationHud');

// ---- Stage 18: growth and gacha chain ----
for (const field of ['fateCount', 'goldCount', 'expMaterialCount',
  'ascensionMaterialCount', 'gachaPity5', 'gachaResultIds',
  'gachaResultRarities', 'gachaResultIsNew', 'rosterIds', 'rosterLevels',
  'rosterAscensions']) {
  assert.match(gameSnapshot, new RegExp(`\\b${field}\\b`),
    `GameSnapshot must expose ${field}`);
  assert.match(nativeBridge, new RegExp(`"${field}"`),
    `native bridge must marshal ${field}`);
  assert.match(page, new RegExp(`this\\.${field}\\s*=\\s*this\\.snapshot\\.${field}`),
    `GamePage polling must assign ${field}`);
}
assert.match(bridge, /export const performGacha/,
  'Bridge must export performGacha');
assert.match(bridge, /export const useExpMaterial/,
  'Bridge must export useExpMaterial');
assert.match(bridge, /export const ascendCharacter/,
  'Bridge must export ascendCharacter');
assert.match(declarations, /performGacha: \(count: number\) => boolean;/,
  'Index.d.ts must declare performGacha(count)');
assert.match(nativeBridge, /"performGacha", nullptr, NativePerformGacha/,
  'native bridge must export performGacha');
assert.match(nativeBridge, /"useExpMaterial", nullptr, NativeUseExpMaterial/,
  'native bridge must export useExpMaterial');
assert.match(nativeBridge, /"ascendCharacter", nullptr, NativeAscendCharacter/,
  'native bridge must export ascendCharacter');
assert.match(loop, /bool Loop::performGacha\(int32_t count\)/,
  'Loop must implement performGacha');
assert.match(loop, /gacha\.draw\(gachaState, count\)/,
  'performGacha must draw through the deterministic gacha system');
assert.match(loop, /inventory\.removeItem\(fateId, count\)/,
  'performGacha must consume fate before drawing');
const gachaPanel = fs.existsSync('entry/src/main/ets/ui/GachaPanel.ets')
  ? fs.readFileSync('entry/src/main/ets/ui/GachaPanel.ets', 'utf8') : '';
assert.match(gachaPanel, /performGacha\(1\);/,
  'GachaPanel must offer a single pull');
assert.match(gachaPanel, /performGacha\(10\);/,
  'GachaPanel must offer a ten pull');
assert.match(gachaPanel, /useExpMaterial\(id, 5\);/,
  'GachaPanel must spend exp materials to level up');
assert.match(gachaPanel, /ascendCharacter\(id\);/,
  'GachaPanel must trigger ascension');
assert.match(page, /GachaPanel\(\{ open: this\.gachaOpen/,
  'GamePage must mount GachaPanel');
assert.match(explorationHud, /onOpenGacha: \(\) => void/,
  'ExplorationHud must accept the open-gacha callback');

// ---- Stage 19: polish — character switch, day/night, quality preset ----
const inputEvent = fs.readFileSync('native/engine/input/input_event.h', 'utf8');
assert.match(inputEvent, /SwitchCharacter/,
  'InputAction must include SwitchCharacter');
assert.match(loop, /Loop::switchCharacter\(/,
  'Loop must implement switchCharacter');
const loopHeader = fs.readFileSync('native/engine/core/loop.h', 'utf8');
assert.match(loopHeader, /void setQualityPreset\(int32_t preset\)/,
  'Loop must implement setQualityPreset');
assert.match(loop, /timeOfDaySeconds/,
  'Loop must track the day/night cycle clock');
for (const field of ['activeCharacterId', 'dayNightHour', 'qualityPreset']) {
  assert.match(gameSnapshot, new RegExp(`\\b${field}\\b`),
    `GameSnapshot must expose ${field}`);
  assert.match(nativeBridge, new RegExp(`"${field}"`),
    `native bridge must marshal ${field}`);
  assert.match(page, new RegExp(`this\\.${field}\\s*=\\s*this\\.snapshot\\.${field}`),
    `GamePage polling must assign ${field}`);
}
assert.match(bridge, /export const setQualityPreset/,
  'Bridge must export setQualityPreset');
assert.match(declarations, /setQualityPreset: \(preset: number\) => void;/,
  'Index.d.ts must declare setQualityPreset(preset)');
assert.match(nativeBridge, /"setQualityPreset", nullptr, NativeSetQualityPreset/,
  'native bridge must export setQualityPreset');
assert.match(explorationHud, /pushAction\(10\);/,
  'ExplorationHud character card must switch character via pushAction(10)');
assert.match(explorationHud, /dayNightHour/,
  'ExplorationHud must show the day/night indicator');
assert.match(pauseMenu, /setQualityPreset\(isOn \? 1 : 0\);/,
  'PauseMenu must expose a quality preset toggle');
assert.match(page, /qualityPreset: this\.qualityPreset/,
  'GamePage must feed qualityPreset into PauseMenu');
assert.match(page, /activeCharacterId: this\.activeCharacterId/,
  'GamePage must feed activeCharacterId into ExplorationHud');

// ---- Stage 20: weather and music region chain ----
const weatherSystem = fs.readFileSync('native/engine/world/weather_system.h', 'utf8');
const weatherSystemImpl = fs.readFileSync('native/engine/world/weather_system.cpp', 'utf8');
assert.match(weatherSystem, /static WeatherState weatherAt\(float gameSeconds\);/,
  'WeatherSystem must expose a deterministic weatherAt');
assert.match(weatherSystemImpl, /kSlots\[slot\]/,
  'WeatherSystem must derive weather from the fixed slot sequence');
const audioBridgeHeader = fs.readFileSync('native/platform/harmony/audio_bridge.h', 'utf8');
assert.match(audioBridgeHeader, /void setAmbientRegion\(int32_t region\);/,
  'AudioBridge must expose setAmbientRegion');
assert.match(loop, /audioBridge\.setAmbientRegion\(region\)/,
  'Loop must notify the audio bridge on music region change');
assert.match(loop, /WeatherSystem::environmentAt\(totalGameSeconds, hour\)/,
  'Loop must derive one complete daylight and weather environment state');
assert.match(loop, /surface\.lightColor\s*=\s*surface\.environmentState\.lightColor/,
  'Loop must drive render lighting from the unified environment state');
const cmake = fs.readFileSync('entry/src/main/cpp/CMakeLists.txt', 'utf8');
assert.match(cmake, /weather_system\.cpp/,
  'CMake must compile weather_system.cpp');
for (const field of ['weatherId', 'musicRegionId']) {
  assert.match(gameSnapshot, new RegExp(`\\b${field}\\b`),
    `GameSnapshot must expose ${field}`);
  assert.match(nativeBridge, new RegExp(`"${field}"`),
    `native bridge must marshal ${field}`);
  assert.match(declarations, new RegExp(`${field}: number`),
    `Index.d.ts must declare ${field}`);
  assert.match(page, new RegExp(`this\\.${field}\\s*=\\s*this\\.snapshot\\.${field}`),
    `GamePage polling must assign ${field}`);
}
assert.match(explorationHud, /@Prop weatherId: number = 0;/,
  'ExplorationHud must accept weatherId');
assert.match(explorationHud, /weatherLabel\(\)/,
  'ExplorationHud must render a weather label');
assert.match(page, /weatherId: this\.weatherId/,
  'GamePage must feed weatherId into ExplorationHud');

// ---- Stage 21: side quests, dungeon instance, story director ----
const sideQuests = fs.readFileSync('native/gameplay/quest/side_quests.cpp', 'utf8');
assert.match(sideQuests, /SideQuestSystem::defaults\(\)/,
  'SideQuestSystem must ship a default layout');
assert.match(sideQuests, /雾谷肃清/,
  'default side quests must include the kill quest');
assert.ok((sideQuests.match(/\{\d+, "/g) || []).length >= 3,
  'default layout must contain at least 3 side quests');
const dungeon = fs.readFileSync('native/gameplay/flow/dungeon.h', 'utf8');
assert.match(dungeon, /bool enter\(\)/,
  'DungeonSession must support entering the instance');
assert.match(dungeon, /bool leave\(\)/,
  'DungeonSession must support leaving the instance');
assert.match(dungeon, /rewardGold/,
  'DungeonSession must settle rewards');
const storyDirector = fs.readFileSync('native/gameplay/flow/story_director.cpp', 'utf8');
assert.match(storyDirector, /StoryDirector::opening\(\)/,
  'StoryDirector must provide the opening cinematic');
assert.match(loop, /storyDirector\.tick\(loopTimeMs\)/,
  'Loop must advance the story director each fixed step');
assert.match(loop, /storyDirector\.advance\(loopTimeMs\)/,
  'advanceDialog must manually advance opening subtitles so the continue button responds');
assert.match(loop, /loop\.storyDirector\.current\(\)/,
  'story cues must publish through the subtitle channel');
assert.match(loop, /dungeon\.enter\(\)/,
  'Loop must enter the dungeon from the entrance interactable');
assert.match(loop, /sideQuests\.notifyEvent\(SideQuestEvent::Kill, killsThisStep\)/,
  'kills must feed side quests');
assert.match(loop, /sideQuests\.completedMask\(\)/,
  'saveProgress must persist the side quest mask');
assert.match(loop, /sideQuests\.restoreMask\(state\.sideQuestMask\)/,
  'loadProgress must restore the side quest mask');
const saveImpl = fs.readFileSync('native/engine/resource/save.cpp', 'utf8');
assert.match(saveImpl, /tmp\s*<<\s*"V10 "/,
  'save writer must emit V10');
// 精确锁定 V8 尾字段、V9 五探索字段及其后唯一的 V10 五世界字段。
const writerTailExpressions = [
  { name: 'openWorldQuestMask', pattern: /s\.openWorldQuestMask/ },
  { name: 'openWorldQuestActiveId', pattern: /s\.openWorldQuestActiveId/ },
  { name: 'explorationPoiMask', pattern: /s\.explorationPoiMask/ },
  { name: 'explorationPuzzleMask', pattern: /s\.explorationPuzzleMask/ },
  { name: 'explorationRewardMask', pattern: /s\.explorationRewardMask/ },
  { name: 'explorationGateMask', pattern: /s\.explorationGateMask/ },
  { name: 'explorationTraversalMask', pattern: /s\.explorationTraversalMask/ },
  {
    name: 'worldSeed',
    pattern: /\(s\.worldSeed\s*==\s*0\s*\?\s*1\s*:\s*s\.worldSeed\)/,
  },
  { name: 'playerChunkX', pattern: /s\.playerChunkX/ },
  { name: 'playerChunkY', pattern: /s\.playerChunkY/ },
  { name: 'playerLocalX', pattern: /s\.playerLocalX/ },
  { name: 'playerLocalY', pattern: /s\.playerLocalY/ },
];
assertSourceSequence(saveImpl,
  /s\.openWorldQuestMask/, /tmp\s*<<\s*"\\n"/, writerTailExpressions,
  /\bs\.\w+\b/);
assert.match(saveImpl,
  /first\s*==\s*"V8"\s*\|\|\s*first\s*==\s*"V9"\s*\|\|\s*first\s*==\s*"V10"/,
  'save reader must share the complete V8/V9/V10 body');
const readerExplorationExpressions = [
  { name: 'explorationPoiMask', pattern: /f\s*>>\s*o\.explorationPoiMask/ },
  { name: 'explorationPuzzleMask', pattern: />>\s*o\.explorationPuzzleMask/ },
  { name: 'explorationRewardMask', pattern: />>\s*o\.explorationRewardMask/ },
  { name: 'explorationGateMask', pattern: />>\s*o\.explorationGateMask/ },
  { name: 'explorationTraversalMask', pattern: />>\s*o\.explorationTraversalMask/ },
];
assertSourceSequence(saveImpl,
  /if\s*\(first\s*==\s*"V9"\s*\|\|\s*first\s*==\s*"V10"\)/,
  /if\s*\(f\.fail\(\)\)\s*return\s+false/,
  readerExplorationExpressions, /\bo\.\w+\b/);
const readerWorldExpressions = [
  {
    name: 'worldSeed',
    pattern: /parseIntegerToken\(seedToken,\s*o\.worldSeed\)/,
  },
  {
    name: 'playerChunkX',
    pattern: /parseIntegerToken\(chunkXToken,\s*o\.playerChunkX\)/,
  },
  {
    name: 'playerChunkY',
    pattern: /parseIntegerToken\(chunkYToken,\s*o\.playerChunkY\)/,
  },
  {
    name: 'playerLocalX',
    pattern: /parseLocalToken\(localXToken,\s*o\.playerLocalX\)/,
  },
  {
    name: 'playerLocalY',
    pattern: /parseLocalToken\(localYToken,\s*o\.playerLocalY\)/,
  },
];
const readerWorldTokenExpressions = [
  { name: 'seedToken', pattern: /f\s*>>\s*seedToken/ },
  { name: 'chunkXToken', pattern: />>\s*chunkXToken/ },
  { name: 'chunkYToken', pattern: />>\s*chunkYToken/ },
  { name: 'localXToken', pattern: />>\s*localXToken/ },
  { name: 'localYToken', pattern: />>\s*localYToken/ },
];
assertSourceSequence(saveImpl, /f\s*>>\s*seedToken/,
  /if\s*\(f\.fail\(\)/, readerWorldTokenExpressions,
  /\b(?:seed|chunkX|chunkY|localX|localY)Token\b/);
assertSourceSequence(saveImpl, /if\s*\(first\s*==\s*"V10"\)/,
  /if\s*\(o\.worldSeed\s*==\s*0\)/, readerWorldExpressions,
  /\bo\.\w+\b/);
for (let version = 2; version <= 7; version += 1) {
  assert.match(saveImpl, new RegExp(`if\\s*\\(first\\s*==\\s*"V${version}"\\)`),
    `V${version} saves must remain readable`);
}
assert.match(saveImpl,
  /parseIntegerToken\(first,\s*o\.campLevel\)[\s\S]*?o\.relics[\s\S]*?o\.regionProgress/,
  'unversioned V1 saves must remain readable');
const interactableHeader = fs.readFileSync('native/gameplay/world/interactable.h', 'utf8');
assert.match(interactableHeader, /Dungeon = 3/,
  'InteractableKind must include Dungeon entrances');
for (const field of ['completedSideQuestCount', 'dungeonState',
  'dungeonProgress', 'dungeonRequired']) {
  assert.match(gameSnapshot, new RegExp(`\\b${field}\\b`),
    `GameSnapshot must expose ${field}`);
  assert.match(nativeBridge, new RegExp(`"${field}"`),
    `native bridge must marshal ${field}`);
  assert.match(declarations, new RegExp(`${field}: number`),
    `Index.d.ts must declare ${field}`);
  assert.match(page, new RegExp(`this\\.${field}\\s*=\\s*this\\.snapshot\\.${field}`),
    `GamePage polling must assign ${field}`);
}
assert.match(explorationHud, /case 5: return '进入';/,
  'ExplorationHud must offer the enter verb for dungeon entrances');
assert.match(explorationHud, /@Prop dungeonState: number = 0;/,
  'ExplorationHud must track dungeon state');
assert.match(cmake, /side_quests\.cpp/,
  'CMake must compile side_quests.cpp');
assert.match(page, /if \(!this\.dialogActive && !this\.gachaOpen && !this\.mapOpen &&\s*!this\.inventoryOpen && !this\.artifactOpen\) \{[\s\S]*?CombatControls\(\{/,
  'CombatControls must be hidden while a dialog, the gacha panel, the map, the inventory or the artifact panel is open so popup buttons stay clickable');
assert.match(cmake, /story_director\.cpp/,
  'CMake must compile story_director.cpp');

// ---- Stage 22: optimization batch (map teleport, minimap items, respawn, side tracker, stats) ----
assert.match(loop, /bool Loop::teleportToAnchor\(int32_t anchorId\)/,
  'Loop must implement teleportToAnchor for map fast travel');
assert.match(loop, /if \(!anchors\.isUnlocked\(anchorId\)\) return false;/,
  'teleportToAnchor must require the anchor to be unlocked');
assert.match(bridge, /export const teleportToAnchor/,
  'Bridge must export teleportToAnchor');
assert.match(declarations, /teleportToAnchor: \(anchorId: number\) => boolean;/,
  'Index.d.ts must declare teleportToAnchor(anchorId)');
assert.match(nativeBridge, /"teleportToAnchor", nullptr, NativeTeleportToAnchor/,
  'native bridge must export teleportToAnchor');
assert.match(loop, /interactables\.reviveConsumed\(InteractableKind::Collectible\)/,
  'collectibles must respawn after the countdown elapses');
assert.match(loop, /collectRespawnRemainingMs = kCollectRespawnMs;/,
  'collecting must arm the respawn countdown');
const interactableImpl = fs.readFileSync('native/gameplay/world/interactable.cpp', 'utf8');
assert.match(interactableImpl, /void InteractableRegistry::reviveConsumed/,
  'InteractableRegistry must support reviving consumed items');
for (const field of ['minimapItemX', 'minimapItemY', 'minimapItemKind',
  'sideQuestProgress', 'sideQuestRequired', 'rosterHp', 'rosterAtk']) {
  assert.match(gameSnapshot, new RegExp(`\\b${field}\\b`),
    `GameSnapshot must expose ${field}`);
  assert.match(nativeBridge, new RegExp(`"${field}"`),
    `native bridge must marshal ${field}`);
  assert.match(declarations, new RegExp(`${field}: number\\[\\]`),
    `Index.d.ts must declare ${field} array`);
  assert.match(page, new RegExp(`this\\.${field}\\s*=\\s*this\\.snapshot\\.${field}`),
    `GamePage polling must assign ${field}`);
}
const mapPanel = fs.readFileSync('entry/src/main/ets/ui/MapPanel.ets', 'utf8');
assert.match(mapPanel, /teleportToAnchor\(index \+ 1\)/,
  'MapPanel must fast-travel via teleportToAnchor');
assert.match(page, /MapPanel\(\{ open: this\.mapOpen/,
  'GamePage must mount MapPanel');
assert.match(explorationHud, /onOpenMap: \(\) => void/,
  'ExplorationHud must accept the open-map callback');
assert.match(explorationHud, /minimapItemKind\[index\] === 3 \? '#E5B84A'/,
  'ExplorationHud minimap must mark interactable items');
assert.match(explorationHud, /sideQuestName\(index\)/,
  'ExplorationHud must render the side quest tracker');
assert.match(gachaPanel, /this\.rosterHp\[index\]/,
  'GachaPanel must display derived HP');
assert.match(gachaPanel, /this\.rosterAtk\[index\]/,
  'GachaPanel must display derived ATK');
assert.match(loop, /ItemId::Fate\),\s*\n\s*pull\.rarity == 5 \? 4 : 2/,
  'duplicate gacha pulls must refund fate contracts');

// ---- Stage 23: weapons, constellations, quest reward toast ----
const weaponSystem = fs.readFileSync('native/gameplay/growth/weapon_system.cpp', 'utf8');
assert.match(weaponSystem, /WeaponSystem::catalog\(\)/,
  'WeaponSystem must ship a catalog');
assert.match(weaponSystem, /bool WeaponSystem::equip/,
  'WeaponSystem must support equipping');
assert.match(loop, /bool Loop::upgradeWeapon\(int32_t weaponId\)/,
  'Loop must implement upgradeWeapon');
assert.match(loop, /bool Loop::equipWeapon\(int32_t weaponId, int32_t characterId\)/,
  'Loop must implement equipWeapon');
assert.match(loop, /weapons\.addWeapon\(4\)/,
  'quest rewards must grant weapons');
assert.match(loop, /characters\.boostConstellation\(pull\.characterId\)/,
  'duplicate gacha pulls must boost constellations');
assert.match(loop, /loop\.weapons\.equippedBonusFor\(character\.characterId\)/,
  'derived ATK must include the equipped weapon bonus');
assert.match(bridge, /export const upgradeWeapon/,
  'Bridge must export upgradeWeapon');
assert.match(bridge, /export const equipWeapon/,
  'Bridge must export equipWeapon');
assert.match(declarations, /equipWeapon: \(weaponId: number, characterId: number\) => boolean;/,
  'Index.d.ts must declare equipWeapon(weaponId, characterId)');
assert.match(nativeBridge, /"upgradeWeapon", nullptr, NativeUpgradeWeapon/,
  'native bridge must export upgradeWeapon');
assert.match(nativeBridge, /"equipWeapon", nullptr, NativeEquipWeapon/,
  'native bridge must export equipWeapon');
assert.match(loop, /state\.weaponTriples\.push_back\(weapon\.weaponId\)/,
  'saveProgress must persist weapon triples');
assert.match(loop, /weapons\.restoreWeapon\(state\.weaponTriples\[i\]/,
  'loadProgress must restore weapons');
for (const field of ['rosterConstellations', 'weaponIds', 'weaponLevels',
  'weaponEquippedBy']) {
  assert.match(gameSnapshot, new RegExp(`\\b${field}\\b`),
    `GameSnapshot must expose ${field}`);
  assert.match(nativeBridge, new RegExp(`"${field}"`),
    `native bridge must marshal ${field}`);
  assert.match(declarations, new RegExp(`${field}: number\\[\\]`),
    `Index.d.ts must declare ${field} array`);
  assert.match(page, new RegExp(`this\\.${field}\\s*=\\s*this\\.snapshot\\.${field}`),
    `GamePage polling must assign ${field}`);
}
const rewardToast = fs.readFileSync('entry/src/main/ets/ui/QuestRewardToast.ets', 'utf8');
assert.match(rewardToast, /任务完成/,
  'QuestRewardToast must announce quest completion');
assert.match(page, /QuestRewardToast\(\{ visible: this\.rewardToastVisible/,
  'GamePage must mount QuestRewardToast');
assert.match(page, /this\.rewardLabelFor\(this\.completedQuestCount\)/,
  'GamePage must trigger the reward toast on quest completion');
assert.match(gachaPanel, /upgradeWeaponWithOre\(id, this\.bestOreId\(\), 1\);/,
  'GachaPanel must offer ore-based weapon upgrade');
assert.match(gachaPanel, /equipWeapon\(id, this\.activeCharacterId\);/,
  'GachaPanel must offer weapon equip');
assert.match(gachaPanel, /rosterConstellations\[index\]/,
  'GachaPanel must display constellations');
assert.match(cmake, /weapon_system\.cpp/,
  'CMake must compile weapon_system.cpp');

// ---- Stage 24: sprint, daily quests, weapon gacha pool, weather tint ----
const dailyQuest = fs.readFileSync('native/gameplay/quest/daily_quest.cpp', 'utf8');
assert.match(dailyQuest, /DailyQuestSystem::questsForDay/,
  'DailyQuestSystem must derive quests deterministically per day');
assert.match(loop, /dailyQuests\.notifyEvent\(DailyQuestKind::Kill, killsThisStep\)/,
  'kills must feed daily quests');
assert.match(loop, /dailyQuests = DailyQuestSystem\(gameDayCount\)/,
  'daily quests must reset each in-game day');
assert.match(loop, /dailyQuests\.allCompleted\(\)/,
  'daily reward must require all quests complete');
const motionHeader = fs.readFileSync('native/gameplay/player/exploration_motion.h', 'utf8');
assert.match(motionHeader, /bool sprinting = false;/,
  'ExplorationMotionState must track sprinting');
assert.match(motionHeader, /float sprintSpeedMultiplier/,
  'sprint speed multiplier must be configurable');
const controllerHeader = fs.readFileSync('native/gameplay/player/player_controller.h', 'utf8');
assert.match(controllerHeader, /float speedScale = 1\.0f/,
  'PlayerController must accept a speedScale for sprinting');
assert.match(loop, /explorationMotion\.config\(\)\.sprintSpeedMultiplier/,
  'Loop must apply sprint speed multiplier to movement');
const gachaHeader = fs.readFileSync('native/gameplay/growth/gacha_system.h', 'utf8');
assert.match(gachaHeader, /drawWeapon\(GachaState& state, int32_t count\)/,
  'GachaSystem must expose a weapon pool');
assert.match(loop, /bool Loop::performWeaponGacha\(int32_t count\)/,
  'Loop must implement performWeaponGacha');
assert.match(loop, /gacha\.drawWeapon\(gachaState, count\)/,
  'performWeaponGacha must draw through the weapon pool');
assert.match(bridge, /export const performWeaponGacha/,
  'Bridge must export performWeaponGacha');
assert.match(declarations, /performWeaponGacha: \(count: number\) => boolean;/,
  'Index.d.ts must declare performWeaponGacha(count)');
assert.match(nativeBridge, /"performWeaponGacha", nullptr, NativePerformWeaponGacha/,
  'native bridge must export performWeaponGacha');
for (const field of ['sprintActive', 'dailyCompletedCount', 'dailyQuestClaimed',
  'gachaPoolKind']) {
  assert.match(gameSnapshot, new RegExp(`\\b${field}\\b`),
    `GameSnapshot must expose ${field}`);
  assert.match(nativeBridge, new RegExp(`"${field}"`),
    `native bridge must marshal ${field}`);
  assert.match(declarations, new RegExp(`${field}: number`),
    `Index.d.ts must declare ${field}`);
  assert.match(page, new RegExp(`this\\.${field}\\s*=\\s*this\\.snapshot\\.${field}`),
    `GamePage polling must assign ${field}`);
}
assert.match(explorationHud, /if \(this\.sprintActive === 1\) return '疾跑';/,
  'ExplorationHud must label sprinting');
assert.match(explorationHud, /每日委托/,
  'ExplorationHud must show daily quest progress');
assert.match(explorationHud, /weatherTint\(\)/,
  'ExplorationHud must render a weather tint overlay');
assert.match(gachaPanel, /performWeaponGacha\(1\);/,
  'GachaPanel must offer a weapon single pull');
assert.match(gachaPanel, /performWeaponGacha\(10\);/,
  'GachaPanel must offer a weapon ten pull');
assert.match(gachaPanel, /resultName\(id\)/,
  'GachaPanel must label results by pool');
assert.match(cmake, /daily_quest\.cpp/,
  'CMake must compile daily_quest.cpp');

// ---- Stage 25: 原神式养成体系（冒险等级/打怪掉落/武器深化/圣遗物/背包）----
const adventureRankSource = fs.readFileSync('native/gameplay/growth/adventure_rank.cpp', 'utf8');
const artifactSystemSource = fs.readFileSync('native/gameplay/growth/artifact_system.cpp', 'utf8');
assert.match(adventureRankSource, /AdventureRank::worldLevelFor/,
  'AdventureRank must map adventure rank to world level');
assert.match(adventureRankSource, /AdventureRank::dropMultiplierPct/,
  'AdventureRank must expose world level drop multiplier');
assert.match(adventureRankSource, /AdventureRank::rankRewards/,
  'AdventureRank must ship rank reward table');
assert.match(artifactSystemSource, /ArtifactSystem::catalog/,
  'ArtifactSystem must ship a catalog');
assert.match(artifactSystemSource, /bool ArtifactSystem::equip/,
  'ArtifactSystem must support equipping');
assert.match(weaponSystem, /bool WeaponSystem::ascend/,
  'WeaponSystem must support ascension');
assert.match(weaponSystem, /bool WeaponSystem::refine/,
  'WeaponSystem must support refinement');
assert.match(weaponSystem, /int32_t WeaponSystem::addWeaponExp/,
  'WeaponSystem must support ore-based exp enhancement');
assert.match(loop, /bool Loop::upgradeWeaponWithOre\(int32_t weaponId, int32_t oreItemId/,
  'Loop must implement upgradeWeaponWithOre');
assert.match(loop, /bool Loop::ascendWeapon\(int32_t weaponId\)/,
  'Loop must implement ascendWeapon');
assert.match(loop, /bool Loop::refineWeapon\(int32_t weaponId\)/,
  'Loop must implement refineWeapon');
assert.match(loop, /bool Loop::useExpItem\(int32_t characterId, int32_t itemId, int32_t count\)/,
  'Loop must implement useExpItem');
assert.match(loop, /bool Loop::equipArtifact\(int32_t instanceId, int32_t characterId\)/,
  'Loop must implement equipArtifact');
assert.match(loop, /bool Loop::claimRankReward\(int32_t rank\)/,
  'Loop must implement claimRankReward');
assert.match(loop, /AdventureRank::dropMultiplierPct\(adventureRank\.worldLevel\(\)\)/,
  'kill drops must scale with world level');
assert.match(loop, /artifacts\.addArtifact\(ArtifactSystem::dropDefId\(dropSeed\), 5/,
  'boss kills must drop a five-star artifact');
assert.match(loop, /weapons\.addRefineStock\(pull\.characterId\)/,
  'duplicate weapon pulls must accumulate refinement stock');
assert.match(loop, /adventureRank\.addExp\(120\)/,
  'dungeon clears must grant adventure exp');
assert.match(loop, /state\.weaponRecords\.push_back\(weapon\.weaponId\)/,
  'saveProgress must persist weapon seven-tuples');
assert.match(loop, /weapons\.restoreWeapon\(state\.weaponRecords\[i\]/,
  'loadProgress must restore weapon seven-tuples');
assert.match(loop, /artifacts\.restoreArtifact/,
  'loadProgress must restore artifacts');
for (const fn of ['upgradeWeaponWithOre', 'ascendWeapon', 'refineWeapon',
  'useExpItem', 'upgradeArtifact', 'equipArtifact', 'claimRankReward']) {
  const pascal = fn.charAt(0).toUpperCase() + fn.slice(1);
  assert.match(bridge, new RegExp(`export const ${fn}`),
    `Bridge must export ${fn}`);
  assert.match(nativeBridge, new RegExp(`"${fn}", nullptr, Native${pascal}`),
    `native bridge must export ${fn}`);
}
assert.match(declarations, /upgradeArtifact: \(targetInstanceId: number, feedInstanceIds: number\[\]\) => boolean;/,
  'Index.d.ts must declare upgradeArtifact(targetInstanceId, feedInstanceIds)');
for (const field of ['weaponAscensions', 'weaponRefines', 'weaponRefineStocks',
  'weaponExps', 'artifactInstanceIds', 'artifactDefIds', 'artifactRarities',
  'artifactLevels', 'artifactEquippedBy']) {
  assert.match(gameSnapshot, new RegExp(`\\b${field}\\b`),
    `GameSnapshot must expose ${field}`);
  assert.match(nativeBridge, new RegExp(`"${field}"`),
    `native bridge must marshal ${field}`);
  assert.match(declarations, new RegExp(`${field}: number\\[\\]`),
    `Index.d.ts must declare ${field} array`);
  assert.match(page, new RegExp(`this\\.${field}\\s*=\\s*this\\.snapshot\\.${field}`),
    `GamePage polling must assign ${field}`);
}
for (const field of ['adventureRank', 'worldLevel', 'oreLowCount',
  'oreHighCount', 'expSmallCount']) {
  assert.match(gameSnapshot, new RegExp(`\\b${field}\\b`),
    `GameSnapshot must expose ${field}`);
  assert.match(nativeBridge, new RegExp(`"${field}"`),
    `native bridge must marshal ${field}`);
  assert.match(page, new RegExp(`this\\.${field}\\s*=\\s*this\\.snapshot\\.${field}`),
    `GamePage polling must assign ${field}`);
}
const inventoryPanel = fs.readFileSync('entry/src/main/ets/ui/InventoryPanel.ets', 'utf8');
assert.match(inventoryPanel, /货币/,
  'InventoryPanel must offer a currency tab');
assert.match(inventoryPanel, /圣遗物/,
  'InventoryPanel must offer an artifact tab');
const artifactPanel = fs.readFileSync('entry/src/main/ets/ui/ArtifactPanel.ets', 'utf8');
assert.match(artifactPanel, /equipArtifact/,
  'ArtifactPanel must offer equip actions');
assert.match(artifactPanel, /upgradeArtifact/,
  'ArtifactPanel must offer feed enhancement');
assert.match(gachaPanel, /ascendWeapon/,
  'GachaPanel must offer weapon ascension');
assert.match(gachaPanel, /refineWeapon/,
  'GachaPanel must offer weapon refinement');
assert.match(cmake, /adventure_rank\.cpp/,
  'CMake must compile adventure_rank.cpp');
assert.match(cmake, /artifact_system\.cpp/,
  'CMake must compile artifact_system.cpp');

// ---- Phase 4：任务与互动 NPC（快照尾部 2 字段 + NPC 模型槽位）----
const phase4LoopHeader = fs.readFileSync('native/engine/core/loop.h', 'utf8');
assert.match(phase4LoopHeader, /QuestSystem openWorldQuests = QuestSystem::openWorldQuests\(\);/,
  'Loop must hold the open-world side quest system');
assert.match(phase4LoopHeader, /NpcAgency npcAgency = NpcAgency::fromWorldLayout\(\);/,
  'Loop must hold the NPC agency');
assert.match(loop, /advanceDialogSession\(\*this\);/,
  'Loop must advance dialog through the quest-offer bridge');
assert.match(loop, /openWorldQuests\.notifyEnemiesKilled\(killsThisStep\);/,
  'wild kills must feed open-world side quests');
// Phase 5：发布调用追加性能联动上限参数（原四参调用语义不变，默认值仍为 6）。
assert.match(loop, /publishNpcs3d\(surface, npcAgency, surface\.player\.x, surface\.player\.y,\s*\n?\s*npcVisibleLimitForPerf\(performanceGuard\.lodLevel\(\)\)\);/,
  'Loop must publish NPC 3D render states with perf-scaled visible limit');
for (const field of ['npcOfferQuestId', 'npcOfferQuestTitle']) {
  assert.match(gameSnapshot, new RegExp(`\\b${field}\\b`),
    `GameSnapshot must expose ${field}`);
  assert.match(nativeBridge, new RegExp(`"${field}"`),
    `native bridge must marshal ${field}`);
  assert.match(declarations, new RegExp(`${field}:`),
    `Index.d.ts must declare ${field}`);
  assert.match(bridge, new RegExp(`${field}:`),
    `Bridge Snapshot interface must declare ${field}`);
}
assert.match(page, /this\.prevNpcOfferQuestId/,
  'GamePage polling must edge-detect npcOfferQuestId');
assert.match(page, /已接取任务：/,
  'GamePage must show the quest accept toast text');
for (const field of ['explorationPoiCount', 'explorationPuzzleCount',
  'explorationRewardCount', 'explorationGateCount',
  'explorationTraversalMask', 'explorationCurrentPoiId',
  'explorationCurrentTargetLabel', 'explorationCurrentTargetDistrict']) {
  assert.match(gameSnapshot, new RegExp(`\\b${field}\\b`),
    `GameSnapshot must expose ${field}`);
  assert.match(nativeBridge, new RegExp(`"${field}"`),
    `native bridge must marshal ${field}`);
  assert.match(declarations, new RegExp(`${field}:`),
    `Index.d.ts must declare ${field}`);
  assert.match(bridge, new RegExp(`${field}:`),
    `Bridge Snapshot interface must declare ${field}`);
  assert.match(page, new RegExp(`this\\.${field}\\s*=\\s*this\\.snapshot\\.${field}`),
    `GamePage polling must assign ${field}`);
}
assert.match(explorationHud, /explorationCurrentTargetLabel/,
  'ExplorationHud must show the current exploration target');
assert.match(bridge, /export const nativeSetNpcAsset/,
  'Bridge must export nativeSetNpcAsset');
assert.match(declarations, /nativeSetNpcAsset: \(bytes: ArrayBuffer\) => boolean;/,
  'Index.d.ts must declare nativeSetNpcAsset(bytes)');
assert.match(nativeBridge, /"nativeSetNpcAsset", nullptr, NativeSetNpcAsset/,
  'native bridge must export nativeSetNpcAsset');
assert.match(page, /nativeSetNpcAsset/,
  'GamePage must inject the NPC model asset');
// ---- 独立高模资产：敌人原型独立模型槽位（缺失回退共享 enemy.glb）----
assert.match(bridge, /export const nativeSetEnemyArchetypeAsset/,
  'Bridge must export nativeSetEnemyArchetypeAsset');
assert.match(declarations,
  /nativeSetEnemyArchetypeAsset: \(archetype: number, bytes: ArrayBuffer\) => boolean;/,
  'Index.d.ts must declare nativeSetEnemyArchetypeAsset(archetype, bytes)');
assert.match(nativeBridge,
  /"nativeSetEnemyArchetypeAsset", nullptr, NativeSetEnemyArchetypeAsset/,
  'native bridge must export nativeSetEnemyArchetypeAsset');
const setEnemyArchetypeAssetBody = functionBody(nativeBridge,
  'static napi_value NativeSetEnemyArchetypeAsset');
assert.match(setEnemyArchetypeAssetBody, /argc != 2/,
  'NativeSetEnemyArchetypeAsset must require exactly two arguments');
assert.match(setEnemyArchetypeAssetBody, /kEnemyArchetypeCount/,
  'NativeSetEnemyArchetypeAsset must bound archetype against kEnemyArchetypeCount');
assert.match(setEnemyArchetypeAssetBody, /CopyArrayBuffer/,
  'NativeSetEnemyArchetypeAsset must copy ArrayBuffer bytes into owned storage');
assert.match(setEnemyArchetypeAssetBody, /setEnemyArchetypeAsset/,
  'NativeSetEnemyArchetypeAsset must commit bytes to the Surface slot');
assert.match(page, /nativeSetEnemyArchetypeAsset/,
  'GamePage must inject per-archetype enemy model assets');
assert.match(page, /enemy_\$\{archetype\}\.glb/,
  'GamePage must read models/enemy_<archetype>.glb rawfiles');
assert.match(cmake, /npc_agent\.cpp/,
  'CMake must compile npc_agent.cpp');

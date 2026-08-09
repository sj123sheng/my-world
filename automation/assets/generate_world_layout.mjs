// 世界布局代码生成：读取 assets/world/world.json（单一事实来源），
// 按 config/schema/world.schema.json 校验（手写轻量 draft-07 子集校验器，
// 零 npm 依赖），再执行语义校验（id 唯一、分块覆盖不重叠、坐标界内、
// archetype 合法、实体落在所属 district 分块范围内），最终生成
// native/generated/world_layout.gen.h（constexpr 数据，零运行时 JSON 依赖）。
// 脚本幂等：同输入重复运行产物逐字节一致。
import { mkdir, readFile, writeFile } from 'node:fs/promises';
import { dirname, join, resolve } from 'node:path';
import { fileURLToPath, pathToFileURL } from 'node:url';

// ---- 轻量 JSON Schema 校验器（draft-07 子集）----

function schemaTypeMatches(schemaType, value) {
  switch (schemaType) {
    case 'object': return value !== null && typeof value === 'object' && !Array.isArray(value);
    case 'array': return Array.isArray(value);
    case 'string': return typeof value === 'string';
    case 'integer': return typeof value === 'number' && Number.isInteger(value);
    case 'number': return typeof value === 'number' && Number.isFinite(value);
    case 'boolean': return typeof value === 'boolean';
    default: return true;
  }
}

export function validateAgainstSchema(value, schema, path = '$') {
  const errors = [];
  if (schema.type && !schemaTypeMatches(schema.type, value)) {
    errors.push(`${path}: expected ${schema.type}`);
    return errors;
  }
  if (schema.enum && !schema.enum.includes(value)) {
    errors.push(`${path}: must be one of ${schema.enum.join('|')}`);
  }
  if (typeof value === 'number') {
    if (schema.minimum !== undefined && value < schema.minimum) {
      errors.push(`${path}: ${value} < minimum ${schema.minimum}`);
    }
    if (schema.maximum !== undefined && value > schema.maximum) {
      errors.push(`${path}: ${value} > maximum ${schema.maximum}`);
    }
  }
  if (typeof value === 'string') {
    if (schema.minLength !== undefined && value.length < schema.minLength) {
      errors.push(`${path}: string shorter than minLength ${schema.minLength}`);
    }
    if (schema.pattern && !new RegExp(schema.pattern, 'u').test(value)) {
      errors.push(`${path}: does not match pattern ${schema.pattern}`);
    }
  }
  if (Array.isArray(value)) {
    if (schema.minItems !== undefined && value.length < schema.minItems) {
      errors.push(`${path}: fewer than ${schema.minItems} items`);
    }
    if (schema.maxItems !== undefined && value.length > schema.maxItems) {
      errors.push(`${path}: more than ${schema.maxItems} items`);
    }
    if (schema.items) {
      value.forEach((item, index) => {
        errors.push(...validateAgainstSchema(item, schema.items, `${path}[${index}]`));
      });
    }
  }
  if (value !== null && typeof value === 'object' && !Array.isArray(value)) {
    for (const key of schema.required ?? []) {
      if (!(key in value)) errors.push(`${path}: missing required property "${key}"`);
    }
    for (const [key, propertySchema] of Object.entries(schema.properties ?? {})) {
      if (key in value) {
        errors.push(...validateAgainstSchema(value[key], propertySchema, `${path}.${key}`));
      }
    }
  }
  return errors;
}

// ---- 语义校验 ----

export function validateWorldLayout(world, schema) {
  const errors = validateAgainstSchema(world, schema);
  if (errors.length > 0) return errors;

  const countX = world.grid.countX;
  const countY = world.grid.countY;
  const coordInBounds = (value) => value >= 0.02 && value <= 0.98;
  const chunkOf = (value, count) => Math.min(count - 1, Math.floor(value * count));

  // district 唯一性、分块范围合法、覆盖全图且互不重叠。
  const districtById = new Map();
  const ownerByChunk = new Map();
  for (const district of world.districts) {
    if (districtById.has(district.districtId)) {
      errors.push(`duplicate districtId: ${district.districtId}`);
      continue;
    }
    districtById.set(district.districtId, district);
    const [xMin, xMax] = district.chunkXRange;
    const [yMin, yMax] = district.chunkYRange;
    if (xMin > xMax || yMin > yMax) {
      errors.push(`${district.districtId}: inverted chunk range`);
      continue;
    }
    for (let cy = yMin; cy <= yMax; cy += 1) {
      for (let cx = xMin; cx <= xMax; cx += 1) {
        const key = `${cx},${cy}`;
        if (ownerByChunk.has(key)) {
          errors.push(`chunk (${key}) claimed by both ${ownerByChunk.get(key)} and ${district.districtId}`);
        } else {
          ownerByChunk.set(key, district.districtId);
        }
      }
    }
  }
  for (let cy = 0; cy < countY; cy += 1) {
    for (let cx = 0; cx < countX; cx += 1) {
      if (!ownerByChunk.has(`${cx},${cy}`)) {
        errors.push(`chunk (${cx},${cy}) not covered by any district`);
      }
    }
  }

  const inDeclaredDistrict = (label, districtId, x, y) => {
    const district = districtById.get(districtId);
    if (!district) {
      errors.push(`${label}: unknown districtId ${districtId}`);
      return;
    }
    const cx = chunkOf(x, countX);
    const cy = chunkOf(y, countY);
    const [xMin, xMax] = district.chunkXRange;
    const [yMin, yMax] = district.chunkYRange;
    if (cx < xMin || cx > xMax || cy < yMin || cy > yMax) {
      errors.push(`${label}: (${x}, ${y}) -> chunk (${cx},${cy}) outside district ${districtId} [x ${xMin}-${xMax}, y ${yMin}-${yMax}]`);
    }
  };

  // 数值 id 全局唯一（锚点/NPC/宝箱/采集物共享存档 id 空间）。
  const seenIds = new Map();
  const claimId = (label, id) => {
    if (seenIds.has(id)) {
      errors.push(`duplicate entity id ${id}: ${seenIds.get(id)} and ${label}`);
    } else {
      seenIds.set(id, label);
    }
  };

  for (const anchor of world.anchors) {
    claimId(`anchor`, anchor.id);
    if (!coordInBounds(anchor.x) || !coordInBounds(anchor.y)) {
      errors.push(`anchor ${anchor.id}: coordinates out of [0.02, 0.98]`);
    }
    inDeclaredDistrict(`anchor ${anchor.id}`, anchor.districtId, anchor.x, anchor.y);
  }

  const seenDialogIds = new Set();
  for (const npc of world.npcs) {
    claimId(`npc`, npc.id);
    if (seenDialogIds.has(npc.dialogId)) {
      errors.push(`duplicate dialogId ${npc.dialogId}`);
    }
    seenDialogIds.add(npc.dialogId);
    if (npc.behavior === 'patrol' && (npc.patrolPoints.length < 2 || npc.patrolPoints.length > 4)) {
      errors.push(`npc ${npc.id}: patrol behavior needs 2-4 patrolPoints`);
    }
    if (npc.behavior === 'idle' && npc.patrolPoints.length !== 0) {
      errors.push(`npc ${npc.id}: idle behavior must have empty patrolPoints`);
    }
    inDeclaredDistrict(`npc ${npc.id}`, npc.districtId, npc.x, npc.y);
    npc.patrolPoints.forEach(([px, py], index) => {
      if (!coordInBounds(px) || !coordInBounds(py)) {
        errors.push(`npc ${npc.id}: patrolPoints[${index}] out of [0.02, 0.98]`);
      }
      inDeclaredDistrict(`npc ${npc.id} patrolPoints[${index}]`, npc.districtId, px, py);
    });
  }

  const archetypes = new Set(['RiftClaw', 'Priest', 'Guard', 'Bruiser', 'Caster', 'Elite']);
  const seenZoneIds = new Set();
  for (const zone of world.spawnZones) {
    if (seenZoneIds.has(zone.zoneId)) errors.push(`duplicate zoneId: ${zone.zoneId}`);
    seenZoneIds.add(zone.zoneId);
    if (!archetypes.has(zone.archetype)) {
      errors.push(`${zone.zoneId}: unknown archetype ${zone.archetype}`);
    }
    if (zone.positions.length !== zone.count) {
      errors.push(`${zone.zoneId}: positions length ${zone.positions.length} != count ${zone.count}`);
    }
    zone.positions.forEach(([px, py], index) => {
      if (!coordInBounds(px) || !coordInBounds(py)) {
        errors.push(`${zone.zoneId}: positions[${index}] out of [0.02, 0.98]`);
      }
      inDeclaredDistrict(`${zone.zoneId} positions[${index}]`, zone.districtId, px, py);
    });
    if (!coordInBounds(zone.patrolCenter[0]) || !coordInBounds(zone.patrolCenter[1])) {
      errors.push(`${zone.zoneId}: patrolCenter out of [0.02, 0.98]`);
    }
    inDeclaredDistrict(`${zone.zoneId} patrolCenter`, zone.districtId, zone.patrolCenter[0], zone.patrolCenter[1]);
  }

  for (const chest of world.chests) {
    claimId(`chest`, chest.id);
    if (!coordInBounds(chest.x) || !coordInBounds(chest.y)) {
      errors.push(`chest ${chest.id}: coordinates out of [0.02, 0.98]`);
    }
    inDeclaredDistrict(`chest ${chest.id}`, chest.districtId, chest.x, chest.y);
  }

  for (const collectible of world.collectibles) {
    claimId(`collectible`, collectible.id);
    if (!coordInBounds(collectible.x) || !coordInBounds(collectible.y)) {
      errors.push(`collectible ${collectible.id}: coordinates out of [0.02, 0.98]`);
    }
    inDeclaredDistrict(`collectible ${collectible.id}`, collectible.districtId, collectible.x, collectible.y);
  }

  const motionStates = new Set(['Grounded', 'Airborne', 'Gliding', 'Climbing', 'Swimming']);
  const seenExplorationIds = new Map();
  const claimExplorationId = (kind, id) => {
    if (seenExplorationIds.has(id)) {
      errors.push(`duplicate exploration id ${id}: ${seenExplorationIds.get(id)} and ${kind}`);
    } else {
      seenExplorationIds.set(id, kind);
    }
  };
  for (const poi of world.pointsOfInterest) {
    claimExplorationId('pointOfInterest', poi.id);
    if (!coordInBounds(poi.x) || !coordInBounds(poi.y)) {
      errors.push(`pointOfInterest ${poi.id}: coordinates out of [0.02, 0.98]`);
    }
    inDeclaredDistrict(`pointOfInterest ${poi.id}`, poi.districtId, poi.x, poi.y);
  }
  const gateIds = new Set(world.traversalGates.map((gate) => gate.id));
  const rewardIds = new Set(world.explorationRewards.map((reward) => reward.id));
  for (const puzzle of world.puzzles) {
    claimExplorationId('puzzle', puzzle.id);
    if (!coordInBounds(puzzle.x) || !coordInBounds(puzzle.y)) {
      errors.push(`puzzle ${puzzle.id}: coordinates out of [0.02, 0.98]`);
    }
    if (!motionStates.has(puzzle.requiredMotion)) {
      errors.push(`puzzle ${puzzle.id}: unknown requiredMotion ${puzzle.requiredMotion}`);
    }
    if (!gateIds.has(puzzle.opensGateId)) errors.push(`puzzle ${puzzle.id}: unknown gate ${puzzle.opensGateId}`);
    if (!rewardIds.has(puzzle.rewardId)) errors.push(`puzzle ${puzzle.id}: unknown reward ${puzzle.rewardId}`);
  }
  for (const gate of world.traversalGates) {
    claimExplorationId('traversalGate', gate.id);
    if (!coordInBounds(gate.x) || !coordInBounds(gate.y)) {
      errors.push(`traversalGate ${gate.id}: coordinates out of [0.02, 0.98]`);
    }
    if (!motionStates.has(gate.requiredMotion)) {
      errors.push(`traversalGate ${gate.id}: unknown requiredMotion ${gate.requiredMotion}`);
    }
    if (!Array.isArray(gate.halfExtents) || gate.halfExtents.length !== 2 ||
        gate.halfExtents.some((value) => !Number.isFinite(value) || value <= 0 || value > 0.15)) {
      errors.push(`traversalGate ${gate.id}: halfExtents must contain two finite values in (0, 0.15]`);
    }
    if (!Number.isFinite(gate.yaw)) {
      errors.push(`traversalGate ${gate.id}: yaw must be finite`);
    }
    if (!Number.isFinite(gate.top) || gate.top <= 0 || gate.top > 0.5) {
      errors.push(`traversalGate ${gate.id}: top must be finite and in (0, 0.5]`);
    }
  }
  for (const reward of world.explorationRewards) {
    claimExplorationId('explorationReward', reward.id);
    if (reward.sourceTraces === 0 && reward.gold === 0 && reward.fate === 0 && reward.itemCount === 0) {
      errors.push(`explorationReward ${reward.id}: reward must not be empty`);
    }
  }

  // 地形特征：featureId 唯一、kind 合法、几何参数有效、districtId 存在。
  const featureKinds = new Set(['hill', 'basin', 'terrace', 'ridge']);
  const seenFeatureIds = new Set();
  for (const feature of world.terrainFeatures ?? []) {
    if (seenFeatureIds.has(feature.featureId)) {
      errors.push(`duplicate terrainFeature featureId: ${feature.featureId}`);
    }
    seenFeatureIds.add(feature.featureId);
    if (!featureKinds.has(feature.kind)) {
      errors.push(`terrainFeature ${feature.featureId}: unknown kind ${feature.kind}`);
    }
    if (feature.districtId !== undefined && !districtById.has(feature.districtId)) {
      errors.push(`terrainFeature ${feature.featureId}: unknown districtId ${feature.districtId}`);
    }
    for (const key of ['x', 'y']) {
      if (!Number.isFinite(feature[key]) || feature[key] < 0 || feature[key] > 1) {
        errors.push(`terrainFeature ${feature.featureId}: ${key} must be finite in [0, 1]`);
      }
    }
    for (const key of ['radiusX', 'radiusY']) {
      if (!Number.isFinite(feature[key]) || feature[key] <= 0 || feature[key] > 0.5) {
        errors.push(`terrainFeature ${feature.featureId}: ${key} must be finite in (0, 0.5]`);
      }
    }
    if (!Number.isFinite(feature.feather) || feature.feather <= 0 || feature.feather > 1) {
      errors.push(`terrainFeature ${feature.featureId}: feather must be finite in (0, 1]`);
    }
    if (feature.kind === 'hill' &&
        (!Number.isFinite(feature.amplitude) || feature.amplitude <= 0 || feature.amplitude > 0.2)) {
      errors.push(`terrainFeature ${feature.featureId}: hill amplitude must be finite in (0, 0.2]`);
    }
    if ((feature.kind === 'basin' || feature.kind === 'terrace') &&
        (!Number.isFinite(feature.targetHeight) || feature.targetHeight < -0.2 || feature.targetHeight > 0.2)) {
      errors.push(`terrainFeature ${feature.featureId}: targetHeight must be finite in [-0.2, 0.2]`);
    }
    if (feature.kind === 'ridge') {
      if (!Number.isFinite(feature.amplitude) || feature.amplitude <= 0 || feature.amplitude > 0.1) {
        errors.push(`terrainFeature ${feature.featureId}: ridge amplitude must be finite in (0, 0.1]`);
      }
      if (!Number.isFinite(feature.frequency) || feature.frequency < 1 || feature.frequency > 16) {
        errors.push(`terrainFeature ${feature.featureId}: ridge frequency must be finite in [1, 16]`);
      }
      if (!Number.isFinite(feature.angleRadians)) {
        errors.push(`terrainFeature ${feature.featureId}: angleRadians must be finite`);
      }
    }
  }

  // 路线：端点必须引用存在的 mainRoute POI。
  const poiById = new Map(world.pointsOfInterest.map((poi) => [poi.id, poi]));
  for (const route of world.routes ?? []) {
    for (const key of ['fromPoiId', 'toPoiId']) {
      if (!poiById.has(route[key])) {
        errors.push(`route ${route.fromPoiId}->${route.toPoiId}: unknown ${key} ${route[key]}`);
      }
    }
  }

  return errors;
}

// ---- C++ 头文件生成 ----

function floatLiteral(value) {
  if (!Number.isFinite(value)) throw new Error(`non-finite number: ${value}`);
  const text = Object.is(value, -0) ? '0' : String(value);
  return /[-+eE.]|\bInfinity\b|\bNaN\b/.test(text) ? `${text}f` : `${text}.0f`;
}

function cxxStringLiteral(value) {
  const escaped = value.replaceAll('\\', '\\\\').replaceAll('"', '\\"');
  return `"${escaped}"sv`;
}

function patrolArrays(points, axis) {
  const maxPoints = 4;
  const values = new Array(maxPoints).fill('0.0f');
  points.forEach(([x, y], index) => {
    values[index] = floatLiteral(axis === 'x' ? x : y);
  });
  return `        {${values.join(', ')}},`;
}

function positionArrays(positions, axis) {
  const maxPositions = 3;
  const values = new Array(maxPositions).fill('0.0f');
  positions.forEach(([x, y], index) => {
    values[index] = floatLiteral(axis === 'x' ? x : y);
  });
  return `        {${values.join(', ')}},`;
}

export function generateWorldLayoutHeader(world) {
  const lines = [];
  const emit = (line = '') => lines.push(line);

  emit('// AUTO-GENERATED by automation/assets/generate_world_layout.mjs');
  emit('// Source of truth: assets/world/world.json. DO NOT EDIT BY HAND.');
  emit('// Regenerate with: node automation/assets/generate_world_layout.mjs');
  emit('#pragma once');
  emit();
  emit('#include <array>');
  emit('#include <cstdint>');
  emit('#include <string_view>');
  emit();
  emit('namespace WorldLayout {');
  emit();
  emit('using namespace std::string_view_literals;');
  emit();
  emit(`constexpr int32_t kGridCountX = ${world.grid.countX};`);
  emit(`constexpr int32_t kGridCountY = ${world.grid.countY};`);
  emit('constexpr float kCoordMin = 0.02f;');
  emit('constexpr float kCoordMax = 0.98f;');
  emit('constexpr int32_t kMaxNpcPatrolPoints = 4;');
  emit('constexpr int32_t kMaxSpawnPositions = 3;');
  emit();
  emit('enum class NpcBehavior : int32_t { Idle = 0, Patrol = 1 };');
  emit();
  emit('enum class SpawnArchetype : int32_t {');
  emit('  RiftClaw = 0, Priest = 1, Guard = 2, Bruiser = 3, Caster = 4, Elite = 5,');
  emit('};');
  emit();
  emit('constexpr std::string_view ArchetypeName(SpawnArchetype archetype) {');
  emit('  switch (archetype) {');
  emit('    case SpawnArchetype::RiftClaw: return "RiftClaw"sv;');
  emit('    case SpawnArchetype::Priest: return "Priest"sv;');
  emit('    case SpawnArchetype::Guard: return "Guard"sv;');
  emit('    case SpawnArchetype::Bruiser: return "Bruiser"sv;');
  emit('    case SpawnArchetype::Caster: return "Caster"sv;');
  emit('    case SpawnArchetype::Elite: return "Elite"sv;');
  emit('  }');
  emit('  return "Unknown"sv;');
  emit('}');
  emit();
  emit('struct WorldDistrictDef {');
  emit('  std::string_view districtId;');
  emit('  std::string_view name;');
  emit('  std::string_view description;');
  emit('  int32_t chunkXMin;');
  emit('  int32_t chunkXMax;');
  emit('  int32_t chunkYMin;');
  emit('  int32_t chunkYMax;');
  emit('};');
  emit();
  emit('struct WorldAnchorDef {');
  emit('  int32_t id;');
  emit('  float x;');
  emit('  float y;');
  emit('  std::string_view label;');
  emit('  std::string_view districtId;');
  emit('};');
  emit();
  emit('struct WorldNpcDef {');
  emit('  int32_t id;');
  emit('  float x;');
  emit('  float y;');
  emit('  float facing;');
  emit('  int32_t dialogId;');
  emit('  NpcBehavior behavior;');
  emit('  int32_t patrolCount;');
  emit('  float patrolX[kMaxNpcPatrolPoints];');
  emit('  float patrolY[kMaxNpcPatrolPoints];');
  emit('  std::string_view label;');
  emit('  std::string_view districtId;');
  emit('};');
  emit();
  emit('struct WorldSpawnZoneDef {');
  emit('  std::string_view zoneId;');
  emit('  std::string_view districtId;');
  emit('  std::string_view aggroGroup;');
  emit('  SpawnArchetype archetype;');
  emit('  int32_t count;');
  emit('  int32_t respawnMs;');
  emit('  float patrolCenterX;');
  emit('  float patrolCenterY;');
  emit('  float positionX[kMaxSpawnPositions];');
  emit('  float positionY[kMaxSpawnPositions];');
  emit('};');
  emit();
  emit('struct WorldChestDef {');
  emit('  int32_t id;');
  emit('  float x;');
  emit('  float y;');
  emit('  std::string_view label;');
  emit('  std::string_view districtId;');
  emit('};');
  emit();
  emit('struct WorldCollectibleDef {');
  emit('  int32_t id;');
  emit('  float x;');
  emit('  float y;');
  emit('  std::string_view kind;');
  emit('  std::string_view label;');
  emit('  std::string_view districtId;');
  emit('};');
  emit();
  emit('struct WorldPointOfInterestDef {');
  emit('  int32_t id; float x; float y;');
  emit('  std::string_view label; std::string_view districtId; bool mainRoute;');
  emit('};');
  emit();
  emit('enum class TraversalMotion : int32_t { Grounded = 0, Airborne = 1, Gliding = 2, Climbing = 3, Swimming = 4 };');
  emit('struct WorldPuzzleNodeDef {');
  emit('  int32_t id; float x; float y; std::string_view label;');
  emit('  TraversalMotion requiredMotion; int32_t opensGateId; int32_t rewardId;');
  emit('};');
  emit();
  emit('struct WorldTraversalGateDef {');
  emit('  int32_t id; float x; float y; std::string_view label; TraversalMotion requiredMotion;');
  emit('  float halfExtents[2]; float yaw; float top;');
  emit('};');
  emit();
  emit('struct WorldExplorationRewardDef {');
  emit('  int32_t id; std::string_view label; int32_t sourceTraces; int32_t gold;');
  emit('  int32_t fate; int32_t itemId; int32_t itemCount;');
  emit('};');
  emit();
  emit('// 地形特征（原神式手工地貌）：kind 取值与引擎 TerrainFeatureKind 严格一致');
  emit('// （0=Hill 加性丘 / 1=Basin 双向拉向目标高度 / 2=Terrace 只抬升 / 3=Ridge 掩码脊线）。');
  emit('struct WorldTerrainFeatureDef {');
  emit('  std::string_view featureId;');
  emit('  std::string_view districtId;');
  emit('  int32_t kind;');
  emit('  float x; float y;');
  emit('  float radiusX; float radiusY;');
  emit('  float amplitude; float targetHeight;');
  emit('  float frequency; float angleRadians;');
  emit('  float feather;');
  emit('};');
  emit();
  emit('// 主干道段：连接 mainRoute POI 的直线段，渲染层在地形上压出路径色。');
  emit('struct WorldRouteDef {');
  emit('  int32_t fromPoiId; int32_t toPoiId;');
  emit('  float fromX; float fromY; float toX; float toY;');
  emit('};');
  emit();

  emit(`constexpr std::size_t kDistrictCount = ${world.districts.length};`);
  emit('constexpr std::array<WorldDistrictDef, kDistrictCount> kDistricts{{');
  for (const district of world.districts) {
    emit('    {');
    emit(`        ${cxxStringLiteral(district.districtId)},`);
    emit(`        ${cxxStringLiteral(district.name)},`);
    emit(`        ${cxxStringLiteral(district.description)},`);
    emit(`        ${district.chunkXRange[0]}, ${district.chunkXRange[1]}, ${district.chunkYRange[0]}, ${district.chunkYRange[1]},`);
    emit('    },');
  }
  emit('}};');
  emit();

  emit(`constexpr std::size_t kAnchorCount = ${world.anchors.length};`);
  emit('constexpr std::array<WorldAnchorDef, kAnchorCount> kAnchors{{');
  for (const anchor of world.anchors) {
    emit('    {');
    emit(`        ${anchor.id}, ${floatLiteral(anchor.x)}, ${floatLiteral(anchor.y)},`);
    emit(`        ${cxxStringLiteral(anchor.label)}, ${cxxStringLiteral(anchor.districtId)},`);
    emit('    },');
  }
  emit('}};');
  emit();

  emit(`constexpr std::size_t kNpcCount = ${world.npcs.length};`);
  emit('constexpr std::array<WorldNpcDef, kNpcCount> kNpcs{{');
  for (const npc of world.npcs) {
    emit('    {');
    emit(`        ${npc.id}, ${floatLiteral(npc.x)}, ${floatLiteral(npc.y)}, ${floatLiteral(npc.facing)},`);
    emit(`        ${npc.dialogId}, NpcBehavior::${npc.behavior === 'patrol' ? 'Patrol' : 'Idle'}, ${npc.patrolPoints.length},`);
    emit(patrolArrays(npc.patrolPoints, 'x'));
    emit(patrolArrays(npc.patrolPoints, 'y'));
    emit(`        ${cxxStringLiteral(npc.label)}, ${cxxStringLiteral(npc.districtId)},`);
    emit('    },');
  }
  emit('}};');
  emit();

  emit(`constexpr std::size_t kSpawnZoneCount = ${world.spawnZones.length};`);
  emit('constexpr std::array<WorldSpawnZoneDef, kSpawnZoneCount> kSpawnZones{{');
  for (const zone of world.spawnZones) {
    emit('    {');
    emit(`        ${cxxStringLiteral(zone.zoneId)}, ${cxxStringLiteral(zone.districtId)}, ${cxxStringLiteral(zone.aggroGroup)},`);
    emit(`        SpawnArchetype::${zone.archetype}, ${zone.count}, ${zone.respawnMs},`);
    emit(`        ${floatLiteral(zone.patrolCenter[0])}, ${floatLiteral(zone.patrolCenter[1])},`);
    emit(positionArrays(zone.positions, 'x'));
    emit(positionArrays(zone.positions, 'y'));
    emit('    },');
  }
  emit('}};');
  emit();

  emit(`constexpr std::size_t kChestCount = ${world.chests.length};`);
  emit('constexpr std::array<WorldChestDef, kChestCount> kChests{{');
  for (const chest of world.chests) {
    emit('    {');
    emit(`        ${chest.id}, ${floatLiteral(chest.x)}, ${floatLiteral(chest.y)},`);
    emit(`        ${cxxStringLiteral(chest.label)}, ${cxxStringLiteral(chest.districtId)},`);
    emit('    },');
  }
  emit('}};');
  emit();

  emit(`constexpr std::size_t kCollectibleCount = ${world.collectibles.length};`);
  emit('constexpr std::array<WorldCollectibleDef, kCollectibleCount> kCollectibles{{');
  for (const collectible of world.collectibles) {
    emit('    {');
    emit(`        ${collectible.id}, ${floatLiteral(collectible.x)}, ${floatLiteral(collectible.y)},`);
    emit(`        ${cxxStringLiteral(collectible.kind)}, ${cxxStringLiteral(collectible.label)}, ${cxxStringLiteral(collectible.districtId)},`);
    emit('    },');
  }
  emit('}};');
  emit();
  emit(`constexpr std::size_t kPointOfInterestCount = ${world.pointsOfInterest.length};`);
  emit('constexpr std::array<WorldPointOfInterestDef, kPointOfInterestCount> kPointsOfInterest{{');
  for (const poi of world.pointsOfInterest) {
    emit(`    {${poi.id}, ${floatLiteral(poi.x)}, ${floatLiteral(poi.y)}, ${cxxStringLiteral(poi.label)}, ${cxxStringLiteral(poi.districtId)}, ${poi.mainRoute ? 'true' : 'false'}},`);
  }
  emit('}};');
  emit();
  const motionLiteral = (value) => `TraversalMotion::${value}`;
  emit(`constexpr std::size_t kPuzzleNodeCount = ${world.puzzles.length};`);
  emit('constexpr std::array<WorldPuzzleNodeDef, kPuzzleNodeCount> kPuzzleNodes{{');
  for (const puzzle of world.puzzles) {
    emit(`    {${puzzle.id}, ${floatLiteral(puzzle.x)}, ${floatLiteral(puzzle.y)}, ${cxxStringLiteral(puzzle.label)}, ${motionLiteral(puzzle.requiredMotion)}, ${puzzle.opensGateId}, ${puzzle.rewardId}},`);
  }
  emit('}};');
  emit();
  emit(`constexpr std::size_t kTraversalGateCount = ${world.traversalGates.length};`);
  emit('constexpr std::array<WorldTraversalGateDef, kTraversalGateCount> kTraversalGates{{');
  for (const gate of world.traversalGates) {
    emit(`    {${gate.id}, ${floatLiteral(gate.x)}, ${floatLiteral(gate.y)}, ${cxxStringLiteral(gate.label)}, ${motionLiteral(gate.requiredMotion)}, {${floatLiteral(gate.halfExtents[0])}, ${floatLiteral(gate.halfExtents[1])}}, ${floatLiteral(gate.yaw)}, ${floatLiteral(gate.top)}},`);
  }
  emit('}};');
  emit();
  emit(`constexpr std::size_t kExplorationRewardCount = ${world.explorationRewards.length};`);
  emit('constexpr std::array<WorldExplorationRewardDef, kExplorationRewardCount> kExplorationRewards{{');
  for (const reward of world.explorationRewards) {
    emit(`    {${reward.id}, ${cxxStringLiteral(reward.label)}, ${reward.sourceTraces}, ${reward.gold}, ${reward.fate}, ${reward.itemId}, ${reward.itemCount}},`);
  }
  emit('}};');
  emit();
  const terrainFeatureKindValue = { hill: 0, basin: 1, terrace: 2, ridge: 3 };
  emit(`constexpr std::size_t kTerrainFeatureCount = ${world.terrainFeatures.length};`);
  emit('constexpr std::array<WorldTerrainFeatureDef, kTerrainFeatureCount> kTerrainFeatures{{');
  for (const feature of world.terrainFeatures) {
    emit('    {');
    emit(`        ${cxxStringLiteral(feature.featureId)}, ${cxxStringLiteral(feature.districtId)}, ${terrainFeatureKindValue[feature.kind]},`);
    emit(`        ${floatLiteral(feature.x)}, ${floatLiteral(feature.y)},`);
    emit(`        ${floatLiteral(feature.radiusX)}, ${floatLiteral(feature.radiusY)},`);
    emit(`        ${floatLiteral(feature.amplitude ?? 0)}, ${floatLiteral(feature.targetHeight ?? 0)},`);
    emit(`        ${floatLiteral(feature.frequency ?? 0)}, ${floatLiteral(feature.angleRadians ?? 0)},`);
    emit(`        ${floatLiteral(feature.feather)},`);
    emit('    },');
  }
  emit('}};');
  emit();
  const poiById = new Map(world.pointsOfInterest.map((poi) => [poi.id, poi]));
  emit(`constexpr std::size_t kRouteCount = ${world.routes.length};`);
  emit('constexpr std::array<WorldRouteDef, kRouteCount> kRoutes{{');
  for (const route of world.routes) {
    const from = poiById.get(route.fromPoiId);
    const to = poiById.get(route.toPoiId);
    emit(`    {${route.fromPoiId}, ${route.toPoiId}, ${floatLiteral(from.x)}, ${floatLiteral(from.y)}, ${floatLiteral(to.x)}, ${floatLiteral(to.y)}},`);
  }
  emit('}};');
  emit();
  emit('}  // namespace WorldLayout');

  return `${lines.join('\n')}\n`;
}

async function main() {
  const root = resolve(dirname(fileURLToPath(import.meta.url)), '../..');
  const worldPath = join(root, 'assets/world/world.json');
  const schemaPath = join(root, 'config/schema/world.schema.json');
  const outputPath = join(root, 'native/generated/world_layout.gen.h');

  const world = JSON.parse(await readFile(worldPath, 'utf8'));
  const schema = JSON.parse(await readFile(schemaPath, 'utf8'));

  const errors = validateWorldLayout(world, schema);
  if (errors.length > 0) {
    for (const error of errors) console.error(`WORLD LAYOUT ERROR: ${error}`);
    throw new Error(`world layout validation failed with ${errors.length} error(s)`);
  }

  const header = generateWorldLayoutHeader(world);
  await mkdir(dirname(outputPath), { recursive: true });
  // 幂等：内容一致时不重写，避免无意义的文件时间戳变更。
  let previous = null;
  try {
    previous = await readFile(outputPath, 'utf8');
  } catch {
    previous = null;
  }
  if (previous !== header) {
    await writeFile(outputPath, header, 'utf8');
  }

  console.log(`WORLD LAYOUT GEN: ${previous === header ? 'up to date' : 'written'} ${outputPath}`);
  console.log(`WORLD LAYOUT GEN: districts=${world.districts.length} anchors=${world.anchors.length}` +
    ` npcs=${world.npcs.length} spawnZones=${world.spawnZones.length}` +
    ` chests=${world.chests.length} collectibles=${world.collectibles.length}`);
}

if (process.argv[1] &&
    pathToFileURL(resolve(process.argv[1])).href === import.meta.url) {
  await main();
}

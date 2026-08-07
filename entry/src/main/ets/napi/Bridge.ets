import native from 'libnative_game.so';

interface InputEvent {
  type: number;
  pointerId: number;
  x: number;
  y: number;
}

export interface Snapshot {
  tick: number;
  hp: number;
  poise: number;
  x: number;
  y: number;
  fps: number;
  moving: boolean;
  moveX: number;
  moveY: number;
  cameraYaw: number;
  cameraPitch: number;
  targetDist: number;
  targetArchetype: number;
  targetHpRatio: number;
  targetId: number;
  bossPhase: number;
  encounterMode: number;
  encounterState: number;
  rendererReady: boolean;
  environmentReady: boolean;
  environmentDrawCalls: number;
  environmentTriangles: number;
  stamina: number;
  comboSegment: number;
  invulnerable: boolean;
  insightMs: number;
  resonance: number;
  targetHp: number;
  targetPoise: number;
  pulseHitRemainingMs: number;
  lastRejectReason: number;
  currentAction: number;
  comboWindowMs: number;
  radianceCooldownMs: number;
  currentCooldownMs: number;
  corruptionCooldownMs: number;
  radianceCooldownTotalMs: number;
  currentCooldownTotalMs: number;
  corruptionCooldownTotalMs: number;
  ultimateWindowMs: number;
  targetPoiseBroken: boolean;
  radianceAttached: boolean;
  currentAttached: boolean;
  corruptionAttached: boolean;
  corroded: boolean;
  currentReaction: number;
  pulsePhase: number;
  levelStage: number;
  gateState: number;
  supplyState: number;
  bossHp: number;
  bossPoise: number;
  bossMechanic: number;
  bossCastMs: number;
  perfLevel: number;
  vfxFlags: number;
  cameraShakeX: number;
  cameraShakeY: number;
  bossHpRatio: number;
  bossCastRatio: number;
  debugHud: boolean;
  objectiveLabel: string;
  resonanceSlots: number[];
  showDebugHud: boolean;
  bossCinematicProgress: number;
  bossShardCount: number;
  bossSourceColor: number;
  bossRingBroken: boolean;
  inputEventCount: number;
  // 开放世界探索字段（阶段一）。
  explorationStamina: number;
  motionState: number;
  playerHeight: number;
  activeChunkCount: number;
  chunkLoadCount: number;
  interactionAnchorId: number;
  interactionUnlocked: boolean;
  interactionLabel: string;
  unlockedAnchorCount: number;
  cameraExploration: boolean;
  teleportFlashMs: number;
  minimapAnchorX: number[];
  minimapAnchorY: number[];
  minimapAnchorUnlocked: number[];
  // 内容与任务字段（阶段二）。
  questId: number;
  questStatus: number;
  questTitle: string;
  questObjectiveLabel: string;
  questObjectiveProgress: number;
  questObjectiveRequired: number;
  completedQuestCount: number;
  dialogActive: boolean;
  dialogSpeaker: string;
  dialogText: string;
  dialogLineIndex: number;
  dialogLineCount: number;
  interactionKind: number;
  // 养成与抽卡字段（阶段三）。
  fateCount: number;
  goldCount: number;
  expMaterialCount: number;
  ascensionMaterialCount: number;
  gachaPity5: number;
  gachaResultIds: number[];
  gachaResultRarities: number[];
  gachaResultIsNew: number[];
  rosterIds: number[];
  rosterLevels: number[];
  rosterAscensions: number[];
  // 阶段四打磨字段。
  activeCharacterId: number;
  dayNightHour: number;
  qualityPreset: number;
  weatherId: number;
  musicRegionId: number;
  completedSideQuestCount: number;
  dungeonState: number;
  dungeonProgress: number;
  dungeonRequired: number;
  minimapItemX: number[];
  minimapItemY: number[];
  minimapItemKind: number[];
  sideQuestProgress: number[];
  sideQuestRequired: number[];
  rosterHp: number[];
  rosterAtk: number[];
  rosterConstellations: number[];
  weaponIds: number[];
  weaponLevels: number[];
  weaponEquippedBy: number[];
  // 原神式养成：武器深化、圣遗物、冒险等级与新物品计数。
  weaponAscensions: number[];
  weaponRefines: number[];
  weaponRefineStocks: number[];
  weaponExps: number[];
  artifactInstanceIds: number[];
  artifactDefIds: number[];
  artifactRarities: number[];
  artifactLevels: number[];
  artifactEquippedBy: number[];
  artifactSeeds: number[];
  adventureRank: number;
  adventureExp: number;
  adventureExpRequired: number;
  worldLevel: number;
  oreLowCount: number;
  oreMidCount: number;
  oreHighCount: number;
  expSmallCount: number;
  expMediumCount: number;
  expLargeCount: number;
  sprintActive: number;
  dailyCompletedCount: number;
  dailyQuestClaimed: number;
  gachaPoolKind: number;
  // NPC 任务发布（Phase 4）：对话结束时接取的支线；-1 表示无。
  npcOfferQuestId: number;
  npcOfferQuestTitle: string;
  explorationPoiCount: number;
  explorationPuzzleCount: number;
  explorationRewardCount: number;
  explorationGateCount: number;
  explorationTraversalMask: number;
  explorationCurrentPoiId: number;
  explorationCurrentTargetLabel: string;
  explorationCurrentTargetDistrict: string;
  explorationBlockedGateId: number;
  explorationBlockedGateLabel: string;
  explorationBlockedByPuzzleLabel: string;
  explorationFeedbackType: number;
  explorationFeedbackId: number;
  explorationFeedbackTitle: string;
  explorationFeedbackSubtitle: string;
  explorationFeedbackRemainingMs: number;
}

export const nativeStart = (): void => native.nativeStart();
export const nativeStop = (): void => native.nativeStop();
export const nativeStartIfForeground = (): void => native.nativeStartIfForeground();
export const nativeSetModelAssets = (player: ArrayBuffer, enemy: ArrayBuffer,
  boss: ArrayBuffer): boolean => native.nativeSetModelAssets(player, enemy, boss);
export const nativeSetEnvironmentAssets = (outer: ArrayBuffer, center: ArrayBuffer,
  backdrop: ArrayBuffer, decoration: ArrayBuffer): boolean =>
  native.nativeSetEnvironmentAssets(outer, center, backdrop, decoration);
export const nativeSetBlockAsset = (blockId: number, bytes: ArrayBuffer): boolean =>
  native.nativeSetBlockAsset(blockId, bytes);
// NPC 模型槽位（Phase 4）：第一版由 GamePage 注入 player.glb 占位字节，
// 缺失时 native 侧保持静态 Mesh 回退。
export const nativeSetNpcAsset = (bytes: ArrayBuffer): boolean =>
  native.nativeSetNpcAsset(bytes);
export const pushInput = (e: InputEvent): void => native.pushInput(e);
export const pushAction = (type: number): void => native.pushAction(type);
export const startEncounter = (mode: number): boolean => native.startEncounter(mode);
export const advanceLevel = (): boolean => native.advanceLevel();
export const useSupply = (): boolean => native.useSupply();
export const retryBoss = (): boolean => native.retryBoss();
export const toggleDebugHud = (): void => native.toggleDebugHud();
export const advanceDialog = (): void => native.advanceDialog();
export const saveProgress = (path: string): boolean => native.saveProgress(path);
export const loadProgress = (path: string): boolean => native.loadProgress(path);
export const performGacha = (count: number): boolean => native.performGacha(count);
export const performWeaponGacha = (count: number): boolean => native.performWeaponGacha(count);
export const useExpMaterial = (characterId: number, materialCount: number): boolean =>
  native.useExpMaterial(characterId, materialCount);
export const ascendCharacter = (characterId: number): boolean =>
  native.ascendCharacter(characterId);
export const upgradeWeapon = (weaponId: number): boolean => native.upgradeWeapon(weaponId);
export const equipWeapon = (weaponId: number, characterId: number): boolean =>
  native.equipWeapon(weaponId, characterId);
export const upgradeWeaponWithOre = (weaponId: number, oreItemId: number, count: number): boolean =>
  native.upgradeWeaponWithOre(weaponId, oreItemId, count);
export const ascendWeapon = (weaponId: number): boolean => native.ascendWeapon(weaponId);
export const refineWeapon = (weaponId: number): boolean => native.refineWeapon(weaponId);
export const useExpItem = (characterId: number, itemId: number, count: number): boolean =>
  native.useExpItem(characterId, itemId, count);
export const upgradeArtifact = (targetInstanceId: number, feedInstanceIds: number[]): boolean =>
  native.upgradeArtifact(targetInstanceId, feedInstanceIds);
export const equipArtifact = (instanceId: number, characterId: number): boolean =>
  native.equipArtifact(instanceId, characterId);
export const claimRankReward = (rank: number): boolean => native.claimRankReward(rank);
export const setQualityPreset = (preset: number): void => native.setQualityPreset(preset);
export const teleportToAnchor = (anchorId: number): boolean => native.teleportToAnchor(anchorId);
export const setAudioEnabled = (enabled: boolean): void => native.setAudioEnabled(enabled);
export const setPaused = (paused: boolean): void => native.setPaused(paused);
export const skipDemoPhase = (phase: number): void => native.skipDemoPhase(phase);
export const pullSnapshot = (): Snapshot => native.pullSnapshot();

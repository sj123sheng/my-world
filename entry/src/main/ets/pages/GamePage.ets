import { nativeSetModelAssets, nativeSetTerrainAssets, nativeSetFoliageAtlas, nativeSetNpcAsset, nativeSetEnemyArchetypeAsset, nativeStartIfForeground,
  nativeStop, pullSnapshot, setAudioEnabled, saveProgress, loadProgress, Snapshot } from '../napi/Bridge';
import { TERRAIN_ATLAS_ASSET, TERRAIN_CONTROL_ASSET,
  FOLIAGE_ATLAS_ASSET } from '../generated/EnvironmentVisualManifest';
import { Hud } from '../ui/Hud';
import { CombatControls } from '../ui/CombatControls';
import { Joystick } from '../ui/Joystick';
import { HitFeedback } from '../ui/HitFeedback';
import { Haptics } from '../ui/Haptics';
import { ActionToast } from '../ui/ActionToast';
import { ComboCounter } from '../ui/ComboCounter';
import { Tutorial } from '../ui/Tutorial';
import { GameStateOverlay } from '../ui/GameStateOverlay';
import { PauseMenu } from '../ui/PauseMenu';
import { TargetFrame } from '../ui/TargetFrame';
import { ExplorationHud } from '../ui/ExplorationHud';
import { DialogBox } from '../ui/DialogBox';
import { GachaPanel } from '../ui/GachaPanel';
import { MapPanel } from '../ui/MapPanel';
import { QuestRewardToast } from '../ui/QuestRewardToast';
import { ExplorationToast } from '../ui/ExplorationToast';
import { InventoryPanel } from '../ui/InventoryPanel';
import { ArtifactPanel } from '../ui/ArtifactPanel';

@Entry
@Component
struct GamePage {
  private snapshot: Snapshot = {
    tick: 0, hp: 100, poise: 100, x: 0.5, y: 0.5, fps: 0, moving: false,
    moveX: 0, moveY: 0, cameraYaw: 0, cameraPitch: 0,
    targetDist: 0, targetId: 0, targetArchetype: -1, targetHpRatio: 0, bossPhase: 0, encounterMode: 0,
    encounterState: 0, rendererReady: true, environmentReady: false,
    environmentDrawCalls: 0, environmentTriangles: 0,
    stamina: 100, comboSegment: 0, invulnerable: false, insightMs: 0,
    resonance: 0, targetHp: 300, targetPoise: 100, pulseHitRemainingMs: 0,
    lastRejectReason: 0, currentAction: 0, comboWindowMs: 0,
    radianceCooldownMs: 0, currentCooldownMs: 0, corruptionCooldownMs: 0,
    radianceCooldownTotalMs: 3000, currentCooldownTotalMs: 4000,
    corruptionCooldownTotalMs: 5000,
    ultimateWindowMs: 0, targetPoiseBroken: false, radianceAttached: false,
    currentAttached: false, corruptionAttached: false, corroded: false,
    currentReaction: -1, pulsePhase: 0,
    levelStage: 0, gateState: 0, supplyState: 0,
    bossHp: 1000, bossPoise: 300, bossMechanic: 0, bossCastMs: 0
    , perfLevel: 0, vfxFlags: 0, cameraShakeX: 0, cameraShakeY: 0,
    bossHpRatio: 0, bossCastRatio: 0, debugHud: false,
    objectiveLabel: '', resonanceSlots: [0, 0, 0], showDebugHud: false,
    bossCinematicProgress: 0, bossShardCount: 0, bossSourceColor: 0,
    bossRingBroken: false,
    inputEventCount: 0,
    explorationStamina: 100, motionState: 0, playerHeight: 0,
    playerChunkX: 0, playerChunkY: 0, playerLocalX: 0.5, playerLocalY: 0.5,
    activeChunkCount: 0, cachedChunkCount: 0, streamingPendingCount: 0,
    interactionAnchorId: -1, interactionUnlocked: false, interactionLabel: '',
    unlockedAnchorCount: 0, cameraExploration: false, teleportFlashMs: 0,
    minimapAnchorX: [], minimapAnchorY: [], minimapAnchorUnlocked: [],
    questId: -1, questStatus: 0, questTitle: '', questObjectiveLabel: '',
    questObjectiveProgress: 0, questObjectiveRequired: 1,
    completedQuestCount: 0,
    dialogActive: false, dialogSpeaker: '', dialogText: '',
    dialogLineIndex: 0, dialogLineCount: 0, interactionKind: 0,
    fateCount: 0, goldCount: 0, expMaterialCount: 0,
    ascensionMaterialCount: 0, gachaPity5: 0,
    gachaResultIds: [], gachaResultRarities: [], gachaResultIsNew: [],
    rosterIds: [], rosterLevels: [], rosterAscensions: [],
    activeCharacterId: 1, dayNightHour: 8, qualityPreset: 0,
    weatherId: 0, musicRegionId: 0,
    completedSideQuestCount: 0, dungeonState: 0, dungeonProgress: 0,
    dungeonRequired: 3,
    minimapItemX: [], minimapItemY: [], minimapItemKind: [],
    sideQuestProgress: [], sideQuestRequired: [],
    rosterHp: [], rosterAtk: [],
    rosterConstellations: [], weaponIds: [], weaponLevels: [],
    weaponEquippedBy: [],
    weaponAscensions: [], weaponRefines: [], weaponRefineStocks: [],
    weaponExps: [],
    artifactInstanceIds: [], artifactDefIds: [], artifactRarities: [],
    artifactLevels: [], artifactEquippedBy: [], artifactSeeds: [],
    adventureRank: 1, adventureExp: 0, adventureExpRequired: 375,
    worldLevel: 0,
    oreLowCount: 0, oreMidCount: 0, oreHighCount: 0,
    expSmallCount: 0, expMediumCount: 0, expLargeCount: 0,
    sprintActive: 0, dailyCompletedCount: 0, dailyQuestClaimed: 0,
    gachaPoolKind: 0,
    npcOfferQuestId: -1, npcOfferQuestTitle: '',
    explorationPoiCount: 0, explorationPuzzleCount: 0,
    explorationRewardCount: 0, explorationGateCount: 0,
    explorationTraversalMask: 0, explorationCurrentPoiId: -1,
    explorationCurrentTargetLabel: '', explorationCurrentTargetDistrict: '',
    explorationFeedbackType: 0, explorationFeedbackId: -1,
    explorationFeedbackTitle: '', explorationFeedbackSubtitle: '',
    explorationFeedbackRemainingMs: 0
  };
  @State tick: number = 0;
  @State hp: number = 100;
  @State poise: number = 100;
  @State posX: number = 0.5;
  @State posY: number = 0.5;
  @State fps: number = 0;
  @State moving: boolean = false;
  @State moveX: number = 0;
  @State moveY: number = 0;
  @State cameraYaw: number = 0;
  @State cameraPitch: number = 0;
  @State targetDist: number = 0;
  @State targetId: number = 0;
  @State targetArchetype: number = -1;
  @State targetHpRatio: number = 0;
  @State bossPhase: number = 0;
  @State encounterMode: number = 0;
  @State encounterState: number = 0;
  @State rendererReady: boolean = true;
  @State environmentReady: boolean = false;
  @State environmentDrawCalls: number = 0;
  @State environmentTriangles: number = 0;
  @State stamina: number = 100;
  @State comboSegment: number = 0;
  @State invulnerable: boolean = false;
  @State insightMs: number = 0;
  @State resonance: number = 0;
  @State targetHp: number = 300;
  @State targetPoise: number = 100;
  @State pulseHitRemainingMs: number = 0;
  @State lastRejectReason: number = 0;
  @State currentAction: number = 0;
  @State comboWindowMs: number = 0;
  @State radianceCooldownMs: number = 0;
  @State currentCooldownMs: number = 0;
  @State corruptionCooldownMs: number = 0;
  @State radianceCooldownTotalMs: number = 3000;
  @State currentCooldownTotalMs: number = 4000;
  @State corruptionCooldownTotalMs: number = 5000;
  @State ultimateWindowMs: number = 0;
  @State targetPoiseBroken: boolean = false;
  @State radianceAttached: boolean = false;
  @State currentAttached: boolean = false;
  @State corruptionAttached: boolean = false;
  @State corroded: boolean = false;
  @State currentReaction: number = -1;
  @State pulsePhase: number = 0;
  @State levelStage: number = 0;
  @State gateState: number = 0;
  @State supplyState: number = 0;
  @State bossHp: number = 1000;
  @State bossPoise: number = 300;
  @State bossMechanic: number = 0;
  @State bossCastMs: number = 0;
  @State perfLevel: number = 0;
  @State vfxFlags: number = 0;
  @State cameraShakeX: number = 0;
  @State cameraShakeY: number = 0;
  @State bossHpRatio: number = 0;
  @State bossCastRatio: number = 0;
  @State debugHud: boolean = false;
  @State objectiveLabel: string = '';
  @State resonanceSlots: number[] = [0, 0, 0];
  @State showDebugHud: boolean = false;
  @State bossCinematicProgress: number = 0;
  @State bossShardCount: number = 0;
  @State bossSourceColor: number = 0;
  @State bossRingBroken: boolean = false;
  @State inputEventCount: number = 0;
  // 开放世界探索状态（阶段一）。
  @State explorationStamina: number = 100;
  @State motionState: number = 0;
  @State playerHeight: number = 0;
  @State playerChunkX: number = 0;
  @State playerChunkY: number = 0;
  @State playerLocalX: number = 0.5;
  @State playerLocalY: number = 0.5;
  @State activeChunkCount: number = 0;
  @State cachedChunkCount: number = 0;
  @State streamingPendingCount: number = 0;
  @State interactionAnchorId: number = -1;
  @State interactionUnlocked: boolean = false;
  @State interactionLabel: string = '';
  @State unlockedAnchorCount: number = 0;
  @State cameraExploration: boolean = false;
  @State teleportFlashMs: number = 0;
  @State minimapAnchorX: number[] = [];
  @State minimapAnchorY: number[] = [];
  @State minimapAnchorUnlocked: number[] = [];
  // 内容与任务状态（阶段二）。
  @State questId: number = -1;
  @State questStatus: number = 0;
  @State questTitle: string = '';
  @State questObjectiveLabel: string = '';
  @State questObjectiveProgress: number = 0;
  @State questObjectiveRequired: number = 1;
  @State completedQuestCount: number = 0;
  @State dialogActive: boolean = false;
  @State dialogSpeaker: string = '';
  @State dialogText: string = '';
  @State dialogLineIndex: number = 0;
  @State dialogLineCount: number = 0;
  @State interactionKind: number = 0;
  @State explorationPoiCount: number = 0;
  @State explorationPuzzleCount: number = 0;
  @State explorationRewardCount: number = 0;
  @State explorationGateCount: number = 0;
  @State explorationTraversalMask: number = 0;
  @State explorationCurrentPoiId: number = -1;
  @State explorationCurrentTargetLabel: string = '';
  @State explorationCurrentTargetDistrict: string = '';
  @State explorationFeedbackType: number = 0;
  @State explorationFeedbackId: number = -1;
  @State explorationFeedbackTitle: string = '';
  @State explorationFeedbackSubtitle: string = '';
  @State explorationFeedbackRemainingMs: number = 0;
  private prevExplorationFeedbackId: number = -1;
  // 养成与抽卡状态（阶段三）。
  @State fateCount: number = 0;
  @State goldCount: number = 0;
  @State expMaterialCount: number = 0;
  @State ascensionMaterialCount: number = 0;
  @State gachaPity5: number = 0;
  @State gachaResultIds: number[] = [];
  @State gachaResultRarities: number[] = [];
  @State gachaResultIsNew: number[] = [];
  @State rosterIds: number[] = [];
  @State rosterLevels: number[] = [];
  @State rosterAscensions: number[] = [];
  @State activeCharacterId: number = 1;
  @State dayNightHour: number = 8;
  @State qualityPreset: number = 0;
  @State weatherId: number = 0;
  @State musicRegionId: number = 0;
  @State completedSideQuestCount: number = 0;
  @State dungeonState: number = 0;
  @State dungeonProgress: number = 0;
  @State dungeonRequired: number = 3;
  @State minimapItemX: number[] = [];
  @State minimapItemY: number[] = [];
  @State minimapItemKind: number[] = [];
  @State sideQuestProgress: number[] = [];
  @State sideQuestRequired: number[] = [];
  @State rosterHp: number[] = [];
  @State rosterAtk: number[] = [];
  @State rosterConstellations: number[] = [];
  @State weaponIds: number[] = [];
  @State weaponLevels: number[] = [];
  @State weaponEquippedBy: number[] = [];
  // 原神式养成：武器深化、圣遗物、冒险等级与新物品计数。
  @State weaponAscensions: number[] = [];
  @State weaponRefines: number[] = [];
  @State weaponRefineStocks: number[] = [];
  @State weaponExps: number[] = [];
  @State artifactInstanceIds: number[] = [];
  @State artifactDefIds: number[] = [];
  @State artifactRarities: number[] = [];
  @State artifactLevels: number[] = [];
  @State artifactEquippedBy: number[] = [];
  @State artifactSeeds: number[] = [];
  @State adventureRank: number = 1;
  @State adventureExp: number = 0;
  @State adventureExpRequired: number = 375;
  @State worldLevel: number = 0;
  @State oreLowCount: number = 0;
  @State oreMidCount: number = 0;
  @State oreHighCount: number = 0;
  @State expSmallCount: number = 0;
  @State expMediumCount: number = 0;
  @State expLargeCount: number = 0;
  @State sprintActive: number = 0;
  @State dailyCompletedCount: number = 0;
  @State dailyQuestClaimed: number = 0;
  @State gachaPoolKind: number = 0;
  // NPC 任务接取提示（Phase 4）：快照边沿检测驱动 ActionToast 弹层。
  @State questAcceptMessage: string = '';
  private prevNpcOfferQuestId: number = -1;
  @State gachaOpen: boolean = false;
  @State mapOpen: boolean = false;
  @State inventoryOpen: boolean = false;
  @State artifactOpen: boolean = false;
  // 任务完成奖励浮层状态。
  @State rewardToastVisible: boolean = false;
  @State rewardToastTitle: string = '';
  @State rewardToastLabel: string = '';
  private rewardToastTimer: number = -1;
  private timer: number = -1;
  private pageActive: boolean = false;
  private modelLoadGeneration: number = 0;
  // Phase 2 区块环境批次懒加载：已尝试（含失败）的区块不重复读取。
  // 快照边沿检测上一帧采样值，用于触发触感反馈。
  private prevHpForHaptics: number = 100;
  private prevInsightMs: number = 0;
  private prevUltimateWindowMs: number = 0;
  private prevRadianceCd: number = 0;
  private prevCurrentCd: number = 0;
  private prevCorruptionCd: number = 0;
  private prevTargetPoiseBroken: boolean = false;
  // 上一帧锁定目标 HP：>0 归零视为击杀，触发确认触感。
  private prevTargetHp: number = 0;
  // 上一帧遭遇状态：胜利上升沿触发确认触感（2 = Victory）。
  private prevEncounterState: number = 0;
  // 上一帧完成任务数：上升沿即时落盘，保证杀进程不丢进度。
  private prevCompletedQuestCount: number = 0;

  private progressSavePath(): string {
    return `${getContext(this).filesDir}/ethelan_progress.dat`;
  }

  // 任务奖励文案：与 native 任务奖励表一一对应。
  private rewardLabelFor(completedCount: number): string {
    switch (completedCount) {
      case 1: return '共鸣之契 ×5';
      case 2: return '共鸣之契 ×5';
      case 3: return '金币 ×50 · 经验 ×5 · 武器：雾猎之弓';
      case 4: return '源晶碎片 ×3';
      default: return '共鸣之契 ×10 · 武器：守望大剑';
    }
  }

  // 对比相邻两次快照的边沿变化，触发对应触感反馈。
  private detectCombatEvents(): void {
    const snap: Snapshot = this.snapshot;
    // 受击：HP 下降；大额伤害重振。
    if (snap.hp < this.prevHpForHaptics) {
      if (this.prevHpForHaptics - snap.hp >= 15) {
        Haptics.heavy();
      } else {
        Haptics.hit();
      }
    }
    // 精准闪避：洞察窗口上升沿。
    if (snap.insightMs > 0 && this.prevInsightMs <= 0) {
      Haptics.sharp();
    }
    // 源技能命中：任一冷却上升沿。
    if ((snap.radianceCooldownMs > 0 && this.prevRadianceCd <= 0) ||
        (snap.currentCooldownMs > 0 && this.prevCurrentCd <= 0) ||
        (snap.corruptionCooldownMs > 0 && this.prevCorruptionCd <= 0)) {
      Haptics.light();
    }
    // 破韧：目标韧性击破上升沿。
    if (snap.targetPoiseBroken && !this.prevTargetPoiseBroken) {
      Haptics.heavy();
    }
    // 击杀确认：锁定目标 HP 从 >0 归零。
    if (this.prevTargetHp > 0 && snap.targetHp === 0) {
      Haptics.heavy();
    }
    // 胜利确认：遭遇状态进入胜利的上升沿。
    if (snap.encounterState === 2 && this.prevEncounterState !== 2) {
      Haptics.ultimateReady();
    }
    // 终结技就绪：终结窗口上升沿。
    if (snap.ultimateWindowMs > 0 && this.prevUltimateWindowMs <= 0) {
      Haptics.ultimateReady();
    }
    this.prevHpForHaptics = snap.hp;
    this.prevInsightMs = snap.insightMs;
    this.prevUltimateWindowMs = snap.ultimateWindowMs;
    this.prevRadianceCd = snap.radianceCooldownMs;
    this.prevCurrentCd = snap.currentCooldownMs;
    this.prevCorruptionCd = snap.corruptionCooldownMs;
    this.prevTargetPoiseBroken = snap.targetPoiseBroken;
    this.prevTargetHp = snap.targetHp;
    this.prevEncounterState = snap.encounterState;
  }

  private isActiveModelLoad(generation: number): boolean {
    return this.pageActive && generation === this.modelLoadGeneration;
  }

  private async loadModelAssets(generation: number): Promise<void> {
    try {
      const resourceManager = getContext(this).resourceManager;
      try {
        const assets: Uint8Array[] = await Promise.all([
          resourceManager.getRawFileContent('models/player.glb'),
          resourceManager.getRawFileContent('models/enemy.glb'),
          resourceManager.getRawFileContent('models/boss.glb')
        ]);
        if (!this.isActiveModelLoad(generation)) {
          return;
        }
        if (!nativeSetModelAssets(assets[0].buffer as ArrayBuffer,
          assets[1].buffer as ArrayBuffer, assets[2].buffer as ArrayBuffer)) {
          console.error('[Ethelan] Native rejected model asset buffers; using fallback meshes');
        }
        // NPC 模型槽位（Phase 4）：第一版复用 player.glb 占位，
        // 存在 models/npc.glb 时优先；注入失败保持静态 Mesh 回退。
        try {
          let npcAsset: Uint8Array = assets[0];
          try {
            npcAsset = await resourceManager.getRawFileContent('models/npc.glb');
          } catch (npcMissing) {
          }
          if (this.isActiveModelLoad(generation) &&
            !nativeSetNpcAsset(npcAsset.buffer as ArrayBuffer)) {
            console.error('[Ethelan] Native rejected npc asset; using fallback mesh');
          }
        } catch (npcError) {
        }
        // 敌人原型独立模型槽位（独立高模资产）：存在
        // models/enemy_<archetype>.glb 时逐原型注入，缺失/注入失败
        // 的原型保持共享 enemy.glb 回退，互不影响。
        for (let archetype = 0; archetype < 6; ++archetype) {
          try {
            const archetypeAsset: Uint8Array =
              await resourceManager.getRawFileContent(`models/enemy_${archetype}.glb`);
            if (this.isActiveModelLoad(generation) &&
              !nativeSetEnemyArchetypeAsset(archetype, archetypeAsset.buffer as ArrayBuffer)) {
              console.error(`[Ethelan] Native rejected enemy_${archetype} asset; using shared enemy model`);
            }
          } catch (archetypeMissing) {
          }
        }
      } catch (error) {
        if (this.isActiveModelLoad(generation)) {
          console.error('[Ethelan] Failed to load model assets; using fallback meshes: ' +
            JSON.stringify(error));
        }
      }
      try {
        const terrainAssets: Uint8Array[] = await Promise.all([
          resourceManager.getRawFileContent(TERRAIN_ATLAS_ASSET),
          resourceManager.getRawFileContent(TERRAIN_CONTROL_ASSET)
        ]);
        if (this.isActiveModelLoad(generation) &&
          !nativeSetTerrainAssets(terrainAssets[0].buffer as ArrayBuffer,
            terrainAssets[1].buffer as ArrayBuffer)) {
          console.error('[Ethelan] Native rejected terrain material assets');
        }
      } catch (error) {
        if (this.isActiveModelLoad(generation)) {
          console.error('[Ethelan] Failed to load terrain visual assets: ' + JSON.stringify(error));
        }
      }
      try {
        const foliageAtlas: Uint8Array =
          await resourceManager.getRawFileContent(FOLIAGE_ATLAS_ASSET);
        if (this.isActiveModelLoad(generation) &&
          !nativeSetFoliageAtlas(foliageAtlas.buffer as ArrayBuffer)) {
          console.error('[Ethelan] Native rejected foliage atlas');
        }
      } catch (error) {
        if (this.isActiveModelLoad(generation)) {
          console.error('[Ethelan] Failed to load foliage atlas: ' + JSON.stringify(error));
        }
      }
    } catch (error) {
      console.error('[Ethelan] Resource manager unavailable: ' + JSON.stringify(error));
    } finally {
      if (this.isActiveModelLoad(generation)) {
        nativeStartIfForeground();
      }
    }
  }

  build() {
    Stack() {
      XComponent({
        id: 'gameSurface',
        type: 'surface',
        libraryname: 'native_game'
      })
        .width('100%')
        .height('100%')
        .backgroundColor('#141628')
        .onDestroy(() => {
          nativeStop();
        })

      if (!this.rendererReady) {
        Column({ space: 12 }) {
          Text('当前图形环境不支持 GLES')
            .fontColor(Color.White)
            .fontSize(22)
            .fontWeight(FontWeight.Bold)
          Text('Native 渲染已安全停止，请重建模拟器或改用真机运行')
            .fontColor(Color.Gray)
            .fontSize(14)
            .textAlign(TextAlign.Center)
        }
        .padding(24)
        .alignItems(HorizontalAlign.Center)
      }

      Joystick()
        .hitTestBehavior(HitTestMode.Transparent)

      Hud({ hp: this.hp, poise: this.poise, x: this.posX, y: this.posY,
        fps: this.fps, moving: this.moving, moveX: this.moveX, moveY: this.moveY,
        cameraYaw: this.cameraYaw, targetDist: this.targetDist,
        stamina: this.stamina, comboSegment: this.comboSegment,
        invulnerable: this.invulnerable, insightMs: this.insightMs,
        resonance: this.resonance, targetHp: this.targetHp,
        targetPoise: this.targetPoise, pulseHitRemainingMs: this.pulseHitRemainingMs,
        lastRejectReason: this.lastRejectReason, currentAction: this.currentAction,
        encounterMode: this.encounterMode, encounterState: this.encounterState,
        comboWindowMs: this.comboWindowMs, radianceCooldownMs: this.radianceCooldownMs,
        currentCooldownMs: this.currentCooldownMs, corruptionCooldownMs: this.corruptionCooldownMs,
        ultimateWindowMs: this.ultimateWindowMs, targetPoiseBroken: this.targetPoiseBroken,
        radianceAttached: this.radianceAttached, currentAttached: this.currentAttached,
        corruptionAttached: this.corruptionAttached, corroded: this.corroded,
        currentReaction: this.currentReaction, pulsePhase: this.pulsePhase,
        levelStage: this.levelStage, gateState: this.gateState,
        supplyState: this.supplyState, bossHp: this.bossHp,
        bossPoise: this.bossPoise, bossPhase: this.bossPhase,
        bossMechanic: this.bossMechanic, bossCastMs: this.bossCastMs,
        debugHud: this.debugHud, perfLevel: this.perfLevel,
        vfxFlags: this.vfxFlags, bossHpRatio: this.bossHpRatio,
        bossCastRatio: this.bossCastRatio, objectiveLabel: this.objectiveLabel,
        resonanceSlots: this.resonanceSlots, showDebugHud: this.showDebugHud,
        bossCinematicProgress: this.bossCinematicProgress,
        bossShardCount: this.bossShardCount, bossSourceColor: this.bossSourceColor,
        bossRingBroken: this.bossRingBroken,
        inputEventCount: this.inputEventCount,
        playerChunkX: this.playerChunkX, playerChunkY: this.playerChunkY,
        playerLocalX: this.playerLocalX, playerLocalY: this.playerLocalY,
        activeChunkCount: this.activeChunkCount,
        cachedChunkCount: this.cachedChunkCount,
        streamingPendingCount: this.streamingPendingCount })
        .hitTestBehavior(HitTestMode.Transparent)

      ExplorationHud({ explorationStamina: this.explorationStamina,
        motionState: this.motionState,
        interactionAnchorId: this.interactionAnchorId,
        interactionUnlocked: this.interactionUnlocked,
        interactionLabel: this.interactionLabel,
        interactionKind: this.interactionKind,
        unlockedAnchorCount: this.unlockedAnchorCount,
        teleportFlashMs: this.teleportFlashMs,
        playerX: this.posX, playerY: this.posY,
        minimapAnchorX: this.minimapAnchorX,
        minimapAnchorY: this.minimapAnchorY,
        minimapAnchorUnlocked: this.minimapAnchorUnlocked,
        minimapItemX: this.minimapItemX,
        minimapItemY: this.minimapItemY,
        minimapItemKind: this.minimapItemKind,
        sideQuestProgress: this.sideQuestProgress,
        sideQuestRequired: this.sideQuestRequired,
        sprintActive: this.sprintActive,
        dailyCompletedCount: this.dailyCompletedCount,
        dailyQuestClaimed: this.dailyQuestClaimed,
        activeCharacterId: this.activeCharacterId,
        dayNightHour: this.dayNightHour,
        weatherId: this.weatherId,
        completedSideQuestCount: this.completedSideQuestCount,
        dungeonState: this.dungeonState,
        dungeonProgress: this.dungeonProgress,
        dungeonRequired: this.dungeonRequired,
        fateCount: this.fateCount,
        adventureRank: this.adventureRank,
        adventureExp: this.adventureExp,
        adventureExpRequired: this.adventureExpRequired,
        worldLevel: this.worldLevel,
        explorationPoiCount: this.explorationPoiCount,
        explorationPuzzleCount: this.explorationPuzzleCount,
        explorationRewardCount: this.explorationRewardCount,
        explorationTraversalMask: this.explorationTraversalMask,
        explorationCurrentTargetLabel: this.explorationCurrentTargetLabel,
        explorationCurrentTargetDistrict: this.explorationCurrentTargetDistrict,
        onOpenGacha: () => { this.gachaOpen = true; },
        onOpenMap: () => { this.mapOpen = true; },
        onOpenInventory: () => { this.inventoryOpen = true; },
        onOpenArtifact: () => { this.artifactOpen = true; } })

      DialogBox({ dialogActive: this.dialogActive,
        dialogSpeaker: this.dialogSpeaker,
        dialogText: this.dialogText,
        dialogLineIndex: this.dialogLineIndex,
        dialogLineCount: this.dialogLineCount })

      GachaPanel({ open: this.gachaOpen,
        fateCount: this.fateCount,
        goldCount: this.goldCount,
        expMaterialCount: this.expMaterialCount,
        ascensionMaterialCount: this.ascensionMaterialCount,
        gachaPity5: this.gachaPity5,
        gachaResultIds: this.gachaResultIds,
        gachaResultRarities: this.gachaResultRarities,
        gachaResultIsNew: this.gachaResultIsNew,
        rosterIds: this.rosterIds,
        rosterLevels: this.rosterLevels,
        rosterAscensions: this.rosterAscensions,
        rosterHp: this.rosterHp,
        rosterAtk: this.rosterAtk,
        rosterConstellations: this.rosterConstellations,
        weaponIds: this.weaponIds,
        weaponLevels: this.weaponLevels,
        weaponEquippedBy: this.weaponEquippedBy,
        weaponAscensions: this.weaponAscensions,
        weaponRefines: this.weaponRefines,
        weaponRefineStocks: this.weaponRefineStocks,
        oreLowCount: this.oreLowCount,
        oreMidCount: this.oreMidCount,
        oreHighCount: this.oreHighCount,
        activeCharacterId: this.activeCharacterId,
        gachaPoolKind: this.gachaPoolKind,
        onClose: () => { this.gachaOpen = false; } })

      InventoryPanel({ open: this.inventoryOpen,
        fateCount: this.fateCount,
        goldCount: this.goldCount,
        expMaterialCount: this.expMaterialCount,
        ascensionMaterialCount: this.ascensionMaterialCount,
        oreLowCount: this.oreLowCount,
        oreMidCount: this.oreMidCount,
        oreHighCount: this.oreHighCount,
        expSmallCount: this.expSmallCount,
        expMediumCount: this.expMediumCount,
        expLargeCount: this.expLargeCount,
        artifactInstanceIds: this.artifactInstanceIds,
        artifactDefIds: this.artifactDefIds,
        artifactRarities: this.artifactRarities,
        artifactLevels: this.artifactLevels,
        artifactEquippedBy: this.artifactEquippedBy,
        adventureRank: this.adventureRank,
        adventureExp: this.adventureExp,
        adventureExpRequired: this.adventureExpRequired,
        worldLevel: this.worldLevel,
        activeCharacterId: this.activeCharacterId,
        onClose: () => { this.inventoryOpen = false; } })

      ArtifactPanel({ open: this.artifactOpen,
        goldCount: this.goldCount,
        artifactInstanceIds: this.artifactInstanceIds,
        artifactDefIds: this.artifactDefIds,
        artifactRarities: this.artifactRarities,
        artifactLevels: this.artifactLevels,
        artifactEquippedBy: this.artifactEquippedBy,
        activeCharacterId: this.activeCharacterId,
        onClose: () => { this.artifactOpen = false; } })

      MapPanel({ open: this.mapOpen,
        minimapAnchorX: this.minimapAnchorX,
        minimapAnchorY: this.minimapAnchorY,
        minimapAnchorUnlocked: this.minimapAnchorUnlocked,
        onClose: () => { this.mapOpen = false; } })

      QuestRewardToast({ visible: this.rewardToastVisible,
        questTitle: this.rewardToastTitle,
        rewardLabel: this.rewardToastLabel })

      ExplorationToast({
        explorationFeedbackType: this.explorationFeedbackType,
        explorationFeedbackTitle: this.explorationFeedbackTitle,
        explorationFeedbackSubtitle: this.explorationFeedbackSubtitle,
        explorationFeedbackRemainingMs: this.explorationFeedbackRemainingMs })
        .hitTestBehavior(HitTestMode.Transparent)

      // 对话、召唤面板、地图、背包或圣遗物面板打开时隐藏技能按钮，避免遮挡弹窗按钮。
      if (!this.dialogActive && !this.gachaOpen && !this.mapOpen &&
          !this.inventoryOpen && !this.artifactOpen) {
        CombatControls({
          radianceCooldownMs: this.radianceCooldownMs,
          currentCooldownMs: this.currentCooldownMs,
          corruptionCooldownMs: this.corruptionCooldownMs,
          radianceCooldownTotalMs: this.radianceCooldownTotalMs,
          currentCooldownTotalMs: this.currentCooldownTotalMs,
          corruptionCooldownTotalMs: this.corruptionCooldownTotalMs,
          ultimateWindowMs: this.ultimateWindowMs })
      }

      HitFeedback({ hp: this.hp })
        .hitTestBehavior(HitTestMode.Transparent)

      ActionToast({ lastRejectReason: this.lastRejectReason,
        questAcceptMessage: this.questAcceptMessage })
        .hitTestBehavior(HitTestMode.Transparent)

      ComboCounter({ comboSegment: this.comboSegment,
        comboWindowMs: this.comboWindowMs })
        .hitTestBehavior(HitTestMode.Transparent)

      Tutorial({ moving: this.moving, comboSegment: this.comboSegment,
        invulnerable: this.invulnerable,
        radianceCooldownMs: this.radianceCooldownMs })
        .hitTestBehavior(HitTestMode.Transparent)

      GameStateOverlay({ encounterState: this.encounterState,
        encounterMode: this.encounterMode })

      TargetFrame({ targetId: this.targetId,
        targetArchetype: this.targetArchetype,
        targetHpRatio: this.targetHpRatio,
        targetPoise: this.targetPoise,
        targetPoiseBroken: this.targetPoiseBroken })
        .hitTestBehavior(HitTestMode.Transparent)

      PauseMenu({ encounterMode: this.encounterMode,
              qualityPreset: this.qualityPreset })
    }
    .width('100%')
    .height('100%')
    // 沉浸式全屏下内容延伸到状态栏/导航栏安全区，避免上下出现黑/白条带。
    .expandSafeArea([SafeAreaType.SYSTEM], [SafeAreaEdge.TOP, SafeAreaEdge.BOTTOM])
  }

  aboutToAppear() {
    // 新手引导完成状态跨会话持久化，需在组件绑定前初始化。
    PersistentStorage.persistProp<boolean>('tutorialDone', false);
    // 振动反馈开关跨会话持久化。
    PersistentStorage.persistProp<boolean>('hapticsEnabled', true);
    // 音效开关跨会话持久化，并把当前值下发给原生音频桥。
    PersistentStorage.persistProp<boolean>('audioEnabled', true);
    setAudioEnabled(AppStorage.get<boolean>('audioEnabled') !== false);
    this.pageActive = true;
    // 恢复探索与任务进度（首次启动无存档时静默降级）。
    try { loadProgress(this.progressSavePath()); } catch (e) {}
    const generation = ++this.modelLoadGeneration;
    this.loadModelAssets(generation);
    this.timer = setInterval(() => {
      try {
        this.snapshot = pullSnapshot();
        this.tick = this.snapshot.tick;
        this.hp = this.snapshot.hp;
        this.poise = this.snapshot.poise;
        this.posX = this.snapshot.x;
        this.posY = this.snapshot.y;
        this.fps = this.snapshot.fps;
        this.moving = this.snapshot.moving;
        this.moveX = this.snapshot.moveX;
        this.moveY = this.snapshot.moveY;
        this.cameraYaw = this.snapshot.cameraYaw;
        this.cameraPitch = this.snapshot.cameraPitch;
        this.targetDist = this.snapshot.targetDist;
        this.targetId = this.snapshot.targetId;
        this.targetArchetype = this.snapshot.targetArchetype;
        this.targetHpRatio = this.snapshot.targetHpRatio;
        this.bossPhase = this.snapshot.bossPhase;
        this.encounterMode = this.snapshot.encounterMode;
        this.encounterState = this.snapshot.encounterState;
        this.rendererReady = this.snapshot.rendererReady;
        this.environmentReady = this.snapshot.environmentReady;
        this.environmentDrawCalls = this.snapshot.environmentDrawCalls;
        this.environmentTriangles = this.snapshot.environmentTriangles;
        this.stamina = this.snapshot.stamina;
        this.comboSegment = this.snapshot.comboSegment;
        this.invulnerable = this.snapshot.invulnerable;
        this.insightMs = this.snapshot.insightMs;
        this.resonance = this.snapshot.resonance;
        this.targetHp = this.snapshot.targetHp;
        this.targetPoise = this.snapshot.targetPoise;
        this.pulseHitRemainingMs = this.snapshot.pulseHitRemainingMs;
        this.lastRejectReason = this.snapshot.lastRejectReason;
        this.currentAction = this.snapshot.currentAction;
        this.comboWindowMs = this.snapshot.comboWindowMs;
        this.radianceCooldownMs = this.snapshot.radianceCooldownMs;
        this.currentCooldownMs = this.snapshot.currentCooldownMs;
        this.corruptionCooldownMs = this.snapshot.corruptionCooldownMs;
        this.radianceCooldownTotalMs = this.snapshot.radianceCooldownTotalMs;
        this.currentCooldownTotalMs = this.snapshot.currentCooldownTotalMs;
        this.corruptionCooldownTotalMs = this.snapshot.corruptionCooldownTotalMs;
        this.ultimateWindowMs = this.snapshot.ultimateWindowMs;
        this.targetPoiseBroken = this.snapshot.targetPoiseBroken;
        this.radianceAttached = this.snapshot.radianceAttached;
        this.currentAttached = this.snapshot.currentAttached;
        this.corruptionAttached = this.snapshot.corruptionAttached;
        this.corroded = this.snapshot.corroded;
        this.currentReaction = this.snapshot.currentReaction;
        this.pulsePhase = this.snapshot.pulsePhase;
        this.levelStage = this.snapshot.levelStage;
        this.gateState = this.snapshot.gateState;
        this.supplyState = this.snapshot.supplyState;
        this.bossHp = this.snapshot.bossHp;
        this.bossPoise = this.snapshot.bossPoise;
        this.bossMechanic = this.snapshot.bossMechanic;
        this.bossCastMs = this.snapshot.bossCastMs;
        this.perfLevel = this.snapshot.perfLevel;
        this.vfxFlags = this.snapshot.vfxFlags;
        this.cameraShakeX = this.snapshot.cameraShakeX;
        this.cameraShakeY = this.snapshot.cameraShakeY;
        this.bossHpRatio = this.snapshot.bossHpRatio;
        this.bossCastRatio = this.snapshot.bossCastRatio;
        this.debugHud = this.snapshot.debugHud;
        this.objectiveLabel = this.snapshot.objectiveLabel;
        this.resonanceSlots = this.snapshot.resonanceSlots;
        this.showDebugHud = this.snapshot.showDebugHud;
        this.bossCinematicProgress = this.snapshot.bossCinematicProgress;
        this.bossShardCount = this.snapshot.bossShardCount;
        this.bossSourceColor = this.snapshot.bossSourceColor;
        this.bossRingBroken = this.snapshot.bossRingBroken;
        this.inputEventCount = this.snapshot.inputEventCount;
        this.explorationStamina = this.snapshot.explorationStamina;
        this.motionState = this.snapshot.motionState;
        this.playerHeight = this.snapshot.playerHeight;
        this.playerChunkX = this.snapshot.playerChunkX;
        this.playerChunkY = this.snapshot.playerChunkY;
        this.playerLocalX = this.snapshot.playerLocalX;
        this.playerLocalY = this.snapshot.playerLocalY;
        this.activeChunkCount = this.snapshot.activeChunkCount;
        this.cachedChunkCount = this.snapshot.cachedChunkCount;
        this.streamingPendingCount = this.snapshot.streamingPendingCount;
        this.interactionAnchorId = this.snapshot.interactionAnchorId;
        this.interactionUnlocked = this.snapshot.interactionUnlocked;
        this.interactionLabel = this.snapshot.interactionLabel;
        this.unlockedAnchorCount = this.snapshot.unlockedAnchorCount;
        this.cameraExploration = this.snapshot.cameraExploration;
        this.teleportFlashMs = this.snapshot.teleportFlashMs;
        this.minimapAnchorX = this.snapshot.minimapAnchorX;
        this.minimapAnchorY = this.snapshot.minimapAnchorY;
        this.minimapAnchorUnlocked = this.snapshot.minimapAnchorUnlocked;
        this.questId = this.snapshot.questId;
        this.questStatus = this.snapshot.questStatus;
        this.questTitle = this.snapshot.questTitle;
        this.questObjectiveLabel = this.snapshot.questObjectiveLabel;
        this.questObjectiveProgress = this.snapshot.questObjectiveProgress;
        this.questObjectiveRequired = this.snapshot.questObjectiveRequired;
        this.completedQuestCount = this.snapshot.completedQuestCount;
        this.dialogActive = this.snapshot.dialogActive;
        this.dialogSpeaker = this.snapshot.dialogSpeaker;
        this.dialogText = this.snapshot.dialogText;
        this.dialogLineIndex = this.snapshot.dialogLineIndex;
        this.dialogLineCount = this.snapshot.dialogLineCount;
        this.interactionKind = this.snapshot.interactionKind;
        this.explorationPoiCount = this.snapshot.explorationPoiCount;
        this.explorationPuzzleCount = this.snapshot.explorationPuzzleCount;
        this.explorationRewardCount = this.snapshot.explorationRewardCount;
        this.explorationGateCount = this.snapshot.explorationGateCount;
        this.explorationTraversalMask = this.snapshot.explorationTraversalMask;
        this.explorationCurrentPoiId = this.snapshot.explorationCurrentPoiId;
        this.explorationCurrentTargetLabel =
          this.snapshot.explorationCurrentTargetLabel;
        this.explorationCurrentTargetDistrict =
          this.snapshot.explorationCurrentTargetDistrict;
        this.explorationFeedbackType = this.snapshot.explorationFeedbackType;
        this.explorationFeedbackId = this.snapshot.explorationFeedbackId;
        this.explorationFeedbackTitle =
          this.snapshot.explorationFeedbackTitle;
        this.explorationFeedbackSubtitle =
          this.snapshot.explorationFeedbackSubtitle;
        this.explorationFeedbackRemainingMs =
          this.snapshot.explorationFeedbackRemainingMs;
        if (this.explorationFeedbackId >= 0 &&
            this.explorationFeedbackId !== this.prevExplorationFeedbackId) {
          if (this.explorationFeedbackType === 3) {
            Haptics.heavy();
          } else {
            Haptics.light();
          }
        }
        this.prevExplorationFeedbackId = this.explorationFeedbackId;
        this.fateCount = this.snapshot.fateCount;
        this.goldCount = this.snapshot.goldCount;
        this.expMaterialCount = this.snapshot.expMaterialCount;
        this.ascensionMaterialCount = this.snapshot.ascensionMaterialCount;
        this.gachaPity5 = this.snapshot.gachaPity5;
        this.gachaResultIds = this.snapshot.gachaResultIds;
        this.gachaResultRarities = this.snapshot.gachaResultRarities;
        this.gachaResultIsNew = this.snapshot.gachaResultIsNew;
        this.rosterIds = this.snapshot.rosterIds;
        this.rosterLevels = this.snapshot.rosterLevels;
        this.rosterAscensions = this.snapshot.rosterAscensions;
        this.activeCharacterId = this.snapshot.activeCharacterId;
        this.dayNightHour = this.snapshot.dayNightHour;
        this.qualityPreset = this.snapshot.qualityPreset;
        this.weatherId = this.snapshot.weatherId;
        this.musicRegionId = this.snapshot.musicRegionId;
        this.completedSideQuestCount = this.snapshot.completedSideQuestCount;
        this.dungeonState = this.snapshot.dungeonState;
        this.dungeonProgress = this.snapshot.dungeonProgress;
        this.dungeonRequired = this.snapshot.dungeonRequired;
        this.minimapItemX = this.snapshot.minimapItemX;
        this.minimapItemY = this.snapshot.minimapItemY;
        this.minimapItemKind = this.snapshot.minimapItemKind;
        this.sideQuestProgress = this.snapshot.sideQuestProgress;
        this.sideQuestRequired = this.snapshot.sideQuestRequired;
        this.rosterHp = this.snapshot.rosterHp;
        this.rosterAtk = this.snapshot.rosterAtk;
        this.rosterConstellations = this.snapshot.rosterConstellations;
        this.weaponIds = this.snapshot.weaponIds;
        this.weaponLevels = this.snapshot.weaponLevels;
        this.weaponEquippedBy = this.snapshot.weaponEquippedBy;
        this.weaponAscensions = this.snapshot.weaponAscensions;
        this.weaponRefines = this.snapshot.weaponRefines;
        this.weaponRefineStocks = this.snapshot.weaponRefineStocks;
        this.weaponExps = this.snapshot.weaponExps;
        this.artifactInstanceIds = this.snapshot.artifactInstanceIds;
        this.artifactDefIds = this.snapshot.artifactDefIds;
        this.artifactRarities = this.snapshot.artifactRarities;
        this.artifactLevels = this.snapshot.artifactLevels;
        this.artifactEquippedBy = this.snapshot.artifactEquippedBy;
        this.artifactSeeds = this.snapshot.artifactSeeds;
        this.adventureRank = this.snapshot.adventureRank;
        this.adventureExp = this.snapshot.adventureExp;
        this.adventureExpRequired = this.snapshot.adventureExpRequired;
        this.worldLevel = this.snapshot.worldLevel;
        this.oreLowCount = this.snapshot.oreLowCount;
        this.oreMidCount = this.snapshot.oreMidCount;
        this.oreHighCount = this.snapshot.oreHighCount;
        this.expSmallCount = this.snapshot.expSmallCount;
        this.expMediumCount = this.snapshot.expMediumCount;
        this.expLargeCount = this.snapshot.expLargeCount;
        this.sprintActive = this.snapshot.sprintActive;
        this.dailyCompletedCount = this.snapshot.dailyCompletedCount;
        this.dailyQuestClaimed = this.snapshot.dailyQuestClaimed;
        this.gachaPoolKind = this.snapshot.gachaPoolKind;
        // NPC 任务接取上升沿（Phase 4）：边沿检测避免重复弹层。
        if (this.snapshot.npcOfferQuestId >= 0 &&
          this.snapshot.npcOfferQuestId !== this.prevNpcOfferQuestId) {
          this.questAcceptMessage = '已接取任务：' + this.snapshot.npcOfferQuestTitle;
        }
        this.prevNpcOfferQuestId = this.snapshot.npcOfferQuestId;
        // 任务完成上升沿即时落盘，保证杀进程重启进度不丢。
        if (this.completedQuestCount > this.prevCompletedQuestCount) {
          try { saveProgress(this.progressSavePath()); } catch (e) {}
          // 奖励结算浮层（内容反馈闭环）：与 native 奖励表对应。
          this.rewardToastTitle = this.questTitle;
          this.rewardToastLabel = this.rewardLabelFor(this.completedQuestCount);
          this.rewardToastVisible = true;
          if (this.rewardToastTimer >= 0) { clearTimeout(this.rewardToastTimer); }
          this.rewardToastTimer = setTimeout(() => {
            this.rewardToastVisible = false;
            this.rewardToastTimer = -1;
          }, 3000);
        }
        this.prevCompletedQuestCount = this.completedQuestCount;
        this.detectCombatEvents();
      } catch (e) {
      }
    }, 100);
  }

  aboutToDisappear() {
    this.pageActive = false;
    ++this.modelLoadGeneration;
    if (this.timer !== -1) {
      clearInterval(this.timer);
      this.timer = -1;
    }
    try { saveProgress(this.progressSavePath()); } catch (e) {}
    nativeStop();
  }
}

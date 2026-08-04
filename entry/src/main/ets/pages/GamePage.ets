import { nativeSetEnvironmentAssets, nativeSetModelAssets, nativeStartIfForeground,
  nativeStop, pullSnapshot, Snapshot } from '../napi/Bridge';
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
    inputEventCount: 0
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
  private timer: number = -1;
  private pageActive: boolean = false;
  private modelLoadGeneration: number = 0;
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
      } catch (error) {
        if (this.isActiveModelLoad(generation)) {
          console.error('[Ethelan] Failed to load model assets; using fallback meshes: ' +
            JSON.stringify(error));
        }
      }
      try {
        const environmentAssets: Uint8Array[] = await Promise.all([
          resourceManager.getRawFileContent('environment/outer_ring.glb'),
          resourceManager.getRawFileContent('environment/center_rift.glb'),
          resourceManager.getRawFileContent('environment/backdrop.glb'),
          resourceManager.getRawFileContent('environment/decoration.glb')
        ]);
        if (this.isActiveModelLoad(generation) &&
          !nativeSetEnvironmentAssets(environmentAssets[0].buffer as ArrayBuffer,
            environmentAssets[1].buffer as ArrayBuffer,
            environmentAssets[2].buffer as ArrayBuffer,
            environmentAssets[3].buffer as ArrayBuffer)) {
          console.error('[Ethelan] Native rejected environment assets; using procedural fallback');
        }
      } catch (error) {
        if (this.isActiveModelLoad(generation)) {
          console.error('[Ethelan] Failed to load environment assets; using procedural fallback: ' +
            JSON.stringify(error));
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
        inputEventCount: this.inputEventCount })
        .hitTestBehavior(HitTestMode.Transparent)

      CombatControls({
        radianceCooldownMs: this.radianceCooldownMs,
        currentCooldownMs: this.currentCooldownMs,
        corruptionCooldownMs: this.corruptionCooldownMs,
        radianceCooldownTotalMs: this.radianceCooldownTotalMs,
        currentCooldownTotalMs: this.currentCooldownTotalMs,
        corruptionCooldownTotalMs: this.corruptionCooldownTotalMs,
        ultimateWindowMs: this.ultimateWindowMs })

      HitFeedback({ hp: this.hp })
        .hitTestBehavior(HitTestMode.Transparent)

      ActionToast({ lastRejectReason: this.lastRejectReason })
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
        targetHpRatio: this.targetHpRatio })
        .hitTestBehavior(HitTestMode.Transparent)

      PauseMenu({ encounterMode: this.encounterMode })
    }
    .width('100%')
    .height('100%')
  }

  aboutToAppear() {
    // 新手引导完成状态跨会话持久化，需在组件绑定前初始化。
    PersistentStorage.persistProp<boolean>('tutorialDone', false);
    // 振动反馈开关跨会话持久化。
    PersistentStorage.persistProp<boolean>('hapticsEnabled', true);
    this.pageActive = true;
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
    nativeStop();
  }
}

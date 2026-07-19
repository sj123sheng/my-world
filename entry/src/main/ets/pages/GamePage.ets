import { nativeStart, nativeStop, pullSnapshot, Snapshot } from '../napi/Bridge';
import { Hud } from '../ui/Hud';
import { CombatControls } from '../ui/CombatControls';

@Entry
@Component
struct GamePage {
  private snapshot: Snapshot = {
    tick: 0, hp: 100, poise: 100, x: 0.5, y: 0.5, fps: 0, moving: false,
    moveX: 0, moveY: 0, cameraYaw: 0, cameraPitch: 0,
    targetDist: 0, targetId: 0, bossPhase: 0, encounterMode: 0,
    encounterState: 0, rendererReady: true,
    stamina: 100, comboSegment: 0, invulnerable: false, insightMs: 0,
    resonance: 0, targetHp: 300, targetPoise: 100, pulseHitRemainingMs: 0,
    lastRejectReason: 0, currentAction: 0, comboWindowMs: 0,
    radianceCooldownMs: 0, currentCooldownMs: 0, corruptionCooldownMs: 0,
    ultimateWindowMs: 0, targetPoiseBroken: false, radianceAttached: false,
    currentAttached: false, corruptionAttached: false, corroded: false,
    currentReaction: -1, pulsePhase: 0,
    levelStage: 0, gateState: 0, supplyState: 0,
    bossHp: 1000, bossPoise: 300, bossMechanic: 0, bossCastMs: 0
    , perfLevel: 0, vfxFlags: 0, cameraShakeX: 0, cameraShakeY: 0,
    bossHpRatio: 0, bossCastRatio: 0, debugHud: false
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
  @State bossPhase: number = 0;
  @State encounterMode: number = 0;
  @State encounterState: number = 0;
  @State rendererReady: boolean = true;
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
  private timer: number = -1;

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

      Hud({ hp: this.hp, poise: this.poise, x: this.posX, y: this.posY,
        fps: this.fps, moving: this.moving, targetDist: this.targetDist,
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
        bossCastRatio: this.bossCastRatio })

      CombatControls()
        .align(Alignment.BottomEnd)
    }
    .width('100%')
    .height('100%')
  }

  aboutToAppear() {
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
        this.bossPhase = this.snapshot.bossPhase;
        this.encounterMode = this.snapshot.encounterMode;
        this.encounterState = this.snapshot.encounterState;
        this.rendererReady = this.snapshot.rendererReady;
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
      } catch (e) {
      }
    }, 100);
  }

  aboutToDisappear() {
    if (this.timer !== -1) {
      clearInterval(this.timer);
      this.timer = -1;
    }
    nativeStop();
  }
}

import { nativeStart, nativeStop, pullSnapshot, Snapshot } from '../napi/Bridge';
import { Hud } from '../ui/Hud';
import { CombatControls } from '../ui/CombatControls';

@Entry
@Component
struct GamePage {
  private snapshot: Snapshot = {
    tick: 0, hp: 100, poise: 100, x: 0.5, y: 0.5, fps: 0, moving: false,
    moveX: 0, moveY: 0, cameraYaw: 0, cameraPitch: 0,
    targetDist: 0, targetId: 0, bossPhase: 0, rendererReady: true,
    stamina: 100, comboSegment: 0, invulnerable: false, insightMs: 0,
    resonance: 0, targetHp: 300, targetPoise: 100, pulseWarningMs: 0,
    lastRejectReason: 0
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
  @State rendererReady: boolean = true;
  @State stamina: number = 100;
  @State comboSegment: number = 0;
  @State invulnerable: boolean = false;
  @State insightMs: number = 0;
  @State resonance: number = 0;
  @State targetHp: number = 300;
  @State targetPoise: number = 100;
  @State pulseWarningMs: number = 0;
  @State lastRejectReason: number = 0;
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
        targetPoise: this.targetPoise, pulseWarningMs: this.pulseWarningMs,
        lastRejectReason: this.lastRejectReason })

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
        this.rendererReady = this.snapshot.rendererReady;
        this.stamina = this.snapshot.stamina;
        this.comboSegment = this.snapshot.comboSegment;
        this.invulnerable = this.snapshot.invulnerable;
        this.insightMs = this.snapshot.insightMs;
        this.resonance = this.snapshot.resonance;
        this.targetHp = this.snapshot.targetHp;
        this.targetPoise = this.snapshot.targetPoise;
        this.pulseWarningMs = this.snapshot.pulseWarningMs;
        this.lastRejectReason = this.snapshot.lastRejectReason;
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

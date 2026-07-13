import { nativeStart, nativeStop, pushInput, pullSnapshot, Snapshot } from '../napi/Bridge';
import { Hud } from '../ui/Hud';

@Entry
@Component
struct GamePage {
  private snapshot: Snapshot = {
    hp: 100, poise: 100, x: 0.5, y: 0.5, fps: 0, moving: false, targetDist: 0, rendererReady: true
  };
  @State hp: number = 100;
  @State poise: number = 100;
  @State posX: number = 0.5;
  @State posY: number = 0.5;
  @State fps: number = 0;
  @State moving: boolean = false;
  @State targetDist: number = 0;
  @State rendererReady: boolean = true;
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
        .onTouch((event: TouchEvent) => {
          pushInput({ type: event.type as number, x: event.touches[0].windowX, y: event.touches[0].windowY });
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

      Hud({ hp: this.hp, poise: this.poise, x: this.posX, y: this.posY, fps: this.fps, moving: this.moving, targetDist: this.targetDist })
    }
    .width('100%')
    .height('100%')
  }

  aboutToAppear() {
    this.timer = setInterval(() => {
      try {
        this.snapshot = pullSnapshot();
        this.hp = this.snapshot.hp;
        this.poise = this.snapshot.poise;
        this.posX = this.snapshot.x;
        this.posY = this.snapshot.y;
        this.fps = this.snapshot.fps;
        this.moving = this.snapshot.moving;
        this.targetDist = this.snapshot.targetDist;
        this.rendererReady = this.snapshot.rendererReady;
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

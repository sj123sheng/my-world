import { nativeStart, nativeStop, pullSnapshot } from '../napi/Bridge';
import { Hud } from '../ui/Hud';

@Entry
@Component
struct GamePage {
  private surfaceId: string = '';
  @State hp: number = 100;
  @State poise: number = 100;

  build() {
    Stack() {
      XComponent({
        id: 'gameSurface',
        type: 'surface',
        libraryname: 'native_game'
      })
        .onLoad((surfaceId: string) => {
          this.surfaceId = surfaceId;
          nativeStart(surfaceId);
        })
        .width('100%')
        .height('100%')

      Hud({ hp: this.hp, poise: this.poise })
    }
  }

  aboutToAppear() {
    setInterval(() => {
      try {
        const snapshot = pullSnapshot();
        this.hp = snapshot.hp;
        this.poise = snapshot.poise;
      } catch (e) {
        // N-API 调用在非真机环境可能抛异常
      }
    }, 100);
  }

  aboutToDisappear() {
    nativeStop();
  }
}

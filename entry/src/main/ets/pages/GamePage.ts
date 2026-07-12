import { nativeStart, nativeStop } from '../napi/Bridge';

@Entry
@Component
struct GamePage {
  private surfaceId: string = '';

  build() {
    Column() {
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
    }
  }

  aboutToDisappear() {
    nativeStop();
  }
}

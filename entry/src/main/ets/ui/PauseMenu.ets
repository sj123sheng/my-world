import { setPaused, startEncounter } from '../napi/Bridge';

// 暂停菜单：右上角暂停入口，冻结原生帧循环；
// 面板提供继续、重开本战与振动反馈开关。
@Component
export struct PauseMenu {
  @Prop encounterMode: number = 0;
  @StorageLink('hapticsEnabled') hapticsEnabled: boolean = true;
  @State menuOpen: boolean = false;

  private pause(): void {
    setPaused(true);
    this.menuOpen = true;
  }

  private resume(): void {
    setPaused(false);
    this.menuOpen = false;
  }

  private restart(): void {
    setPaused(false);
    this.menuOpen = false;
    startEncounter(this.encounterMode);
  }

  build() {
    Stack() {
      // 暂停入口按钮。
      Button('II')
        .width(40)
        .height(40)
        .buttonStyle(ButtonStyleType.Circle)
        .backgroundColor('rgba(20, 26, 38, 0.55)')
        .border({ width: 1, color: 'rgba(199, 208, 206, 0.35)' })
        .fontColor('#C7D0CE')
        .fontSize(13)
        .fontWeight(FontWeight.Bold)
        .stateEffect(true)
        .position({ x: '100%', y: 14 })
        .markAnchor({ x: 106, y: 0 })
        .onClick(() => {
          this.pause();
        })

      // 暂停面板。
      if (this.menuOpen) {
        Stack() {
          // 半透明遮罩，阻断下层交互。
          Column()
            .width('100%')
            .height('100%')
            .backgroundColor('rgba(6, 9, 14, 0.6)')

          Column({ space: 14 }) {
            Text('已暂停')
              .fontSize(20)
              .fontWeight(FontWeight.Bold)
              .fontColor('#E5ECEA')
              .letterSpacing(4)
              .margin({ bottom: 6 })

            Button('继续')
              .width(168)
              .height(42)
              .backgroundColor('#2F4858')
              .border({ width: 1, color: '#5B8FA8' })
              .fontColor('#D6E2DF')
              .fontSize(15)
              .stateEffect(true)
              .onClick(() => {
                this.resume();
              })

            Button('重开本战')
              .width(168)
              .height(42)
              .backgroundColor('#274043')
              .border({ width: 1, color: '#3E6B62' })
              .fontColor('#9FD8CB')
              .fontSize(15)
              .stateEffect(true)
              .onClick(() => {
                this.restart();
              })

            Row({ space: 10 }) {
              Text('振动反馈')
                .fontSize(14)
                .fontColor('#C7D0CE')
              Toggle({ type: ToggleType.Switch, isOn: this.hapticsEnabled })
                .selectedColor('#43CDB5')
                .onChange((isOn: boolean) => {
                  this.hapticsEnabled = isOn;
                })
            }
            .margin({ top: 4 })
          }
          .padding({ left: 28, right: 28, top: 26, bottom: 26 })
          .backgroundColor('#E611181D')
          .borderRadius(20)
          .border({ width: 1, color: 'rgba(199, 208, 206, 0.2)' })
        }
        .width('100%')
        .height('100%')
      }
    }
    .width('100%')
    .height('100%')
    .hitTestBehavior(this.menuOpen ? HitTestMode.Default : HitTestMode.Transparent)
  }
}

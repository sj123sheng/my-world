import { setAudioEnabled, setPaused, setQualityPreset, startEncounter } from '../napi/Bridge';

// 暂停菜单：右上角暂停入口，冻结原生帧循环；
// 面板提供继续、重开本战与振动/音效开关。
// 增强：平滑入场/退场过渡，交错按钮弹入，遮罩渐显。
@Component
export struct PauseMenu {
  @Prop encounterMode: number = 0;
  @Prop qualityPreset: number = 0;
  @StorageLink('hapticsEnabled') hapticsEnabled: boolean = true;
  @StorageLink('audioEnabled') audioEnabled: boolean = true;
  @State menuOpen: boolean = false;
  @State panelVisible: boolean = false;

  private pause(): void {
    setPaused(true);
    this.menuOpen = true;
    // 下一帧触发入场动画。
    setTimeout(() => {
      this.panelVisible = true;
    }, 30);
  }

  private resume(): void {
    this.panelVisible = false;
    // 等退场动画结束后关闭面板。
    setTimeout(() => {
      this.menuOpen = false;
      setPaused(false);
    }, 250);
  }

  private restart(): void {
    this.panelVisible = false;
    setTimeout(() => {
      this.menuOpen = false;
      setPaused(false);
      startEncounter(this.encounterMode);
    }, 250);
  }

  build() {
    Stack() {
      // 暂停入口按钮：面板打开时淡出。
      Button('II')
        .width(40)
        .height(40)
        .type(ButtonType.Circle)
        .backgroundColor('rgba(20, 26, 38, 0.55)')
        .border({ width: 1, color: 'rgba(199, 208, 206, 0.35)' })
        .fontColor('#C7D0CE')
        .fontSize(13)
        .fontWeight(FontWeight.Bold)
        .stateEffect(true)
        .opacity(this.menuOpen ? 0 : 1)
        .animation({ duration: 200, curve: Curve.EaseOut })
        .position({ x: '100%', y: 14 })
        .markAnchor({ x: 106, y: 0 })
        .onClick(() => {
          this.pause();
        })

      // 暂停面板。
      if (this.menuOpen) {
        Stack() {
          // 半透明遮罩，渐显入场。
          Column()
            .width('100%')
            .height('100%')
            .backgroundColor('rgba(6, 9, 14, 0.6)')
            .opacity(this.panelVisible ? 1 : 0)
            .animation({ duration: 250, curve: Curve.EaseOut })

          Column({ space: 14 }) {
            Text('已暂停')
              .fontSize(20)
              .fontWeight(FontWeight.Bold)
              .fontColor('#E5ECEA')
              .letterSpacing(4)
              .margin({ bottom: 6 })
              .opacity(this.panelVisible ? 1 : 0)
              .translate({ y: this.panelVisible ? 0 : -10 })
              .animation({ duration: 300, delay: 50, curve: Curve.EaseOut })

            Button('继续')
              .width(168)
              .height(42)
              .backgroundColor('#2F4858')
              .border({ width: 1, color: '#5B8FA8' })
              .fontColor('#D6E2DF')
              .fontSize(15)
              .stateEffect(true)
              .opacity(this.panelVisible ? 1 : 0)
              .scale({ x: this.panelVisible ? 1.0 : 0.92, y: this.panelVisible ? 1.0 : 0.92 })
              .animation({ duration: 300, delay: 100, curve: Curve.EaseOut })
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
              .opacity(this.panelVisible ? 1 : 0)
              .scale({ x: this.panelVisible ? 1.0 : 0.92, y: this.panelVisible ? 1.0 : 0.92 })
              .animation({ duration: 300, delay: 150, curve: Curve.EaseOut })
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
            .opacity(this.panelVisible ? 1 : 0)
            .animation({ duration: 300, delay: 200, curve: Curve.EaseOut })

            Row({ space: 10 }) {
              Text('低画质')
                .fontSize(14)
                .fontColor('#C7D0CE')
              Toggle({ type: ToggleType.Switch, isOn: this.qualityPreset === 1 })
                .selectedColor('#43CDB5')
                .onChange((isOn: boolean) => {
                  setQualityPreset(isOn ? 1 : 0);
                })
            }
            .margin({ top: 4 })
            .opacity(this.panelVisible ? 1 : 0)
            .animation({ duration: 300, delay: 250, curve: Curve.EaseOut })

            Row({ space: 10 }) {
              Text('音效')
                .fontSize(14)
                .fontColor('#C7D0CE')
              Toggle({ type: ToggleType.Switch, isOn: this.audioEnabled })
                .selectedColor('#43CDB5')
                .onChange((isOn: boolean) => {
                  this.audioEnabled = isOn;
                  setAudioEnabled(isOn);
                })
            }
            .margin({ top: 4 })
            .opacity(this.panelVisible ? 1 : 0)
            .animation({ duration: 300, delay: 250, curve: Curve.EaseOut })
          }
          .padding({ left: 28, right: 28, top: 26, bottom: 26 })
          .backgroundColor('#E611181D')
          .borderRadius(20)
          .border({ width: 1, color: 'rgba(199, 208, 206, 0.2)' })
          .opacity(this.panelVisible ? 1 : 0)
          .scale({ x: this.panelVisible ? 1.0 : 0.95, y: this.panelVisible ? 1.0 : 0.95 })
          .animation({ duration: 250, curve: Curve.EaseOut })
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

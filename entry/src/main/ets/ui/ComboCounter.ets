// 连击计数浮层：连击段数 >= 2 且在连击窗口内时显示，
// 每次段数增加触发放大回弹动画，窗口关闭后自动隐藏。
@Component
export struct ComboCounter {
  @Prop @Watch('onComboChanged') comboSegment: number = 0;
  @Prop comboWindowMs: number = 0;
  @State settled: boolean = true;

  onComboChanged(): void {
    if (this.comboSegment >= 2) {
      // 先跳到放大态，再回弹到常尺寸，形成打击节奏感。
      this.settled = false;
      animateTo({ duration: 200, curve: Curve.EaseOut }, () => {
        this.settled = true;
      });
    }
  }

  build() {
    Stack({ alignContent: Alignment.TopEnd }) {
      if (this.comboSegment >= 2 && this.comboWindowMs > 0) {
        Column({ space: 2 }) {
          Text(`x${this.comboSegment}`)
            .fontSize(36)
            .fontWeight(FontWeight.Bold)
            .fontColor('#F2D9A8')
            .textShadow({ radius: 10, color: '#B0000000', offsetX: 0, offsetY: 2 })
            .scale({
              x: this.settled ? 1.0 : 1.4,
              y: this.settled ? 1.0 : 1.4
            })
          Text('连击')
            .fontSize(12)
            .letterSpacing(4)
            .fontColor('rgba(229, 236, 234, 0.8)')
            .textShadow({ radius: 6, color: '#B0000000', offsetX: 0, offsetY: 1 })
        }
        .alignItems(HorizontalAlign.Center)
        .margin({ right: 40, top: 130 })
      }
    }
    .width('100%')
    .height('100%')
    .hitTestBehavior(HitTestMode.Transparent)
  }
}

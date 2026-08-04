// 锁定目标焦点框：软瞄准锁定普通敌人时，屏幕上方显示其名称、血条与韧性条，
// 与敌人头顶血条、锁定环共同构成完整的目标反馈。首领已有专属血条，
// archetype 为 -1 时不显示。破韧时触发金色闪光与标签提示。
@Component
export struct TargetFrame {
  @Prop targetId: number = 0;
  @Prop targetArchetype: number = -1;
  @Prop targetHpRatio: number = 0;
  @Prop targetPoise: number = 100;
  @Prop @Watch('onPoiseBroken') targetPoiseBroken: boolean = false;
  @State poiseBreakFlash: number = 0;
  @State poiseBreakShow: boolean = false;
  private prevPoiseBroken: boolean = false;
  private poiseBreakTimer: number = -1;

  aboutToDisappear(): void {
    if (this.poiseBreakTimer !== -1) {
      clearTimeout(this.poiseBreakTimer);
      this.poiseBreakTimer = -1;
    }
  }

  onPoiseBroken(): void {
    if (this.targetPoiseBroken && !this.prevPoiseBroken) {
      this.poiseBreakFlash = 1;
      animateTo({ duration: 400, curve: Curve.EaseOut }, () => {
        this.poiseBreakFlash = 0;
      });
      this.poiseBreakShow = true;
      if (this.poiseBreakTimer !== -1) {
        clearTimeout(this.poiseBreakTimer);
      }
      this.poiseBreakTimer = setTimeout((): void => {
        this.poiseBreakShow = false;
        this.poiseBreakTimer = -1;
      }, 1200);
    }
    this.prevPoiseBroken = this.targetPoiseBroken;
  }

  private archetypeName(): string {
    switch (this.targetArchetype) {
      case 0: return '裂隙之爪';
      case 1: return '辉光祭司';
      case 2: return '蚀壳守卫';
      default: return '';
    }
  }

  private hpColor(): string {
    if (this.targetHpRatio > 0.5) return '#4FD4BB';
    if (this.targetHpRatio > 0.25) return '#D9A145';
    return '#E06A5E';
  }

  private hpGlow(): string {
    if (this.targetHpRatio > 0.5) return '#304FD4BB';
    if (this.targetHpRatio > 0.25) return '#30D9A145';
    return '#30E06A5E';
  }

  private poiseRatio(): number {
    return Math.min(100, Math.max(0, this.targetPoise));
  }

  build() {
    Stack({ alignContent: Alignment.Top }) {
      if (this.targetId > 0 && this.targetArchetype >= 0) {
        Column({ space: 4 }) {
          Text(this.archetypeName())
            .fontSize(13)
            .fontColor('#E5ECEA')
            .fontWeight(FontWeight.Medium)
            .letterSpacing(2)
            .textShadow({ radius: 8, color: '#B0000000', offsetX: 0, offsetY: 1 })

          // HP 条。
          Progress({ value: this.targetHpRatio * 100, total: 100,
            type: ProgressType.Linear })
            .width(180)
            .height(5)
            .color(this.hpColor())
            .backgroundColor('rgba(20, 26, 38, 0.6)')
            .animation({ duration: 120, curve: Curve.EaseOut })

          // 韧性条 + 破韧闪光叠加。
          Stack() {
            Progress({ value: this.poiseRatio(), total: 100,
              type: ProgressType.Linear })
              .width(140)
              .height(3)
              .color('#D9A145')
              .backgroundColor('rgba(20, 26, 38, 0.4)')
              .animation({ duration: 120, curve: Curve.EaseOut })
            // 破韧金色闪光。
            Stack()
              .width(140).height(3)
              .backgroundColor('#F2D9A8')
              .opacity(this.poiseBreakFlash * 0.8)
              .borderRadius(2)
          }

          // 破韧标签。
          if (this.poiseBreakShow) {
            Text('破韧')
              .fontSize(11)
              .fontWeight(FontWeight.Bold)
              .fontColor('#F2D9A8')
              .letterSpacing(3)
              .textShadow({ radius: 8, color: '#60D9A145', offsetX: 0, offsetY: 0 })
              .transition({
                type: TransitionType.All,
                opacity: 0.0,
                scale: { x: 0.8, y: 0.8 }
              })
          }
        }
        .alignItems(HorizontalAlign.Center)
        .padding({ left: 18, right: 18, top: 8, bottom: 10 })
        .backgroundColor('#8A11181D')
        .borderRadius(14)
        .border({ width: 1, color: 'rgba(199, 208, 206, 0.25)' })
        .shadow({ radius: 8, color: this.hpGlow(), offsetX: 0, offsetY: 2 })
        .margin({ top: 44 })
        .transition({
          type: TransitionType.All,
          opacity: 0.0,
          scale: { x: 0.9, y: 0.9 }
        })
      }
    }
    .width('100%')
    .height('100%')
    .hitTestBehavior(HitTestMode.Transparent)
  }
}

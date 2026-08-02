// 受击视觉反馈层：
// - 受到伤害时屏幕边缘红色暗角闪烁，强度随伤害量缩放；
// - 低血量（<=30）时暗角持续呼吸脉冲，提示危险状态。
const LOW_HP_THRESHOLD: number = 30;

@Component
export struct HitFeedback {
  @Prop @Watch('onHpChanged') hp: number = 100;
  @State flashOpacity: number = 0;
  @State lowHpPulse: boolean = false;
  private prevHp: number = 100;

  aboutToAppear(): void {
    this.prevHp = this.hp;
    // 低血量呼吸脉冲：透明度在 0.35 ~ 0.6 之间往复。
    animateTo({
      duration: 900,
      iterations: -1,
      playMode: PlayMode.Alternate,
      curve: Curve.EaseInOut
    }, () => {
      this.lowHpPulse = true;
    });
  }

  onHpChanged(): void {
    if (this.hp < this.prevHp) {
      const damage: number = this.prevHp - this.hp;
      // 闪烁峰值随伤害量缩放：小伤轻闪，重击强闪。
      this.flashOpacity = Math.min(0.85, 0.30 + damage / 40);
      animateTo({ duration: 480, curve: Curve.EaseOut }, () => {
        this.flashOpacity = 0;
      });
    }
    this.prevHp = this.hp;
  }

  build() {
    Stack() {
      // 受击闪烁暗角。
      Stack()
        .width('100%')
        .height('100%')
        .radialGradient({
          center: { x: '50%', y: '50%' },
          radius: '75%',
          colors: [['rgba(180, 30, 40, 0)', 0.55], ['rgba(180, 30, 40, 0.9)', 1.0]]
        })
        .opacity(this.flashOpacity)
        .hitTestBehavior(HitTestMode.Transparent)

      // 低血量持续脉冲暗角。
      if (this.hp <= LOW_HP_THRESHOLD && this.hp > 0) {
        Stack()
          .width('100%')
          .height('100%')
          .radialGradient({
            center: { x: '50%', y: '50%' },
            radius: '78%',
            colors: [['rgba(160, 24, 34, 0)', 0.6], ['rgba(160, 24, 34, 0.85)', 1.0]]
          })
          .opacity(this.lowHpPulse ? 0.6 : 0.35)
          .hitTestBehavior(HitTestMode.Transparent)
      }
    }
    .width('100%')
    .height('100%')
    .hitTestBehavior(HitTestMode.Transparent)
  }
}

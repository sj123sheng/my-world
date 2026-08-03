// 受击视觉反馈层：
// - 受到伤害时屏幕边缘红色暗角闪烁，强度随伤害量缩放；
// - 大额伤害（>=15）叠加白色闪光冲击；
// - 低血量（<=30）时暗角持续呼吸脉冲，提示危险状态。
const LOW_HP_THRESHOLD: number = 30;
const HEAVY_HIT_THRESHOLD: number = 15;

@Component
export struct HitFeedback {
  @Prop @Watch('onHpChanged') hp: number = 100;
  @State flashOpacity: number = 0;
  @State whiteFlashOpacity: number = 0;
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
      // 红色闪烁峰值随伤害量缩放：小伤轻闪，重击强闪。
      this.flashOpacity = Math.min(0.9, 0.35 + damage / 35);
      animateTo({ duration: 420, curve: Curve.EaseOut }, () => {
        this.flashOpacity = 0;
      });
      // 大额伤害叠加白色闪光冲击。
      if (damage >= HEAVY_HIT_THRESHOLD) {
        this.whiteFlashOpacity = Math.min(0.5, 0.2 + damage / 60);
        animateTo({ duration: 280, curve: Curve.EaseOut }, () => {
          this.whiteFlashOpacity = 0;
        });
      }
    }
    this.prevHp = this.hp;
  }

  build() {
    Stack() {
      // 受击红色暗角闪烁。
      Stack()
        .width('100%')
        .height('100%')
        .radialGradient({
          center: ['50%', '50%'],
          radius: '72%',
          colors: [['rgba(200, 30, 40, 0)', 0.5], ['rgba(200, 30, 40, 0.95)', 1.0]]
        })
        .opacity(this.flashOpacity)
        .hitTestBehavior(HitTestMode.Transparent)

      // 大额伤害白色闪光冲击。
      Stack()
        .width('100%')
        .height('100%')
        .radialGradient({
          center: ['50%', '50%'],
          radius: '60%',
          colors: [['rgba(255, 240, 240, 0.8)', 0.0], ['rgba(255, 200, 200, 0)', 0.7]]
        })
        .opacity(this.whiteFlashOpacity)
        .hitTestBehavior(HitTestMode.Transparent)

      // 低血量持续脉冲暗角。
      if (this.hp <= LOW_HP_THRESHOLD && this.hp > 0) {
        Stack()
          .width('100%')
          .height('100%')
          .radialGradient({
            center: ['50%', '50%'],
            radius: '75%',
            colors: [['rgba(180, 20, 30, 0)', 0.55], ['rgba(180, 20, 30, 0.9)', 1.0]]
          })
          .opacity(this.lowHpPulse ? 0.6 : 0.35)
          .animation({ duration: 900, curve: Curve.EaseInOut })
          .hitTestBehavior(HitTestMode.Transparent)
      }
    }
    .width('100%')
    .height('100%')
    .hitTestBehavior(HitTestMode.Transparent)
  }
}

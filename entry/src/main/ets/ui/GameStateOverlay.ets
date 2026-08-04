import { retryBoss, startEncounter, advanceLevel } from '../napi/Bridge';

// 遭遇结算界面：失败时提供重试，胜利时提供继续/推进，
// 遮罩阻断下层交互，构成完整的战斗闭环。
// 增强入场动画：装饰粒子脉冲、交错延迟弹入、背景呼吸光晕。
const ENCOUNTER_STATE_VICTORY: number = 2;
const ENCOUNTER_STATE_DEFEAT: number = 3;
const ENCOUNTER_MODE_LEVEL_FLOW: number = 4;
const ENCOUNTER_MODE_BOSS: number = 5;

@Component
export struct GameStateOverlay {
  @Prop encounterState: number = 0;
  @Prop encounterMode: number = 0;
  @State appeared: boolean = false;
  @State particlePulse: boolean = false;
  @State bgPulse: boolean = false;

  aboutToAppear(): void {
    setTimeout(() => {
      this.appeared = true;
    }, 50);
    // 装饰粒子持续呼吸脉冲。
    animateTo({
      duration: 1400,
      iterations: -1,
      playMode: PlayMode.Alternate,
      curve: Curve.EaseInOut
    }, () => {
      this.particlePulse = true;
    });
    // 背景光晕缓慢呼吸。
    animateTo({
      duration: 2000,
      iterations: -1,
      playMode: PlayMode.Alternate,
      curve: Curve.EaseInOut
    }, () => {
      this.bgPulse = true;
    });
  }

  private retry(): void {
    if (this.encounterMode === ENCOUNTER_MODE_BOSS) {
      retryBoss();
    } else {
      startEncounter(this.encounterMode);
    }
  }

  private continueAfterVictory(): void {
    if (this.encounterMode === ENCOUNTER_MODE_LEVEL_FLOW) {
      advanceLevel();
    } else {
      startEncounter(this.encounterMode);
    }
  }

  // 装饰粒子：围绕标题的 4 个小光点，交错脉冲。
  @Builder
  decorParticles(color: string) {
    Stack() {
      Circle().width(4).height(4).fill(color)
        .position({ x: -50, y: -20 })
        .opacity(this.particlePulse ? 0.8 : 0.2)
        .animation({ duration: 800, curve: Curve.EaseInOut })
      Circle().width(3).height(3).fill(color)
        .position({ x: 46, y: -16 })
        .opacity(this.particlePulse ? 0.2 : 0.7)
        .animation({ duration: 900, curve: Curve.EaseInOut })
      Circle().width(3).height(3).fill(color)
        .position({ x: -42, y: 18 })
        .opacity(this.particlePulse ? 0.6 : 0.15)
        .animation({ duration: 1100, curve: Curve.EaseInOut })
      Circle().width(4).height(4).fill(color)
        .position({ x: 40, y: 22 })
        .opacity(this.particlePulse ? 0.15 : 0.65)
        .animation({ duration: 1000, curve: Curve.EaseInOut })
    }
    .width(120)
    .height(60)
    .hitTestBehavior(HitTestMode.Transparent)
  }

  build() {
    Stack() {
      if (this.encounterState === ENCOUNTER_STATE_DEFEAT) {
        Stack() {
          Column()
            .width('100%')
            .height('100%')
            .backgroundColor('rgba(10, 6, 8, 0.72)')

          // 背景呼吸光晕。
          Stack()
            .width(240).height(240)
            .borderRadius(120)
            .backgroundColor('rgba(224, 106, 94, 0.06)')
            .opacity(this.bgPulse ? 1 : 0.4)
            .animation({ duration: 2000, curve: Curve.EaseInOut })

          Column({ space: 16 }) {
            // 上装饰线。
            Stack()
              .width(this.appeared ? 60 : 0)
              .height(1)
              .backgroundColor('rgba(224, 106, 94, 0.5)')
              .animation({ duration: 600, curve: Curve.EaseOut })

            // 标题 + 装饰粒子。
            Stack() {
              Text('战败')
                .fontSize(36)
                .fontWeight(FontWeight.Bold)
                .fontColor('#E06A5E')
                .letterSpacing(12)
                .textShadow({ radius: 20, color: '#60E06A5E', offsetX: 0, offsetY: 0 })
                .opacity(this.appeared ? 1 : 0)
                .scale({ x: this.appeared ? 1.0 : 0.85, y: this.appeared ? 1.0 : 0.85 })
                .animation({ duration: 500, curve: Curve.EaseOut })
              this.decorParticles('#E06A5E')
            }
            .width(160).height(60)

            Text('共鸣之火暂时熄灭，再试一次')
              .fontSize(13)
              .fontColor('rgba(229, 236, 234, 0.6)')
              .letterSpacing(1)
              .opacity(this.appeared ? 1 : 0)
              .translate({ y: this.appeared ? 0 : 8 })
              .animation({ duration: 500, delay: 200, curve: Curve.EaseOut })

            // 下装饰线。
            Stack()
              .width(this.appeared ? 40 : 0)
              .height(1)
              .backgroundColor('rgba(224, 106, 94, 0.3)')
              .animation({ duration: 600, delay: 100, curve: Curve.EaseOut })

            Button('重试')
              .width(168)
              .height(44)
              .backgroundColor('#5A2750')
              .linearGradient({ angle: 135, colors: [['#7A3A68', 0.0], ['#5A2750', 0.5], ['#3A1530', 1.0]] })
              .border({ width: 1, color: '#AB4F92' })
              .fontColor('#F0C4E4')
              .fontSize(16)
              .fontWeight(FontWeight.Medium)
              .shadow({ radius: 10, color: '#40AB4F92', offsetX: 0, offsetY: 2 })
              .margin({ top: 8 })
              .opacity(this.appeared ? 1 : 0)
              .scale({ x: this.appeared ? 1.0 : 0.9, y: this.appeared ? 1.0 : 0.9 })
              .animation({ duration: 400, delay: 300, curve: Curve.EaseOut })
              .onClick(() => {
                this.retry();
              })
          }
          .alignItems(HorizontalAlign.Center)
        }
        .width('100%')
        .height('100%')
      }

      if (this.encounterState === ENCOUNTER_STATE_VICTORY) {
        Stack() {
          Column()
            .width('100%')
            .height('100%')
            .backgroundColor('rgba(8, 12, 12, 0.66)')

          // 背景呼吸光晕。
          Stack()
            .width(260).height(260)
            .borderRadius(130)
            .backgroundColor('rgba(242, 217, 168, 0.05)')
            .opacity(this.bgPulse ? 1 : 0.3)
            .animation({ duration: 2000, curve: Curve.EaseInOut })

          Column({ space: 16 }) {
            // 上装饰线。
            Stack()
              .width(this.appeared ? 60 : 0)
              .height(1)
              .backgroundColor('rgba(242, 217, 168, 0.5)')
              .animation({ duration: 600, curve: Curve.EaseOut })

            // 标题 + 装饰粒子。
            Stack() {
              Text('共鸣完成')
                .fontSize(32)
                .fontWeight(FontWeight.Bold)
                .fontColor('#F2D9A8')
                .letterSpacing(10)
                .textShadow({ radius: 20, color: '#60F2D9A8', offsetX: 0, offsetY: 0 })
                .opacity(this.appeared ? 1 : 0)
                .scale({ x: this.appeared ? 1.0 : 0.85, y: this.appeared ? 1.0 : 0.85 })
                .animation({ duration: 500, curve: Curve.EaseOut })
              this.decorParticles('#F2D9A8')
            }
            .width(180).height(60)

            Text('遗迹的回响与你同在')
              .fontSize(13)
              .fontColor('rgba(229, 236, 234, 0.6)')
              .letterSpacing(1)
              .opacity(this.appeared ? 1 : 0)
              .translate({ y: this.appeared ? 0 : 8 })
              .animation({ duration: 500, delay: 200, curve: Curve.EaseOut })

            // 下装饰线。
            Stack()
              .width(this.appeared ? 40 : 0)
              .height(1)
              .backgroundColor('rgba(242, 217, 168, 0.3)')
              .animation({ duration: 600, delay: 100, curve: Curve.EaseOut })

            Button('继续')
              .width(168)
              .height(44)
              .backgroundColor('#2F4858')
              .linearGradient({ angle: 135, colors: [['#4A6A7A', 0.0], ['#2F4858', 0.5], ['#1A2E38', 1.0]] })
              .border({ width: 1, color: '#5B8FA8' })
              .fontColor('#D6E2DF')
              .fontSize(16)
              .fontWeight(FontWeight.Medium)
              .shadow({ radius: 10, color: '#405B8FA8', offsetX: 0, offsetY: 2 })
              .margin({ top: 8 })
              .opacity(this.appeared ? 1 : 0)
              .scale({ x: this.appeared ? 1.0 : 0.9, y: this.appeared ? 1.0 : 0.9 })
              .animation({ duration: 400, delay: 300, curve: Curve.EaseOut })
              .onClick(() => {
                this.continueAfterVictory();
              })
          }
          .alignItems(HorizontalAlign.Center)
        }
        .width('100%')
        .height('100%')
      }
    }
    .width('100%')
    .height('100%')
    .hitTestBehavior(
      this.encounterState === ENCOUNTER_STATE_DEFEAT ||
      this.encounterState === ENCOUNTER_STATE_VICTORY
        ? HitTestMode.Default : HitTestMode.Transparent)
  }
}

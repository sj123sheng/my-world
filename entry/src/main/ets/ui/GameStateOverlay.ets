import { retryBoss, startEncounter, advanceLevel } from '../napi/Bridge';

// 遭遇结算界面：失败时提供重试，胜利时提供继续/推进，
// 遮罩阻断下层交互，构成完整的战斗闭环。
const ENCOUNTER_STATE_VICTORY: number = 2;
const ENCOUNTER_STATE_DEFEAT: number = 3;
const ENCOUNTER_MODE_LEVEL_FLOW: number = 4;
const ENCOUNTER_MODE_BOSS: number = 5;

@Component
export struct GameStateOverlay {
  @Prop encounterState: number = 0;
  @Prop encounterMode: number = 0;
  @State appeared: boolean = false;

  aboutToAppear(): void {
    // 延迟一帧触发入场动画。
    setTimeout(() => {
      this.appeared = true;
    }, 50);
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

  build() {
    Stack() {
      if (this.encounterState === ENCOUNTER_STATE_DEFEAT) {
        Stack() {
          Column()
            .width('100%')
            .height('100%')
            .backgroundColor('rgba(10, 6, 8, 0.72)')

          Column({ space: 16 }) {
            // 装饰线。
            Stack()
              .width(60)
              .height(1)
              .backgroundColor('rgba(224, 106, 94, 0.5)')
              .opacity(this.appeared ? 1 : 0)
              .animation({ duration: 600, curve: Curve.EaseOut })

            Text('战败')
              .fontSize(36)
              .fontWeight(FontWeight.Bold)
              .fontColor('#E06A5E')
              .letterSpacing(12)
              .textShadow({ radius: 20, color: '#60E06A5E', offsetX: 0, offsetY: 0 })
              .opacity(this.appeared ? 1 : 0)
              .scale({ x: this.appeared ? 1.0 : 0.85, y: this.appeared ? 1.0 : 0.85 })
              .animation({ duration: 500, curve: Curve.EaseOut })

            Text('共鸣之火暂时熄灭，再试一次')
              .fontSize(13)
              .fontColor('rgba(229, 236, 234, 0.6)')
              .letterSpacing(1)
              .opacity(this.appeared ? 1 : 0)
              .animation({ duration: 500, delay: 200, curve: Curve.EaseOut })

            // 装饰线。
            Stack()
              .width(40)
              .height(1)
              .backgroundColor('rgba(224, 106, 94, 0.3)')
              .opacity(this.appeared ? 1 : 0)
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

          Column({ space: 16 }) {
            // 装饰线。
            Stack()
              .width(60)
              .height(1)
              .backgroundColor('rgba(242, 217, 168, 0.5)')
              .opacity(this.appeared ? 1 : 0)
              .animation({ duration: 600, curve: Curve.EaseOut })

            Text('共鸣完成')
              .fontSize(32)
              .fontWeight(FontWeight.Bold)
              .fontColor('#F2D9A8')
              .letterSpacing(10)
              .textShadow({ radius: 20, color: '#60F2D9A8', offsetX: 0, offsetY: 0 })
              .opacity(this.appeared ? 1 : 0)
              .scale({ x: this.appeared ? 1.0 : 0.85, y: this.appeared ? 1.0 : 0.85 })
              .animation({ duration: 500, curve: Curve.EaseOut })

            Text('遗迹的回响与你同在')
              .fontSize(13)
              .fontColor('rgba(229, 236, 234, 0.6)')
              .letterSpacing(1)
              .opacity(this.appeared ? 1 : 0)
              .animation({ duration: 500, delay: 200, curve: Curve.EaseOut })

            // 装饰线。
            Stack()
              .width(40)
              .height(1)
              .backgroundColor('rgba(242, 217, 168, 0.3)')
              .opacity(this.appeared ? 1 : 0)
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

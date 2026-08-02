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
            .backgroundColor('rgba(10, 6, 8, 0.66)')

          Column({ space: 18 }) {
            Text('战败')
              .fontSize(34)
              .fontWeight(FontWeight.Bold)
              .fontColor('#E06A5E')
              .letterSpacing(10)
              .textShadow({ radius: 14, color: '#B0000000', offsetX: 0, offsetY: 2 })
            Text('共鸣之火暂时熄灭，再试一次')
              .fontSize(13)
              .fontColor('rgba(229, 236, 234, 0.7)')
              .letterSpacing(1)
            Button('重试')
              .width(168)
              .height(44)
              .backgroundColor('#5A2750')
              .border({ width: 1, color: '#AB4F92' })
              .fontColor('#F0C4E4')
              .fontSize(16)
              .fontWeight(FontWeight.Medium)
              .stateEffect(true)
              .margin({ top: 8 })
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
            .backgroundColor('rgba(8, 12, 12, 0.6)')

          Column({ space: 18 }) {
            Text('共鸣完成')
              .fontSize(30)
              .fontWeight(FontWeight.Bold)
              .fontColor('#F2D9A8')
              .letterSpacing(8)
              .textShadow({ radius: 14, color: '#B0000000', offsetX: 0, offsetY: 2 })
            Text('遗迹的回响与你同在')
              .fontSize(13)
              .fontColor('rgba(229, 236, 234, 0.7)')
              .letterSpacing(1)
            Button('继续')
              .width(168)
              .height(44)
              .backgroundColor('#2F4858')
              .border({ width: 1, color: '#5B8FA8' })
              .fontColor('#D6E2DF')
              .fontSize(16)
              .fontWeight(FontWeight.Medium)
              .stateEffect(true)
              .margin({ top: 8 })
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

// 新手引导浮层：首次运行分步教学核心操控（移动 → 普攻 → 闪避 → 源技能），
// 每步由真实游戏状态驱动完成，全部完成后持久化标记，不再打扰玩家。
const TUTORIAL_STEP_MOVE: number = 0;
const TUTORIAL_STEP_ATTACK: number = 1;
const TUTORIAL_STEP_DODGE: number = 2;
const TUTORIAL_STEP_SOURCE: number = 3;
const TUTORIAL_STEP_COUNT: number = 4;

@Component
export struct Tutorial {
  @Prop @Watch('onMovingChanged') moving: boolean = false;
  @Prop @Watch('onComboChanged') comboSegment: number = 0;
  @Prop @Watch('onInvulnerableChanged') invulnerable: boolean = false;
  @Prop @Watch('onSourceUsed') radianceCooldownMs: number = 0;
  @StorageLink('tutorialDone') tutorialDone: boolean = false;
  @State step: number = TUTORIAL_STEP_MOVE;
  @State showComplete: boolean = false;
  private moveSamples: number = 0;
  private completeTimer: number = -1;

  aboutToAppear(): void {
    if (this.tutorialDone) {
      this.step = TUTORIAL_STEP_COUNT;
    }
  }

  aboutToDisappear(): void {
    if (this.completeTimer !== -1) {
      clearTimeout(this.completeTimer);
      this.completeTimer = -1;
    }
  }

  private advance(): void {
    if (this.step >= TUTORIAL_STEP_COUNT) {
      return;
    }
    this.step += 1;
    if (this.step >= TUTORIAL_STEP_COUNT) {
      this.showComplete = true;
      this.tutorialDone = true;
      this.completeTimer = setTimeout((): void => {
        this.showComplete = false;
        this.completeTimer = -1;
      }, 1800);
    }
  }

  onMovingChanged(): void {
    // 移动步骤需要持续位移样本，避免误触通过。
    if (this.step === TUTORIAL_STEP_MOVE && this.moving) {
      this.moveSamples += 1;
      if (this.moveSamples >= 3) {
        this.advance();
      }
    }
  }

  onComboChanged(): void {
    if (this.step === TUTORIAL_STEP_ATTACK && this.comboSegment >= 1) {
      this.advance();
    }
  }

  onInvulnerableChanged(): void {
    if (this.step === TUTORIAL_STEP_DODGE && this.invulnerable) {
      this.advance();
    }
  }

  onSourceUsed(): void {
    if (this.step === TUTORIAL_STEP_SOURCE && this.radianceCooldownMs > 0) {
      this.advance();
    }
  }

  private stepText(): string {
    switch (this.step) {
      case TUTORIAL_STEP_MOVE: return '滑动屏幕左侧移动角色';
      case TUTORIAL_STEP_ATTACK: return '点击右下角普攻攻击敌人';
      case TUTORIAL_STEP_DODGE: return '点击闪避躲开敌人攻击';
      case TUTORIAL_STEP_SOURCE: return '释放辉印，开启三源共鸣';
      default: return '';
    }
  }

  build() {
    Stack({ alignContent: Alignment.Top }) {
      if (!this.tutorialDone && this.step < TUTORIAL_STEP_COUNT) {
        Column({ space: 8 }) {
          Text(this.stepText())
            .fontSize(15)
            .fontColor('#E5ECEA')
            .fontWeight(FontWeight.Medium)
            .letterSpacing(1)
          Row({ space: 6 }) {
            ForEach([0, 1, 2, 3], (index: number) => {
              Circle({ width: 6, height: 6 })
                .fill(index <= this.step ? '#D9A145' : 'rgba(229, 236, 234, 0.3)')
            }, (index: number) => index.toString())
          }
        }
        .alignItems(HorizontalAlign.Center)
        .padding({ left: 20, right: 20, top: 10, bottom: 10 })
        .backgroundColor('#B311181D')
        .borderRadius(16)
        .border({ width: 1, color: 'rgba(217, 161, 69, 0.35)' })
        .margin({ top: 74 })
      }

      if (this.showComplete) {
        Text('引导完成')
          .fontSize(16)
          .fontColor('#F2D9A8')
          .fontWeight(FontWeight.Bold)
          .letterSpacing(3)
          .padding({ left: 22, right: 22, top: 10, bottom: 10 })
          .backgroundColor('#B311181D')
          .borderRadius(16)
          .border({ width: 1, color: 'rgba(217, 161, 69, 0.5)' })
          .margin({ top: 74 })
      }
    }
    .width('100%')
    .height('100%')
    .hitTestBehavior(HitTestMode.Transparent)
  }
}

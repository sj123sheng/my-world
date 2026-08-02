import { pushAction, startEncounter, advanceLevel, useSupply, retryBoss, toggleDebugHud } from '../napi/Bridge';

@Component
export struct CombatControls {
  @Prop radianceCooldownMs: number = 0;
  @Prop currentCooldownMs: number = 0;
  @Prop corruptionCooldownMs: number = 0;
  // 冷却总时长由快照下发（native CombatConfig.sourceCooldownMs），不再硬编码。
  @Prop radianceCooldownTotalMs: number = 3000;
  @Prop currentCooldownTotalMs: number = 4000;
  @Prop corruptionCooldownTotalMs: number = 5000;
  @Prop ultimateWindowMs: number = 0;
  @State debugOpen: boolean = false;
  @State ultimatePulse: boolean = false;

  aboutToAppear(): void {
    // 终结技可用时的呼吸脉冲。
    animateTo({
      duration: 650,
      iterations: -1,
      playMode: PlayMode.Alternate,
      curve: Curve.EaseInOut
    }, () => {
      this.ultimatePulse = true;
    });
  }

  // 冷却环遮罩：剩余时间 > 0 时覆盖按钮，环形进度随快照平滑收缩。
  @Builder
  cooldownRing(remainingMs: number, totalMs: number, color: string) {
    if (remainingMs > 0) {
      Stack() {
        Progress({ value: remainingMs / Math.max(totalMs, 1) * 100, total: 100, type: ProgressType.Ring })
          .width('100%')
          .height('100%')
          .color(color)
          .backgroundColor('rgba(8, 12, 18, 0.55)')
          .style({ strokeWidth: 4 })
          .animation({ duration: 100, curve: Curve.Linear })
        Text(`${Math.ceil(remainingMs / 1000)}`)
          .fontSize(14)
          .fontWeight(FontWeight.Bold)
          .fontColor('#E5ECEA')
      }
      .width('100%')
      .height('100%')
      .hitTestBehavior(HitTestMode.Transparent)
    }
  }

  build() {
    Stack() {
      // 技能按钮簇：右下角弧形布局（普攻主钮贴角，技能向左上扇形展开）。
      Stack() {
        // 终结技：普攻上方，共鸣窗口开启时金色高亮呼吸。
        Stack() {
          Button('终结')
            .width(56)
            .height(56)
            .buttonStyle(ButtonStyleType.Circle)
            .backgroundColor(this.ultimateWindowMs > 0 ? '#C89B3C' : '#343A40')
            .border({ width: 2, color: this.ultimateWindowMs > 0 ? '#F5D78E' : '#4A5058' })
            .fontColor(this.ultimateWindowMs > 0 ? '#1A1408' : '#7B8288')
            .fontSize(14)
            .fontWeight(FontWeight.Bold)
            .stateEffect(true)
            .scale({
              x: this.ultimateWindowMs > 0 && this.ultimatePulse ? 1.08 : 1.0,
              y: this.ultimateWindowMs > 0 && this.ultimatePulse ? 1.08 : 1.0
            })
            .animation({ duration: 300, curve: Curve.EaseInOut })
            .onClick(() => { pushAction(5); })
        }
        .width(56)
        .height(56)
        .position({ x: 194, y: 66 })

        // 蚀质：弧形最靠近终结技。
        Stack() {
          Button('蚀质')
            .width(48)
            .height(48)
            .buttonStyle(ButtonStyleType.Circle)
            .backgroundColor(this.corruptionCooldownMs > 0 ? '#3A1B34' : '#5A2750')
            .border({ width: 1.5, color: '#8B4A7C' })
            .fontColor('#F0C4E4')
            .fontSize(13)
            .fontWeight(FontWeight.Medium)
            .stateEffect(true)
            .onClick(() => { pushAction(4); })
          this.cooldownRing(this.corruptionCooldownMs, this.corruptionCooldownTotalMs, '#AB4F92')
        }
        .width(48)
        .height(48)
        .position({ x: 128, y: 78 })

        // 脉流：弧形中段。
        Stack() {
          Button('脉流')
            .width(48)
            .height(48)
            .buttonStyle(ButtonStyleType.Circle)
            .backgroundColor(this.currentCooldownMs > 0 ? '#173B36' : '#1F5A50')
            .border({ width: 1.5, color: '#3E8B7E' })
            .fontColor('#B8F2E6')
            .fontSize(13)
            .fontWeight(FontWeight.Medium)
            .stateEffect(true)
            .onClick(() => { pushAction(3); })
          this.cooldownRing(this.currentCooldownMs, this.currentCooldownTotalMs, '#43CDB5')
        }
        .width(48)
        .height(48)
        .position({ x: 68, y: 96 })

        // 辉印：弧形最外侧。
        Stack() {
          Button('辉印')
            .width(48)
            .height(48)
            .buttonStyle(ButtonStyleType.Circle)
            .backgroundColor(this.radianceCooldownMs > 0 ? '#33291A' : '#7A5A22')
            .border({ width: 1.5, color: '#A8853F' })
            .fontColor('#F2E0B8')
            .fontSize(13)
            .fontWeight(FontWeight.Medium)
            .stateEffect(true)
            .onClick(() => { pushAction(2); })
          this.cooldownRing(this.radianceCooldownMs, this.radianceCooldownTotalMs, '#D9A145')
        }
        .width(48)
        .height(48)
        .position({ x: 16, y: 122 })

        // 闪避：普攻左侧。
        Stack() {
          Button('闪避')
            .width(56)
            .height(56)
            .buttonStyle(ButtonStyleType.Circle)
            .backgroundColor('#274043')
            .border({ width: 1.5, color: '#3E6B62' })
            .fontColor('#9FD8CB')
            .fontSize(14)
            .fontWeight(FontWeight.Medium)
            .stateEffect(true)
            .onClick(() => { pushAction(1); })
        }
        .width(56)
        .height(56)
        .position({ x: 112, y: 146 })

        // 普攻：主按钮，最右贴近底角。
        Stack() {
          Button('普攻')
            .width(76)
            .height(76)
            .buttonStyle(ButtonStyleType.Circle)
            .backgroundColor('#2F4858')
            .border({ width: 2, color: '#5B8FA8' })
            .fontColor('#D6E2DF')
            .fontSize(18)
            .fontWeight(FontWeight.Bold)
            .stateEffect(true)
            .onClick(() => { pushAction(0); })
        }
        .width(76)
        .height(76)
        .position({ x: 184, y: 134 })
      }
      .width(260)
      .height(210)
      .position({ x: '100%', y: '100%' })
      .markAnchor({ x: 278, y: 244 })
      .hitTestBehavior(HitTestMode.Transparent)

      // 调试入口：右上小型圆钮，展开/收起调试面板。
      Button('☰')
        .width(40)
        .height(40)
        .buttonStyle(ButtonStyleType.Circle)
        .backgroundColor('rgba(20, 26, 38, 0.55)')
        .border({ width: 1, color: 'rgba(199, 208, 206, 0.35)' })
        .fontColor('#C7D0CE')
        .fontSize(15)
        .stateEffect(true)
        .position({ x: '100%', y: 14 })
        .markAnchor({ x: 58, y: 0 })
        .onClick(() => {
          this.debugOpen = !this.debugOpen;
        })

      // 调试面板：默认收起，避免调试按钮裸露在游戏画面。
      if (this.debugOpen) {
        Column({ space: 6 }) {
          Row({ space: 4 }) {
            Button('训练').width(36).height(28).fontSize(11).buttonStyle(ButtonStyleType.Normal)
              .borderRadius(8).backgroundColor('#2A3441').fontColor('#C7D0CE').stateEffect(true)
              .onClick(() => { startEncounter(0); })
            Button('兽群').width(36).height(28).fontSize(11).buttonStyle(ButtonStyleType.Normal)
              .borderRadius(8).backgroundColor('#2A3441').fontColor('#C7D0CE').stateEffect(true)
              .onClick(() => { startEncounter(1); })
            Button('混战').width(36).height(28).fontSize(11).buttonStyle(ButtonStyleType.Normal)
              .borderRadius(8).backgroundColor('#2A3441').fontColor('#C7D0CE').stateEffect(true)
              .onClick(() => { startEncounter(2); })
            Button('守卫').width(36).height(28).fontSize(11).buttonStyle(ButtonStyleType.Normal)
              .borderRadius(8).backgroundColor('#2A3441').fontColor('#C7D0CE').stateEffect(true)
              .onClick(() => { startEncounter(3); })
          }
          Row({ space: 4 }) {
            Button('流程').width(36).height(28).fontSize(11).buttonStyle(ButtonStyleType.Normal)
              .borderRadius(8).backgroundColor('#2A3441').fontColor('#C7D0CE').stateEffect(true)
              .onClick(() => { startEncounter(4); })
            Button('首领').width(36).height(28).fontSize(11).buttonStyle(ButtonStyleType.Normal)
              .borderRadius(8).backgroundColor('#2A3441').fontColor('#C7D0CE').stateEffect(true)
              .onClick(() => { startEncounter(5); })
            Button('推进').width(36).height(28).fontSize(11).buttonStyle(ButtonStyleType.Normal)
              .borderRadius(8).backgroundColor('#2A3441').fontColor('#C7D0CE').stateEffect(true)
              .onClick(() => { advanceLevel(); })
          }
          Row({ space: 4 }) {
            Button('补给').width(36).height(28).fontSize(11).buttonStyle(ButtonStyleType.Normal)
              .borderRadius(8).backgroundColor('#2A3441').fontColor('#C7D0CE').stateEffect(true)
              .onClick(() => { useSupply(); })
            Button('重试').width(36).height(28).fontSize(11).buttonStyle(ButtonStyleType.Normal)
              .borderRadius(8).backgroundColor('#2A3441').fontColor('#C7D0CE').stateEffect(true)
              .onClick(() => { retryBoss(); })
            Button('调试').width(36).height(28).fontSize(11).buttonStyle(ButtonStyleType.Normal)
              .borderRadius(8).backgroundColor('#2A3441').fontColor('#C7D0CE').stateEffect(true)
              .onClick(() => { toggleDebugHud(); })
          }
        }
        .padding(10)
        .backgroundColor('#CC11181D')
        .borderRadius(14)
        .border({ width: 1, color: 'rgba(199, 208, 206, 0.2)' })
        .position({ x: '100%', y: 62 })
        .markAnchor({ x: 196, y: 0 })
      }
    }
    .width('100%')
    .height('100%')
    .hitTestBehavior(HitTestMode.Transparent)
  }
}

import { pushAction, startEncounter, advanceLevel, useSupply, retryBoss, toggleDebugHud } from '../napi/Bridge';

@Component
export struct CombatControls {
  @Prop @Watch('onRadianceCd') radianceCooldownMs: number = 0;
  @Prop @Watch('onCurrentCd') currentCooldownMs: number = 0;
  @Prop @Watch('onCorruptionCd') corruptionCooldownMs: number = 0;
  // 冷却总时长由快照下发（native CombatConfig.sourceCooldownMs），不再硬编码。
  @Prop radianceCooldownTotalMs: number = 3000;
  @Prop currentCooldownTotalMs: number = 4000;
  @Prop corruptionCooldownTotalMs: number = 5000;
  @Prop ultimateWindowMs: number = 0;
  @State debugOpen: boolean = false;
  @State ultimatePulse: boolean = false;
  @State sourceReadyPulse: boolean = false;
  @State attackPressed: boolean = false;
  @State dodgePressed: boolean = false;
  @State radiancePressed: boolean = false;
  @State currentPressed: boolean = false;
  @State corruptionPressed: boolean = false;
  @State ultimatePressed: boolean = false;
  @State jumpPressed: boolean = false;
  @State glidePressed: boolean = false;
  @State radianceFlash: number = 0;
  @State currentFlash: number = 0;
  @State corruptionFlash: number = 0;
  private prevRadianceCd: number = 0;
  private prevCurrentCd: number = 0;
  private prevCorruptionCd: number = 0;

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
    // 源技能可用时的微光呼吸。
    animateTo({
      duration: 1200,
      iterations: -1,
      playMode: PlayMode.Alternate,
      curve: Curve.EaseInOut
    }, () => {
      this.sourceReadyPulse = true;
    });
  }

  onRadianceCd(): void {
    if (this.radianceCooldownMs > 0 && this.prevRadianceCd <= 0) {
      this.radianceFlash = 1;
      animateTo({ duration: 300, curve: Curve.EaseOut }, () => {
        this.radianceFlash = 0;
      });
    }
    this.prevRadianceCd = this.radianceCooldownMs;
  }

  onCurrentCd(): void {
    if (this.currentCooldownMs > 0 && this.prevCurrentCd <= 0) {
      this.currentFlash = 1;
      animateTo({ duration: 300, curve: Curve.EaseOut }, () => {
        this.currentFlash = 0;
      });
    }
    this.prevCurrentCd = this.currentCooldownMs;
  }

  onCorruptionCd(): void {
    if (this.corruptionCooldownMs > 0 && this.prevCorruptionCd <= 0) {
      this.corruptionFlash = 1;
      animateTo({ duration: 300, curve: Curve.EaseOut }, () => {
        this.corruptionFlash = 0;
      });
    }
    this.prevCorruptionCd = this.corruptionCooldownMs;
  }

  // 冷却外圈“画圈”特效：环从 12 点方向起笔顺时针逐渐生长，
  // 生长比例 = 已恢复比例；整圈画满即冷却结束、遮罩消失可释放。
  // 淡色轨道环提示画圈终点，中心保留剩余秒数供精确读数。
  @Builder
  cooldownDraw(remainingMs: number, totalMs: number, color: string) {
    if (remainingMs > 0) {
      Stack() {
        Circle()
          .width('100%')
          .height('100%')
          .fill('rgba(6, 8, 14, 0.62)')
        Progress({
          value: (1 - Math.min(remainingMs / Math.max(totalMs, 1), 1)) * 100,
          total: 100,
          type: ProgressType.Ring
        })
          .width('100%')
          .height('100%')
          .color(color)
          .backgroundColor('rgba(255, 255, 255, 0.1)')
          .style({ strokeWidth: 4.5 })
          .animation({ duration: 120, curve: Curve.Linear })
        Text(`${Math.ceil(remainingMs / 1000)}`)
          .fontSize(14)
          .fontWeight(FontWeight.Bold)
          .fontColor('#E5ECEA')
          .textShadow({ radius: 4, color: '#80000000', offsetX: 0, offsetY: 1 })
      }
      .width('100%')
      .height('100%')
      .borderRadius('50%')
      .hitTestBehavior(HitTestMode.Transparent)
    }
  }

  build() {
    Stack() {
      // 技能按钮簇：右下角弧形布局（普攻主钮贴角，技能向左上扇形展开）。
      Stack() {
        // 滑翔：按住触发滑翔（GlidePress），松开结束（GlideRelease）。
        Stack() {
          Button('滑翔')
            .width(48)
            .height(48)
            .type(ButtonType.Circle)
            .backgroundColor(this.glidePressed ? '#1F5A50' : '#23303E')
            .border({ width: 1.5, color: this.glidePressed ? '#5EAB9E' : '#4A5A68' })
            .fontColor('#B8E8F2')
            .fontSize(12)
            .fontWeight(FontWeight.Medium)
            .shadow({ radius: 8, color: '#40000000', offsetX: 0, offsetY: 2 })
            .scale({ x: this.glidePressed ? 0.9 : 1.0, y: this.glidePressed ? 0.9 : 1.0 })
            .animation({ duration: 120, curve: Curve.EaseOut })
            .hitTestBehavior(HitTestMode.Block)
            .onTouch((e: TouchEvent) => {
              if (e.type === TouchType.Down) {
                this.glidePressed = true;
                pushAction(8);
              }
              if (e.type === TouchType.Up || e.type === TouchType.Cancel) {
                this.glidePressed = false;
                pushAction(9);
              }
            })
        }
        .width(48)
        .height(48)
        .position({ x: 206, y: 4 })

        // 跳跃：探索垂直机动入口。
        Stack() {
          Button('跳跃')
            .width(52)
            .height(52)
            .type(ButtonType.Circle)
            .backgroundColor('#2A3A2E')
            .border({ width: 1.5, color: '#4A6B52' })
            .fontColor('#C8E8D0')
            .fontSize(13)
            .fontWeight(FontWeight.Medium)
            .shadow({ radius: 8, color: '#40000000', offsetX: 0, offsetY: 2 })
            .scale({ x: this.jumpPressed ? 0.9 : 1.0, y: this.jumpPressed ? 0.9 : 1.0 })
            .animation({ duration: 120, curve: Curve.EaseOut })
            .hitTestBehavior(HitTestMode.Block)
            .onTouch((e: TouchEvent) => {
              if (e.type === TouchType.Down) this.jumpPressed = true;
              if (e.type === TouchType.Up || e.type === TouchType.Cancel) this.jumpPressed = false;
            })
            .onClick(() => { pushAction(6); })
        }
        .width(52)
        .height(52)
        .position({ x: 148, y: 12 })

        // 终结技：普攻上方，共鸣窗口开启时金色高亮呼吸。
        Stack() {
          Button('终结')
            .width(56)
            .height(56)
            .type(ButtonType.Circle)
            .backgroundColor(this.ultimateWindowMs > 0 ? '#C89B3C' : '#343A40')
            .linearGradient(this.ultimateWindowMs > 0
              ? { angle: 135, colors: [['#E5B84A', 0.0], ['#C89B3C', 0.5], ['#A67D2F', 1.0]] }
              : { angle: 135, colors: [['#4A5058', 0.0], ['#343A40', 0.5], ['#1E2428', 1.0]] })
            .border({ width: this.ultimateWindowMs > 0 ? 2.5 : 2,
              color: this.ultimateWindowMs > 0 ? '#F5D78E' : '#4A5058' })
            .fontColor(this.ultimateWindowMs > 0 ? '#1A1408' : '#7B8288')
            .fontSize(14)
            .fontWeight(FontWeight.Bold)
            .shadow({ radius: this.ultimateWindowMs > 0 ? 18 : 8,
              color: this.ultimateWindowMs > 0 ? '#60F5D78E' : '#40000000',
              offsetX: 0, offsetY: 2 })
            .scale({
              x: this.ultimatePressed ? 0.9 :
                (this.ultimateWindowMs > 0 && this.ultimatePulse ? 1.08 : 1.0),
              y: this.ultimatePressed ? 0.9 :
                (this.ultimateWindowMs > 0 && this.ultimatePulse ? 1.08 : 1.0)
            })
            .animation({ duration: 300, curve: Curve.EaseInOut })
            .hitTestBehavior(HitTestMode.Block)
            .onTouch((e: TouchEvent) => {
              if (e.type === TouchType.Down) this.ultimatePressed = true;
              if (e.type === TouchType.Up || e.type === TouchType.Cancel) this.ultimatePressed = false;
            })
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
            .type(ButtonType.Circle)
            .backgroundColor(this.corruptionCooldownMs > 0 ? '#3A1B34' : '#5A2750')
            .linearGradient(this.corruptionCooldownMs > 0
              ? { angle: 135, colors: [['#4A2540', 0.0], ['#3A1B34', 0.5], ['#2A1024', 1.0]] }
              : { angle: 135, colors: [['#7A3A68', 0.0], ['#5A2750', 0.5], ['#3A1530', 1.0]] })
            .border({ width: this.corruptionCooldownMs <= 0 && this.sourceReadyPulse ? 2 : 1.5,
              color: this.corruptionCooldownMs <= 0 ? '#AB6A9C' : '#8B4A7C' })
            .fontColor('#F0C4E4')
            .fontSize(13)
            .fontWeight(FontWeight.Medium)
            .shadow({ radius: this.corruptionCooldownMs <= 0 ? 12 : 6,
              color: this.corruptionCooldownMs <= 0 ? '#50AB4F92' : '#30000000',
              offsetX: 0, offsetY: 2 })
            .scale({ x: this.corruptionPressed ? 0.88 : 1.0, y: this.corruptionPressed ? 0.88 : 1.0 })
            .animation({ duration: 120, curve: Curve.EaseOut })
            .hitTestBehavior(HitTestMode.Block)
            .onTouch((e: TouchEvent) => {
              if (e.type === TouchType.Down) this.corruptionPressed = true;
              if (e.type === TouchType.Up || e.type === TouchType.Cancel) this.corruptionPressed = false;
            })
            .onClick(() => { pushAction(4); })
          this.cooldownDraw(this.corruptionCooldownMs, this.corruptionCooldownTotalMs, '#AB4F92')
          Circle()
            .width(48).height(48)
            .fill('rgba(240, 200, 230, 0.6)')
            .opacity(this.corruptionFlash)
            .hitTestBehavior(HitTestMode.Transparent)
        }
        .width(48)
        .height(48)
        .position({ x: 128, y: 78 })

        // 脉流：弧形中段。
        Stack() {
          Button('脉流')
            .width(48)
            .height(48)
            .type(ButtonType.Circle)
            .backgroundColor(this.currentCooldownMs > 0 ? '#173B36' : '#1F5A50')
            .linearGradient(this.currentCooldownMs > 0
              ? { angle: 135, colors: [['#1F4A44', 0.0], ['#173B36', 0.5], ['#0D2622', 1.0]] }
              : { angle: 135, colors: [['#2E7A6C', 0.0], ['#1F5A50', 0.5], ['#103830', 1.0]] })
            .border({ width: this.currentCooldownMs <= 0 && this.sourceReadyPulse ? 2 : 1.5,
              color: this.currentCooldownMs <= 0 ? '#5EAB9E' : '#3E8B7E' })
            .fontColor('#B8F2E6')
            .fontSize(13)
            .fontWeight(FontWeight.Medium)
            .shadow({ radius: this.currentCooldownMs <= 0 ? 12 : 6,
              color: this.currentCooldownMs <= 0 ? '#5043CDB5' : '#30000000',
              offsetX: 0, offsetY: 2 })
            .scale({ x: this.currentPressed ? 0.88 : 1.0, y: this.currentPressed ? 0.88 : 1.0 })
            .animation({ duration: 120, curve: Curve.EaseOut })
            .hitTestBehavior(HitTestMode.Block)
            .onTouch((e: TouchEvent) => {
              if (e.type === TouchType.Down) this.currentPressed = true;
              if (e.type === TouchType.Up || e.type === TouchType.Cancel) this.currentPressed = false;
            })
            .onClick(() => { pushAction(3); })
          this.cooldownDraw(this.currentCooldownMs, this.currentCooldownTotalMs, '#43CDB5')
          Circle()
            .width(48).height(48)
            .fill('rgba(200, 255, 240, 0.6)')
            .opacity(this.currentFlash)
            .hitTestBehavior(HitTestMode.Transparent)
        }
        .width(48)
        .height(48)
        .position({ x: 68, y: 96 })

        // 辉印：弧形最外侧。
        Stack() {
          Button('辉印')
            .width(48)
            .height(48)
            .type(ButtonType.Circle)
            .backgroundColor(this.radianceCooldownMs > 0 ? '#33291A' : '#7A5A22')
            .linearGradient(this.radianceCooldownMs > 0
              ? { angle: 135, colors: [['#443820', 0.0], ['#33291A', 0.5], ['#221A10', 1.0]] }
              : { angle: 135, colors: [['#9A7A3A', 0.0], ['#7A5A22', 0.5], ['#5A3A12', 1.0]] })
            .border({ width: this.radianceCooldownMs <= 0 && this.sourceReadyPulse ? 2 : 1.5,
              color: this.radianceCooldownMs <= 0 ? '#C8A55F' : '#A8853F' })
            .fontColor('#F2E0B8')
            .fontSize(13)
            .fontWeight(FontWeight.Medium)
            .shadow({ radius: this.radianceCooldownMs <= 0 ? 12 : 6,
              color: this.radianceCooldownMs <= 0 ? '#50D9A145' : '#30000000',
              offsetX: 0, offsetY: 2 })
            .scale({ x: this.radiancePressed ? 0.88 : 1.0, y: this.radiancePressed ? 0.88 : 1.0 })
            .animation({ duration: 120, curve: Curve.EaseOut })
            .hitTestBehavior(HitTestMode.Block)
            .onTouch((e: TouchEvent) => {
              if (e.type === TouchType.Down) this.radiancePressed = true;
              if (e.type === TouchType.Up || e.type === TouchType.Cancel) this.radiancePressed = false;
            })
            .onClick(() => { pushAction(2); })
          this.cooldownDraw(this.radianceCooldownMs, this.radianceCooldownTotalMs, '#D9A145')
          // 技能释放闪光叠加。
          Circle()
            .width(48).height(48)
            .fill('rgba(255, 240, 200, 0.6)')
            .opacity(this.radianceFlash)
            .hitTestBehavior(HitTestMode.Transparent)
        }
        .width(48)
        .height(48)
        .position({ x: 16, y: 122 })

        // 闪避：普攻左侧。
        Stack() {
          Button('闪避')
            .width(56)
            .height(56)
            .type(ButtonType.Circle)
            .backgroundColor('#274043')
            .linearGradient({ angle: 135, colors: [['#3A5A5E', 0.0], ['#274043', 0.5], ['#18282A', 1.0]] })
            .border({ width: 1.5, color: '#3E6B62' })
            .fontColor('#9FD8CB')
            .fontSize(14)
            .fontWeight(FontWeight.Medium)
            .shadow({ radius: 8, color: '#40000000', offsetX: 0, offsetY: 2 })
            .scale({ x: this.dodgePressed ? 0.9 : 1.0, y: this.dodgePressed ? 0.9 : 1.0 })
            .animation({ duration: 120, curve: Curve.EaseOut })
            .hitTestBehavior(HitTestMode.Block)
            .onTouch((e: TouchEvent) => {
              if (e.type === TouchType.Down) this.dodgePressed = true;
              if (e.type === TouchType.Up || e.type === TouchType.Cancel) this.dodgePressed = false;
            })
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
            .type(ButtonType.Circle)
            .backgroundColor('#2F4858')
            .linearGradient({ angle: 135, colors: [['#4A6A7A', 0.0], ['#2F4858', 0.5], ['#1A2E38', 1.0]] })
            .border({ width: 2, color: '#5B8FA8' })
            .fontColor('#D6E2DF')
            .fontSize(18)
            .fontWeight(FontWeight.Bold)
            .shadow({ radius: 12, color: '#505B8FA8', offsetX: 0, offsetY: 3 })
            .scale({ x: this.attackPressed ? 0.9 : 1.0, y: this.attackPressed ? 0.9 : 1.0 })
            .animation({ duration: 120, curve: Curve.EaseOut })
            .hitTestBehavior(HitTestMode.Block)
            .onTouch((e: TouchEvent) => {
              if (e.type === TouchType.Down) this.attackPressed = true;
              if (e.type === TouchType.Up || e.type === TouchType.Cancel) this.attackPressed = false;
            })
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
        .type(ButtonType.Circle)
        .backgroundColor('rgba(20, 26, 38, 0.55)')
        .border({ width: 1, color: 'rgba(199, 208, 206, 0.35)' })
        .fontColor('#C7D0CE')
        .fontSize(15)
        .stateEffect(true)
        .position({ x: '100%', y: 14 })
        .markAnchor({ x: 58, y: 0 })
        .hitTestBehavior(HitTestMode.Block)
        .onClick(() => {
          this.debugOpen = !this.debugOpen;
        })

      // 调试面板：默认收起，避免调试按钮裸露在游戏画面。
      if (this.debugOpen) {
        Column({ space: 6 }) {
          Row({ space: 4 }) {
            Button('训练').width(36).height(28).fontSize(11).type(ButtonType.Normal)
              .borderRadius(8).backgroundColor('#2A3441').fontColor('#C7D0CE').stateEffect(true)
              .hitTestBehavior(HitTestMode.Block)
              .onClick(() => { startEncounter(0); })
            Button('兽群').width(36).height(28).fontSize(11).type(ButtonType.Normal)
              .borderRadius(8).backgroundColor('#2A3441').fontColor('#C7D0CE').stateEffect(true)
              .hitTestBehavior(HitTestMode.Block)
              .onClick(() => { startEncounter(1); })
            Button('混战').width(36).height(28).fontSize(11).type(ButtonType.Normal)
              .borderRadius(8).backgroundColor('#2A3441').fontColor('#C7D0CE').stateEffect(true)
              .hitTestBehavior(HitTestMode.Block)
              .onClick(() => { startEncounter(2); })
            Button('守卫').width(36).height(28).fontSize(11).type(ButtonType.Normal)
              .borderRadius(8).backgroundColor('#2A3441').fontColor('#C7D0CE').stateEffect(true)
              .hitTestBehavior(HitTestMode.Block)
              .onClick(() => { startEncounter(3); })
          }
          Row({ space: 4 }) {
            Button('流程').width(36).height(28).fontSize(11).type(ButtonType.Normal)
              .borderRadius(8).backgroundColor('#2A3441').fontColor('#C7D0CE').stateEffect(true)
              .hitTestBehavior(HitTestMode.Block)
              .onClick(() => { startEncounter(4); })
            Button('首领').width(36).height(28).fontSize(11).type(ButtonType.Normal)
              .borderRadius(8).backgroundColor('#2A3441').fontColor('#C7D0CE').stateEffect(true)
              .hitTestBehavior(HitTestMode.Block)
              .onClick(() => { startEncounter(5); })
            Button('推进').width(36).height(28).fontSize(11).type(ButtonType.Normal)
              .borderRadius(8).backgroundColor('#2A3441').fontColor('#C7D0CE').stateEffect(true)
              .hitTestBehavior(HitTestMode.Block)
              .onClick(() => { advanceLevel(); })
          }
          Row({ space: 4 }) {
            Button('补给').width(36).height(28).fontSize(11).type(ButtonType.Normal)
              .borderRadius(8).backgroundColor('#2A3441').fontColor('#C7D0CE').stateEffect(true)
              .hitTestBehavior(HitTestMode.Block)
              .onClick(() => { useSupply(); })
            Button('重试').width(36).height(28).fontSize(11).type(ButtonType.Normal)
              .borderRadius(8).backgroundColor('#2A3441').fontColor('#C7D0CE').stateEffect(true)
              .hitTestBehavior(HitTestMode.Block)
              .onClick(() => { retryBoss(); })
            Button('调试').width(36).height(28).fontSize(11).type(ButtonType.Normal)
              .borderRadius(8).backgroundColor('#2A3441').fontColor('#C7D0CE').stateEffect(true)
              .hitTestBehavior(HitTestMode.Block)
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
    .hitTestBehavior(HitTestMode.None)
  }
}

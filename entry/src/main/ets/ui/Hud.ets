@Component
export struct Hud {
  @Prop @Watch('onHpChanged') hp: number = 100;
  @Prop poise: number = 100;
  @Prop x: number = 0.5;
  @Prop y: number = 0.5;
  @Prop fps: number = 0;
  @Prop moving: boolean = false;
  @Prop moveX: number = 0;
  @Prop moveY: number = 0;
  @Prop cameraYaw: number = 0;
  @Prop targetDist: number = 0;
  @Prop @Watch('onStaminaChanged') stamina: number = 100;
  @Prop comboSegment: number = 0;
  @Prop invulnerable: boolean = false;
  @Prop insightMs: number = 0;
  @Prop resonance: number = 0;
  @Prop targetHp: number = 300;
  @Prop targetPoise: number = 100;
  @Prop pulseHitRemainingMs: number = 0;
  @Prop lastRejectReason: number = 0;
  @Prop encounterMode: number = 0;
  @Prop encounterState: number = 0;
  @Prop currentAction: number = 0;
  @Prop comboWindowMs: number = 0;
  @Prop radianceCooldownMs: number = 0;
  @Prop currentCooldownMs: number = 0;
  @Prop corruptionCooldownMs: number = 0;
  @Prop ultimateWindowMs: number = 0;
  @Prop targetPoiseBroken: boolean = false;
  @Prop radianceAttached: boolean = false;
  @Prop currentAttached: boolean = false;
  @Prop corruptionAttached: boolean = false;
  @Prop corroded: boolean = false;
  @Prop currentReaction: number = -1;
  @Prop pulsePhase: number = 0;
  @Prop levelStage: number = 0;
  @Prop gateState: number = 0;
  @Prop supplyState: number = 0;
  @Prop bossHp: number = 1000;
  @Prop bossPoise: number = 300;
  @Prop bossMechanic: number = 0;
  @Prop bossCastMs: number = 0;
  @Prop bossPhase: number = 0;
  @Prop debugHud: boolean = false;
  @Prop perfLevel: number = 0;
  @Prop vfxFlags: number = 0;
  @Prop bossHpRatio: number = 0;
  @Prop bossCastRatio: number = 0;
  @Prop objectiveLabel: string = '';
  @Prop @Watch('onSlotsChanged') resonanceSlots: number[] = [0, 0, 0];
  @Prop showDebugHud: boolean = false;
  @Prop bossCinematicProgress: number = 0;
  @Prop bossShardCount: number = 0;
  @Prop bossSourceColor: number = 0;
  @Prop bossRingBroken: boolean = false;
  @Prop inputEventCount: number = 0;
  @State auras: string[] = [];
  @State hpFlash: number = 0;
  @State staminaFlash: number = 0;
  @State slotPulse: number[] = [0, 0, 0];
  private prevHp: number = 100;
  private prevStamina: number = 100;
  private prevSlots: number[] = [0, 0, 0];

  aboutToAppear(): void {
    this.prevHp = this.hp;
    this.prevStamina = this.stamina;
    this.prevSlots = [...this.resonanceSlots];
  }

  onHpChanged(): void {
    if (this.hp < this.prevHp) {
      this.hpFlash = 1;
      animateTo({ duration: 350, curve: Curve.EaseOut }, () => {
        this.hpFlash = 0;
      });
    }
    this.prevHp = this.hp;
  }

  onStaminaChanged(): void {
    if (this.stamina !== this.prevStamina) {
      this.staminaFlash = 1;
      animateTo({ duration: 300, curve: Curve.EaseOut }, () => {
        this.staminaFlash = 0;
      });
    }
    this.prevStamina = this.stamina;
  }

  onSlotsChanged(): void {
    for (let i = 0; i < 3; i++) {
      if (this.resonanceSlots[i] > 0 && this.prevSlots[i] <= 0) {
        this.slotPulse[i] = 1;
        animateTo({ duration: 500, curve: Curve.EaseOut }, () => {
          this.slotPulse[i] = 0;
        });
      }
    }
    this.prevSlots = [...this.resonanceSlots];
  }

  build() {
    Column() {
      Text(this.objectiveLabel)
        .fontColor('#E5ECEA')
        .fontSize(16)
        .fontWeight(FontWeight.Medium)
        .letterSpacing(1.5)
        .textShadow({ radius: 8, color: '#B0000000', offsetX: 0, offsetY: 2 })
        .margin({ top: 18 })

      if (this.encounterMode == 5) {
        Column() {
          Text('共鸣核心祭司')
            .fontColor('#CBD5D3').fontSize(11).letterSpacing(2)
            .textShadow({ radius: 6, color: '#80AA4F86', offsetX: 0, offsetY: 1 })
            .margin({ bottom: 4 })
          Progress({ value: this.bossHpRatio * 100, total: 100, type: ProgressType.Linear })
            .width('54%').height(7)
            .color('#AA4F86')
            .backgroundColor('rgba(20, 10, 18, 0.6)')
            .animation({ duration: 120, curve: Curve.EaseOut })
        }
        .alignItems(HorizontalAlign.Center)
        .width('100%')
        .padding({ top: 8, bottom: 8 })
        .margin({ top: 10 })
        .backgroundColor('rgba(15, 8, 14, 0.45)')
        .borderRadius(12)
        .border({ width: 1, color: 'rgba(170, 79, 134, 0.3)' })
      }

      Blank().layoutWeight(1)

      Column({ space: 5 }) {
        // HP 条：♥ 图标 + 进度条 + 受击闪烁叠加层。
        Row({ space: 6 }) {
          Text('♥')
            .fontSize(13)
            .fontColor(this.hpFlash > 0.3 ? '#E06A5E' : '#D6E2DF')
            .textShadow({ radius: 6, color: this.hpFlash > 0.3 ? '#60E06A5E' : '#30D6E2DF',
              offsetX: 0, offsetY: 0 })
            .animation({ duration: 200, curve: Curve.EaseOut })
          Stack() {
            Progress({ value: this.hp, total: 100, type: ProgressType.Linear })
              .width(150).height(5).color('#D6E2DF')
              .backgroundColor('rgba(20, 26, 30, 0.5)')
              .animation({ duration: 120, curve: Curve.EaseOut })
            Stack()
              .width(150).height(5)
              .backgroundColor('#E06A5E')
              .opacity(this.hpFlash * 0.5)
              .borderRadius(3)
          }
        }
        .alignItems(VerticalAlign.Center)

        // 韧性条：◆ 图标 + 进度条。
        Row({ space: 6 }) {
          Text('◆')
            .fontSize(10)
            .fontColor('#D9A145')
            .textShadow({ radius: 4, color: '#30D9A145', offsetX: 0, offsetY: 0 })
          Progress({ value: this.poise, total: 100, type: ProgressType.Linear })
            .width(134).height(2).color('#D9A145')
            .backgroundColor('rgba(20, 26, 30, 0.4)')
            .animation({ duration: 120, curve: Curve.EaseOut })
        }
        .alignItems(VerticalAlign.Center)

        // 分隔线。
        Stack()
          .width(100).height(0.5)
          .backgroundColor('rgba(199, 208, 206, 0.08)')
          .margin({ top: 1, bottom: 1 })

        // 体力条：⚡ 图标 + 进度条 + 变化闪烁。
        Row({ space: 6 }) {
          Text('⚡')
            .fontSize(11)
            .fontColor(this.stamina < 30 ? '#E06A5E' : '#4FD4BB')
            .textShadow({ radius: 4,
              color: this.stamina < 30 ? '#40E06A5E' : '#304FD4BB',
              offsetX: 0, offsetY: 0 })
            .animation({ duration: 200, curve: Curve.EaseOut })
          Stack() {
            Progress({ value: this.stamina, total: 100, type: ProgressType.Linear })
              .width(118).height(3)
              .color(this.stamina < 30 ? '#E06A5E' : '#4FD4BB')
              .backgroundColor('rgba(20, 26, 30, 0.4)')
              .animation({ duration: 150, curve: Curve.EaseOut })
            Stack()
              .width(118).height(3)
              .backgroundColor(this.stamina < 30 ? '#E06A5E' : '#4FD4BB')
              .opacity(this.staminaFlash * 0.4)
              .borderRadius(2)
          }
        }
        .alignItems(VerticalAlign.Center)

        // 三源共鸣槽位：发光圆球指示器，激活时脉冲扩散。
        Row({ space: 14 }) {
          // 辉（Radiance）金色。
          Stack() {
            Circle()
              .width(16 + this.slotPulse[0] * 14)
              .height(16 + this.slotPulse[0] * 14)
              .fill('transparent')
              .border({ width: 1.5, color: `rgba(217, 161, 69, ${0.5 * (1 - this.slotPulse[0])})` })
              .opacity(this.resonanceSlots[0] > 0 ? 1 : 0)
            Circle()
              .width(16)
              .height(16)
              .fill(this.resonanceSlots[0] > 0 ? '#D9A145' : '#3A3E3D')
              .shadow(this.resonanceSlots[0] > 0
                ? { radius: 10 + this.slotPulse[0] * 8, color: '#80D9A145', offsetX: 0, offsetY: 0 }
                : { radius: 2, color: '#20000000', offsetX: 0, offsetY: 1 })
              .animation({ duration: 200, curve: Curve.EaseOut })
            if (this.resonanceSlots[0] > 0) {
              Circle()
                .width(8)
                .height(8)
                .fill('#F2E0B8')
            }
          }
          .width(16)
          .height(16)
          // 流（Flow）青色。
          Stack() {
            Circle()
              .width(16 + this.slotPulse[1] * 14)
              .height(16 + this.slotPulse[1] * 14)
              .fill('transparent')
              .border({ width: 1.5, color: `rgba(67, 205, 181, ${0.5 * (1 - this.slotPulse[1])})` })
              .opacity(this.resonanceSlots[1] > 0 ? 1 : 0)
            Circle()
              .width(16)
              .height(16)
              .fill(this.resonanceSlots[1] > 0 ? '#43CDB5' : '#3A3E3D')
              .shadow(this.resonanceSlots[1] > 0
                ? { radius: 10 + this.slotPulse[1] * 8, color: '#8043CDB5', offsetX: 0, offsetY: 0 }
                : { radius: 2, color: '#20000000', offsetX: 0, offsetY: 1 })
              .animation({ duration: 200, curve: Curve.EaseOut })
            if (this.resonanceSlots[1] > 0) {
              Circle()
                .width(8)
                .height(8)
                .fill('#B8F2E6')
            }
          }
          .width(16)
          .height(16)
          // 蚀（Corruption）紫色。
          Stack() {
            Circle()
              .width(16 + this.slotPulse[2] * 14)
              .height(16 + this.slotPulse[2] * 14)
              .fill('transparent')
              .border({ width: 1.5, color: `rgba(171, 79, 146, ${0.5 * (1 - this.slotPulse[2])})` })
              .opacity(this.resonanceSlots[2] > 0 ? 1 : 0)
            Circle()
              .width(16)
              .height(16)
              .fill(this.resonanceSlots[2] > 0 ? '#AB4F92' : '#3A3E3D')
              .shadow(this.resonanceSlots[2] > 0
                ? { radius: 10 + this.slotPulse[2] * 8, color: '#80AB4F92', offsetX: 0, offsetY: 0 }
                : { radius: 2, color: '#20000000', offsetX: 0, offsetY: 1 })
              .animation({ duration: 200, curve: Curve.EaseOut })
            if (this.resonanceSlots[2] > 0) {
              Circle()
                .width(8)
                .height(8)
                .fill('#F0C4E4')
            }
          }
          .width(16)
          .height(16)
        }
        .margin({ top: 2 })
      }
      .alignItems(HorizontalAlign.Center)
      .padding({ left: 16, right: 16, top: 8, bottom: 8 })
      .backgroundColor('#8A11181D')
        .borderRadius(18)
        .border({ width: 1, color: 'rgba(199, 208, 206, 0.15)' })
        .margin({ bottom: 22 })

      if (this.showDebugHud) {
        if (this.debugHud) {
          Column() {
          Text(`FPS ${this.fps.toFixed(1)}`)
            .fontColor(Color.Yellow).fontSize(12)
          Text(`X ${this.x.toFixed(3)}  Y ${this.y.toFixed(3)} · ${this.moving ? '移动中' : '待机'} · 目标 ${(this.targetDist * 100).toFixed(1)}%`)
            .fontColor(Color.Gray).fontSize(10)
          Text(`输入 ${this.moveX.toFixed(2)}, ${this.moveY.toFixed(2)} · 相机 yaw ${this.cameraYaw.toFixed(2)}`)
            .fontColor(Color.Yellow).fontSize(10)
          Text(`遭遇 ${this.encounterMode} · 状态 ${this.encounterState} · 性能 ${this.perfLevel} · VFX ${this.vfxFlags}`)
            .fontColor(Color.Gray).fontSize(10)
          Text(`关卡 ${this.levelStage} · 门 ${this.gateState} · 补给 ${this.supplyState}`)
            .fontColor(Color.Gray).fontSize(10)
          Text(`首领 阶段 ${this.bossPhase} · 机制 ${this.bossMechanic} · 读条 ${this.bossCastMs}ms`)
            .fontColor(Color.Gray).fontSize(10)
          Text(`假人 HP ${this.targetHp.toFixed(1)} · 韧性 ${this.targetPoise.toFixed(1)}`)
            .fontColor(Color.White).fontSize(10)
          Text(`脉冲 ${this.pulseHitRemainingMs}ms · 无敌 ${this.invulnerable ? '是' : '否'} · 拒绝 ${this.lastRejectReason}`)
            .fontColor(this.pulseHitRemainingMs >= 100 && this.pulseHitRemainingMs <= 500 ? Color.Red : Color.Gray)
            .fontSize(10)
          Text(`动作 ${this.currentAction} · 连击窗 ${this.comboWindowMs}ms · 终结窗 ${this.ultimateWindowMs}ms`)
            .fontColor(Color.Gray).fontSize(10)
          Text(`冷却 ${this.radianceCooldownMs}/${this.currentCooldownMs}/${this.corruptionCooldownMs}ms`)
            .fontColor(Color.Gray).fontSize(10)
          Text(`精准窗 ${this.pulseHitRemainingMs >= 100 && this.pulseHitRemainingMs <= 500 ? '开启' : '关闭'}`)
            .fontColor(Color.Gray).fontSize(10)
          Text(`侵蚀 ${this.corroded ? '是' : '否'} · 反应 ${this.currentReaction} · 相位 ${this.pulsePhase}`)
            .fontColor(Color.Gray).fontSize(10)
          Text(`输入事件 ${this.inputEventCount}`)
            .fontColor(Color.Yellow).fontSize(10)
          }
          .alignItems(HorizontalAlign.Start)
          .position({ x: 12, y: 70 })
        }
      }
    }
    .alignItems(HorizontalAlign.Center)
    .position({ x: 0, y: 0 })
    .width('100%')
    .height('100%')
    .hitTestBehavior(HitTestMode.Transparent)
  }
}

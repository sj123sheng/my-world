@Component
export struct Hud {
  @Prop hp: number = 100;
  @Prop poise: number = 100;
  @Prop x: number = 0.5;
  @Prop y: number = 0.5;
  @Prop fps: number = 0;
  @Prop moving: boolean = false;
  @Prop targetDist: number = 0;
  @Prop stamina: number = 100;
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
  @Prop resonanceSlots: number[] = [0, 0, 0];
  @Prop showDebugHud: boolean = false;
  @Prop bossCinematicProgress: number = 0;
  @Prop bossShardCount: number = 0;
  @Prop bossSourceColor: number = 0;
  @Prop bossRingBroken: boolean = false;
  @Prop inputEventCount: number = 0;
  @State auras: string[] = [];

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
          Progress({ value: this.bossHpRatio * 100, total: 100, type: ProgressType.Linear })
            .width('54%').height(6)
            .color('#AA4F86')
          Text('共鸣核心祭司')
            .fontColor('#CBD5D3').fontSize(11).letterSpacing(2)
        }
        .alignItems(HorizontalAlign.Center)
        .width('100%')
        .margin({ top: 10 })
      }

      Blank().layoutWeight(1)

      Column({ space: 5 }) {
        Progress({ value: this.hp, total: 100, type: ProgressType.Linear })
          .width(150).height(5).color('#D6E2DF')
        Progress({ value: this.poise, total: 100, type: ProgressType.Linear })
          .width(134).height(2).color('#D9A145')
        Progress({ value: this.stamina, total: 100, type: ProgressType.Linear })
          .width(118).height(3).color('#4FD4BB')
        Row({ space: 12 }) {
          Text('辉').fontSize(14)
            .fontColor(this.resonanceSlots[0] > 0 ? '#D9A145' : '#66706F')
          Text('流').fontSize(14)
            .fontColor(this.resonanceSlots[1] > 0 ? '#43CDB5' : '#66706F')
          Text('蚀').fontSize(14)
            .fontColor(this.resonanceSlots[2] > 0 ? '#AB4F92' : '#66706F')
        }
      }
      .alignItems(HorizontalAlign.Center)
      .padding({ left: 16, right: 16, top: 8, bottom: 8 })
      .backgroundColor('#8A11181D')
        .borderRadius(18)
        .margin({ bottom: 22 })

      if (this.showDebugHud) {
        if (this.debugHud) {
          Column() {
          Text(`FPS ${this.fps.toFixed(1)}`)
            .fontColor(Color.Yellow).fontSize(12)
          Text(`X ${this.x.toFixed(3)}  Y ${this.y.toFixed(3)} · ${this.moving ? '移动中' : '待机'} · 目标 ${(this.targetDist * 100).toFixed(1)}%`)
            .fontColor(Color.Gray).fontSize(10)
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

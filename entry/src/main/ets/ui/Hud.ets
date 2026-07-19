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
  @State auras: string[] = [];

  build() {
    Column() {
      Row() {
        Column() {
          Progress({ value: this.hp, total: 100, type: ProgressType.Linear })
            .width('40%').height(8)
            .color(Color.Red)
          Progress({ value: this.poise, total: 100, type: ProgressType.Linear })
            .width('40%').height(6)
            .color(Color.Orange)
          Progress({ value: this.stamina, total: 100, type: ProgressType.Linear })
            .width('40%').height(6)
            .color(Color.Green)
          Text(`连击 ${this.comboSegment} · 洞察 ${this.insightMs}ms`)
            .fontColor(this.insightMs > 0 ? Color.Yellow : Color.Gray)
            .fontSize(12)
          Text(`附着 ${this.radianceAttached ? '辉' : '-'}${this.currentAttached ? '流' : '-'}${this.corruptionAttached ? '蚀' : '-'}`)
            .fontColor(this.targetPoiseBroken ? Color.Red : Color.Gray).fontSize(12)
        }
        .alignItems(HorizontalAlign.Start)
        .layoutWeight(1)

        Column() {
          Text(`FPS ${this.fps.toFixed(1)}`)
            .fontColor(Color.Yellow)
            .fontSize(14)
        }
        .alignItems(HorizontalAlign.End)
      }
      .width('100%')
      .justifyContent(FlexAlign.SpaceBetween)

      if (this.encounterMode == 5) {
        Column() {
          Progress({ value: this.bossHpRatio * 100, total: 100, type: ProgressType.Linear })
            .width('60%').height(10)
            .color(Color.Red)
          Text(`首领 阶段 ${this.bossPhase} · 机制 ${this.bossMechanic} · 读条 ${this.bossCastMs}ms`)
            .fontColor(Color.White).fontSize(12)
        }
        .alignItems(HorizontalAlign.Center)
        .width('100%')
        .margin({ top: 4 })
      }

      if (this.encounterMode == 4) {
        Text(`关卡 ${this.levelStage} · 门 ${this.gateState} · 补给 ${this.supplyState}`)
          .fontColor(Color.Gray).fontSize(12)
          .margin({ top: 4 })
      }

      if (this.debugHud) {
        Column() {
          Text(`X ${this.x.toFixed(3)}  Y ${this.y.toFixed(3)} · ${this.moving ? '移动中' : '待机'} · 目标 ${(this.targetDist * 100).toFixed(1)}%`)
            .fontColor(Color.Gray).fontSize(10)
          Text(`遭遇 ${this.encounterMode} · 状态 ${this.encounterState} · 性能 ${this.perfLevel} · VFX ${this.vfxFlags}`)
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
          Text(`侵蚀 ${this.corroded ? '是' : '否'} · 反应 ${this.currentReaction} · 相位 ${this.pulsePhase}`)
            .fontColor(Color.Gray).fontSize(10)
        }
        .alignItems(HorizontalAlign.Start)
      }

      ForEach(this.auras, (a: string) => {
        Text(a)
          .fontColor(Color.Yellow)
          .fontSize(14)
      }, (a: string) => a)
    }
    .alignItems(HorizontalAlign.Start)
    .padding(16)
    .position({ x: 0, y: 0 })
    .width('100%')
  }
}

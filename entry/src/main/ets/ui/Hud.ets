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
  @State auras: string[] = [];

  build() {
    Column() {
      Row() {
        Column() {
          Text(`HP ${this.hp}`)
            .fontColor(Color.White)
            .fontSize(18)
            .fontWeight(FontWeight.Bold)
          Text(`韧性 ${this.poise}`)
            .fontColor(Color.White)
            .fontSize(18)
            .fontWeight(FontWeight.Bold)
          Text(`${this.moving ? '移动中' : '待机'} · 目标 ${(this.targetDist * 100).toFixed(1)}%`)
            .fontColor(this.moving ? Color.Green : Color.Gray)
            .fontSize(12)
          Text(`体力 ${this.stamina.toFixed(1)} · 连击 ${this.comboSegment}`)
            .fontColor(Color.White)
            .fontSize(12)
          Text(`洞察 ${this.insightMs}ms · 共鸣 ${this.resonance.toFixed(1)}`)
            .fontColor(this.insightMs > 0 ? Color.Yellow : Color.Gray)
            .fontSize(12)
          Text(`假人 HP ${this.targetHp.toFixed(1)} · 韧性 ${this.targetPoise.toFixed(1)}`)
            .fontColor(Color.White)
            .fontSize(12)
          Text(`脉冲 ${this.pulseHitRemainingMs}ms · 无敌 ${this.invulnerable ? '是' : '否'} · 拒绝 ${this.lastRejectReason}`)
            .fontColor(this.pulseHitRemainingMs >= 100 && this.pulseHitRemainingMs <= 500 ? Color.Red : Color.Gray)
            .fontSize(12)
          Text(`遭遇 ${this.encounterMode} · 状态 ${this.encounterState}`)
            .fontColor(Color.Gray).fontSize(12)
          Text(`动作 ${this.currentAction} · 连击窗 ${this.comboWindowMs}ms · 终结窗 ${this.ultimateWindowMs}ms`)
            .fontColor(Color.Gray).fontSize(12)
          Text(`冷却 ${this.radianceCooldownMs}/${this.currentCooldownMs}/${this.corruptionCooldownMs}ms`)
            .fontColor(Color.Gray).fontSize(12)
          Text(`附着 ${this.radianceAttached ? '辉' : '-'}${this.currentAttached ? '流' : '-'}${this.corruptionAttached ? '蚀' : '-'} · 侵蚀 ${this.corroded ? '是' : '否'} · 反应 ${this.currentReaction} · 相位 ${this.pulsePhase}`)
            .fontColor(this.targetPoiseBroken ? Color.Red : Color.Gray).fontSize(12)
        }
        .alignItems(HorizontalAlign.Start)
        .layoutWeight(1)

        Column() {
          Text(`FPS ${this.fps.toFixed(1)}`)
            .fontColor(Color.Yellow)
            .fontSize(14)
          Text(`X ${this.x.toFixed(3)}  Y ${this.y.toFixed(3)}`)
            .fontColor(Color.Gray)
            .fontSize(12)
        }
        .alignItems(HorizontalAlign.End)
      }
      .width('100%')
      .justifyContent(FlexAlign.SpaceBetween)

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

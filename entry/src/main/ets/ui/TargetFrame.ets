// 锁定目标焦点框：软瞄准锁定普通敌人时，屏幕上方显示其名称与血条，
// 与敌人头顶血条、锁定环共同构成完整的目标反馈。首领已有专属血条，
// archetype 为 -1 时不显示。
@Component
export struct TargetFrame {
  @Prop targetId: number = 0;
  @Prop targetArchetype: number = -1;
  @Prop targetHpRatio: number = 0;

  private archetypeName(): string {
    switch (this.targetArchetype) {
      case 0: return '裂隙之爪';
      case 1: return '辉光祭司';
      case 2: return '蚀壳守卫';
      default: return '';
    }
  }

  private hpColor(): string {
    if (this.targetHpRatio > 0.5) return '#4FD4BB';
    if (this.targetHpRatio > 0.25) return '#D9A145';
    return '#E06A5E';
  }

  private hpGlow(): string {
    if (this.targetHpRatio > 0.5) return '#304FD4BB';
    if (this.targetHpRatio > 0.25) return '#30D9A145';
    return '#30E06A5E';
  }

  build() {
    Stack({ alignContent: Alignment.Top }) {
      if (this.targetId > 0 && this.targetArchetype >= 0) {
        Column({ space: 6 }) {
          Text(this.archetypeName())
            .fontSize(13)
            .fontColor('#E5ECEA')
            .fontWeight(FontWeight.Medium)
            .letterSpacing(2)
            .textShadow({ radius: 8, color: '#B0000000', offsetX: 0, offsetY: 1 })
          Progress({ value: this.targetHpRatio * 100, total: 100,
            type: ProgressType.Linear })
            .width(180)
            .height(5)
            .color(this.hpColor())
            .backgroundColor('rgba(20, 26, 38, 0.6)')
            .animation({ duration: 120, curve: Curve.EaseOut })
        }
        .alignItems(HorizontalAlign.Center)
        .padding({ left: 18, right: 18, top: 8, bottom: 10 })
        .backgroundColor('#8A11181D')
        .borderRadius(14)
        .border({ width: 1, color: 'rgba(199, 208, 206, 0.25)' })
        .shadow({ radius: 8, color: this.hpGlow(), offsetX: 0, offsetY: 2 })
        .margin({ top: 44 })
        .transition({
          type: TransitionType.All,
          opacity: 0.0,
          scale: { x: 0.9, y: 0.9 }
        })
      }
    }
    .width('100%')
    .height('100%')
    .hitTestBehavior(HitTestMode.Transparent)
  }
}

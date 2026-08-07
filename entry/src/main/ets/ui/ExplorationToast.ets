// 探索反馈统一提示：地标发现、机关激活、路径门开启和奖励领取。
@Component
export struct ExplorationToast {
  @Prop explorationFeedbackType: number = 0;
  @Prop explorationFeedbackTitle: string = '';
  @Prop explorationFeedbackSubtitle: string = '';
  @Prop explorationFeedbackRemainingMs: number = 0;

  private kindLabel(): string {
    switch (this.explorationFeedbackType) {
      case 1: return '地标';
      case 2: return '机关';
      case 3: return '路径';
      case 4: return '奖励';
      default: return '';
    }
  }

  private accentColor(): string {
    switch (this.explorationFeedbackType) {
      case 1: return '#9FD8CB';
      case 2: return '#4FD4BB';
      case 3: return '#E5B84A';
      case 4: return '#F2E0B8';
      default: return '#CBD5D3';
    }
  }

  build() {
    if (this.explorationFeedbackType > 0 &&
        this.explorationFeedbackRemainingMs > 0) {
      Column({ space: 2 }) {
        Text(`${this.kindLabel()} · ${this.explorationFeedbackTitle}`)
          .fontSize(12)
          .fontWeight(FontWeight.Bold)
          .fontColor(this.accentColor())
        if (this.explorationFeedbackSubtitle.length > 0) {
          Text(this.explorationFeedbackSubtitle)
            .fontSize(9)
            .fontColor('rgba(199, 208, 206, 0.82)')
        }
      }
      .alignItems(HorizontalAlign.Start)
      .padding({ left: 8, right: 8, top: 6, bottom: 6 })
      .backgroundColor('rgba(10, 14, 22, 0.62)')
      .borderRadius(8)
      .position({ x: '50%', y: 96 })
      .markAnchor({ x: '50%', y: 0 })
      .hitTestBehavior(HitTestMode.None)
    }
  }
}

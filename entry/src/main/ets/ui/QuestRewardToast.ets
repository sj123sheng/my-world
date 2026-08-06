// 任务完成奖励浮层（内容反馈闭环）：
// completedQuestCount 上升沿时由 GamePage 触发，短暂展示后自动淡出。
@Component
export struct QuestRewardToast {
  @Prop visible: boolean = false;
  @Prop questTitle: string = '';
  @Prop rewardLabel: string = '';

  build() {
    if (this.visible) {
      Column({ space: 6 }) {
        Text('任务完成')
          .fontSize(16)
          .fontWeight(FontWeight.Bold)
          .fontColor('#F2E0B8')
        if (this.questTitle !== '') {
          Text(this.questTitle)
            .fontSize(13)
            .fontColor('#E5ECEA')
        }
        Text(this.rewardLabel)
          .fontSize(12)
          .fontColor('#B8F2E6')
      }
      .padding({ left: 24, right: 24, top: 14, bottom: 14 })
      .backgroundColor('rgba(8, 12, 20, 0.9)')
      .borderRadius(14)
      .border({ width: 1, color: 'rgba(229, 184, 74, 0.5)' })
      .position({ x: '50%', y: '30%' })
      .markAnchor({ x: '50%', y: '50%' })
      .hitTestBehavior(HitTestMode.Transparent)
    }
  }
}

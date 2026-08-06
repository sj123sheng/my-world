import { advanceDialog } from '../napi/Bridge';

// 对话框（阶段二）：底部显示说话人与台词，按钮推进会话。
// 样式避开 linearGradient/mask 以规避已知 ArkTS 兼容性问题。
@Component
export struct DialogBox {
  @Prop dialogActive: boolean = false;
  @Prop dialogSpeaker: string = '';
  @Prop dialogText: string = '';
  @Prop dialogLineIndex: number = 0;
  @Prop dialogLineCount: number = 0;

  build() {
    if (this.dialogActive) {
      Column({ space: 10 }) {
        Row() {
          Text(this.dialogSpeaker)
            .fontSize(13)
            .fontWeight(FontWeight.Bold)
            .fontColor('#F2E0B8')
            .backgroundColor('rgba(122, 90, 34, 0.75)')
            .borderRadius(10)
            .padding({ left: 10, right: 10, top: 3, bottom: 3 })
        }
        .width('100%')

        Text(this.dialogText)
          .fontSize(15)
          .fontColor('#E5ECEA')
          .width('100%')

        Row() {
          Text(`${this.dialogLineIndex + 1}/${this.dialogLineCount}`)
            .fontSize(11)
            .fontColor('rgba(199, 208, 206, 0.7)')
          Blank()
          Button('继续')
            .width(72)
            .height(32)
            .fontSize(13)
            .type(ButtonType.Normal)
            .borderRadius(16)
            .backgroundColor('#1F5A50')
            .fontColor('#FFFFFF')
            .fontWeight(FontWeight.Bold)
            .stateEffect(true)
            .hitTestBehavior(HitTestMode.Block)
            .onClick(() => { advanceDialog(); })
        }
        .width('100%')
      }
      .width('86%')
      .padding(16)
      .backgroundColor('rgba(10, 14, 22, 0.85)')
      .borderRadius(16)
      .border({ width: 1, color: 'rgba(229, 184, 74, 0.4)' })
      .position({ x: '50%', y: '100%' })
      .markAnchor({ x: '50%', y: 190 })
    }
  }
}

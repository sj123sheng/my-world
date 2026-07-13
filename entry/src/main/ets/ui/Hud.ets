@Component
export struct Hud {
  @Prop hp: number = 100;
  @Prop poise: number = 100;
  @Prop x: number = 0.5;
  @Prop y: number = 0.5;
  @Prop fps: number = 0;
  @Prop moving: boolean = false;
  @Prop targetDist: number = 0;
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

@Component
export struct Hud {
  @State hp: number = 100;
  @State poise: number = 100;
  @State auras: string[] = [];

  build() {
    Column() {
      Text(`HP ${this.hp}`).fontColor(Color.White)
      Text(`韧性 ${this.poise}`).fontColor(Color.White)
      ForEach(this.auras, (a: string) => Text(a).fontColor(Color.Yellow), (a: string) => a)
    }.alignItems(HorizontalAlign.Start)
  }
}

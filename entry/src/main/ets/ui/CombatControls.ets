import { pushAction, startEncounter } from '../napi/Bridge';

@Component
export struct CombatControls {
  build() {
    Column({ space: 8 }) {
      Row({ space: 8 }) {
        Button('训练').onClick(() => { startEncounter(0); })
        Button('兽群').onClick(() => { startEncounter(1); })
        Button('混战').onClick(() => { startEncounter(2); })
        Button('守卫').onClick(() => { startEncounter(3); })
      }
      Row({ space: 8 }) {
        Button('普攻').onClick(() => { pushAction(0); })
        Button('闪避').onClick(() => { pushAction(1); })
      }
      Row({ space: 8 }) {
        Button('辉印').onClick(() => { pushAction(2); })
        Button('脉流').onClick(() => { pushAction(3); })
      }
      Row({ space: 8 }) {
        Button('蚀质').onClick(() => { pushAction(4); })
        Button('终结').onClick(() => { pushAction(5); })
      }
    }
    .padding(12)
    .margin({ right: 16, bottom: 16 })
    .alignItems(HorizontalAlign.End)
  }
}

// 动作拒绝反馈浮层：技能冷却/体力不足/共鸣不足等拒绝原因即时提示，
// 让玩家明白按键为何没有生效，避免无效操作困惑。
import { Haptics } from './Haptics';

const TOAST_DURATION_MS: number = 900;

@Component
export struct ActionToast {
  @Prop @Watch('onRejectChanged') lastRejectReason: number = 0;
  @State label: string = '';
  @State visible: boolean = false;
  private hideTimer: number = -1;

  aboutToDisappear(): void {
    if (this.hideTimer !== -1) {
      clearTimeout(this.hideTimer);
      this.hideTimer = -1;
    }
  }

  onRejectChanged(): void {
    if (this.lastRejectReason <= 0) {
      return;
    }
    this.label = ActionToast.labelFor(this.lastRejectReason);
    this.visible = true;
    // 拒绝轻振：与命中/受击振动区分，提示“按键未生效”。
    Haptics.light();
    if (this.hideTimer !== -1) {
      clearTimeout(this.hideTimer);
    }
    this.hideTimer = setTimeout((): void => {
      this.visible = false;
      this.hideTimer = -1;
    }, TOAST_DURATION_MS);
  }

  // 与 native ActionRejectReason 枚举一一对应。
  static labelFor(reason: number): string {
    switch (reason) {
      case 1: return '无目标';
      case 2: return '目标已死亡';
      case 3: return '技能冷却中';
      case 4: return '体力不足';
      case 5: return '共鸣不足';
      case 6: return '动作锁定';
      default: return '无效操作';
    }
  }

  build() {
    Stack({ alignContent: Alignment.Bottom }) {
      if (this.visible) {
        Text(this.label)
          .fontSize(14)
          .fontColor('#F2D9A8')
          .fontWeight(FontWeight.Medium)
          .letterSpacing(1.5)
          .padding({ left: 18, right: 18, top: 8, bottom: 8 })
          .backgroundColor('#CC11181D')
          .borderRadius(16)
          .border({ width: 1, color: 'rgba(217, 161, 69, 0.45)' })
          .shadow({ radius: 10, color: '#40000000', offsetX: 0, offsetY: 3 })
          .textShadow({ radius: 6, color: '#80000000', offsetX: 0, offsetY: 1 })
          .margin({ bottom: 196 })
          .transition({
            type: TransitionType.All,
            opacity: 0.0,
            translate: { y: 20 }
          })
      }
    }
    .width('100%')
    .height('100%')
    .hitTestBehavior(HitTestMode.Transparent)
  }
}

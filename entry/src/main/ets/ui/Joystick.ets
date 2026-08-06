// 仅负责摇杆视觉反馈；生产输入由下层 XComponent 的 native 回调统一处理。
// 摇杆固定在左下角区域：底盘常显，旋钮跟手移动，行程限制在底盘内。
// 旋钮视觉行程半径（vp）。
const KNOB_TRAVEL: number = 38;
const BASE_SIZE: number = 132;
const KNOB_SIZE: number = 52;
const TICK_SIZE: number = 6;
// 固定底盘相对屏幕左下角的边距（vp）。
const BASE_MARGIN_LEFT: number = 40;
const BASE_MARGIN_BOTTOM: number = 34;

@Component
export struct Joystick {
  @State active: boolean = false;
  // 旋钮相对底盘中心的偏移，随手指移动并在抬起时归零。
  @State knobOffsetX: number = 0;
  @State knobOffsetY: number = 0;
  private trackedPointer: number = -1;
  private originX: number = 0;
  private originY: number = 0;

  private handleTouch(event: TouchEvent): void {
    let touch: TouchObject | undefined = undefined;
    if (event.changedTouches !== undefined && event.changedTouches.length > 0) {
      touch = event.changedTouches[0];
    } else if (event.touches !== undefined && event.touches.length > 0) {
      touch = event.touches[0];
    }
    if (touch === undefined) {
      return;
    }

    if (event.type === TouchType.Down) {
      // 已跟踪一根手指时忽略后续落点，避免视觉状态被抢占。
      if (this.trackedPointer !== -1) {
        return;
      }
      this.trackedPointer = touch.id;
      this.originX = touch.x;
      this.originY = touch.y;
      this.knobOffsetX = 0;
      this.knobOffsetY = 0;
      this.active = true;
      return;
    }

    if (touch.id !== this.trackedPointer) {
      return;
    }

    if (event.type === TouchType.Move) {
      const dx: number = touch.x - this.originX;
      const dy: number = touch.y - this.originY;
      const length: number = Math.sqrt(dx * dx + dy * dy);
      let knobOffsetX: number = 0;
      let knobOffsetY: number = 0;
      if (length > 0) {
        const clamped: number = Math.min(length, KNOB_TRAVEL);
        const unitX: number = dx / length;
        const unitY: number = dy / length;
        knobOffsetX = unitX * clamped;
        knobOffsetY = unitY * clamped;
      }
      this.knobOffsetX = knobOffsetX;
      this.knobOffsetY = knobOffsetY;
      return;
    }

    // Up / Cancel：native XComponent 会结束输入，这里旋钮回中。
    this.trackedPointer = -1;
    this.knobOffsetX = 0;
    this.knobOffsetY = 0;
    this.active = false;
  }

  build() {
    Stack({ alignContent: Alignment.BottomStart }) {
      // 左半屏仅观察触摸以驱动摇杆视觉；透明命中让事件继续到 XComponent。
      // 与 native TouchRouter 的左半屏移动判定保持一致。
      Stack()
        .width('50%')
        .height('100%')
        .backgroundColor(Color.Transparent)
        .onTouch((event: TouchEvent) => {
          this.handleTouch(event);
        })

      // 左下角固定底盘：常显作移动锚点，激活时轻微提亮。
      Stack() {
        Circle({ width: TICK_SIZE, height: TICK_SIZE })
          .fill('rgba(214, 226, 223, 0.50)')
          .position({ x: BASE_SIZE / 2 - TICK_SIZE / 2, y: 9 })
        Circle({ width: TICK_SIZE, height: TICK_SIZE })
          .fill('rgba(214, 226, 223, 0.50)')
          .position({ x: BASE_SIZE / 2 - TICK_SIZE / 2, y: BASE_SIZE - 9 - TICK_SIZE })
        Circle({ width: TICK_SIZE, height: TICK_SIZE })
          .fill('rgba(214, 226, 223, 0.50)')
          .position({ x: 9, y: BASE_SIZE / 2 - TICK_SIZE / 2 })
        Circle({ width: TICK_SIZE, height: TICK_SIZE })
          .fill('rgba(214, 226, 223, 0.50)')
          .position({ x: BASE_SIZE - 9 - TICK_SIZE, y: BASE_SIZE / 2 - TICK_SIZE / 2 })
        // 中心十字准星，增强方向感。
        Circle({ width: 4, height: 4 })
          .fill('rgba(214, 226, 223, 0.25)')
          .position({ x: BASE_SIZE / 2 - 2, y: BASE_SIZE / 2 - 2 })

        // 旋钮：跟手移动，行程限制在 KNOB_TRAVEL 内，空闲时回中。
        Stack() {
          // 旋钮主体。
          Stack()
            .width(KNOB_SIZE)
            .height(KNOB_SIZE)
            .borderRadius(KNOB_SIZE / 2)
            .linearGradient({ angle: 135,
              colors: [['#A8F0E4', 0.0], ['#4DB8A8', 0.45], ['#2E8F81', 1.0]] })
            .border({ width: 1.5, color: 'rgba(229, 236, 234, 0.80)' })
            .shadow({ radius: 14, color: '#70000000', offsetX: 0, offsetY: 3 })
          // 内圈高光，增加立体感。
          Circle()
            .width(KNOB_SIZE * 0.55)
            .height(KNOB_SIZE * 0.55)
            .fill('rgba(255, 255, 255, 0.12)')
            .position({ x: KNOB_SIZE * 0.12, y: KNOB_SIZE * 0.08 })
        }
        .width(KNOB_SIZE)
        .height(KNOB_SIZE)
        .position({ x: (BASE_SIZE - KNOB_SIZE) / 2 + this.knobOffsetX,
          y: (BASE_SIZE - KNOB_SIZE) / 2 + this.knobOffsetY })
        .opacity(this.active ? 1 : 0.85)
        .hitTestBehavior(HitTestMode.Transparent)
      }
      .width(BASE_SIZE)
      .height(BASE_SIZE)
      .borderRadius(BASE_SIZE / 2)
      .backgroundColor(this.active ? 'rgba(20, 26, 38, 0.42)' : 'rgba(20, 26, 38, 0.28)')
      .border({ width: 1.5,
        color: this.active ? 'rgba(214, 226, 223, 0.45)' : 'rgba(214, 226, 223, 0.28)' })
      .shadow({ radius: 10, color: '#30000000', offsetX: 0, offsetY: 2 })
      .margin({ left: BASE_MARGIN_LEFT, bottom: BASE_MARGIN_BOTTOM })
      .hitTestBehavior(HitTestMode.Transparent)
      .animation({ duration: 120, curve: Curve.EaseOut })
    }
    .width('100%')
    .height('100%')
    .hitTestBehavior(HitTestMode.Transparent)
  }
}

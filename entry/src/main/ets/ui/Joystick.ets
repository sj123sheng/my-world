// 仅负责摇杆视觉反馈；生产输入由下层 XComponent 的 native 回调统一处理。
// 旋钮视觉行程半径（vp）。
const KNOB_TRAVEL: number = 50;
const BASE_SIZE: number = 120;
const KNOB_SIZE: number = 52;
const TICK_SIZE: number = 6;

@Component
export struct Joystick {
  @State active: boolean = false;
  @State baseCenterX: number = 0;
  @State baseCenterY: number = 0;
  @State knobCenterX: number = 0;
  @State knobCenterY: number = 0;
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
      // 已跟踪一根手指时忽略后续落点，避免视觉原点被抢占。
      if (this.trackedPointer !== -1) {
        return;
      }
      this.trackedPointer = touch.id;
      this.originX = touch.x;
      this.originY = touch.y;
      this.baseCenterX = touch.x;
      this.baseCenterY = touch.y;
      this.knobCenterX = touch.x;
      this.knobCenterY = touch.y;
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
      this.knobCenterX = this.originX + knobOffsetX;
      this.knobCenterY = this.originY + knobOffsetY;
      return;
    }

    // Up / Cancel：native XComponent 会结束输入，这里只隐藏视觉摇杆。
    this.trackedPointer = -1;
    this.active = false;
  }

  build() {
    Stack({ alignContent: Alignment.Start }) {
      // 空闲提示：左下固定位置的淡底盘虚影，触摸出现后淡出。
      Stack({ alignContent: Alignment.BottomStart }) {
        Column({ space: 6 }) {
          Stack() {
            // 内圈小圆点，增加视觉锚点。
            Circle({ width: 6, height: 6 })
              .fill('rgba(214, 226, 223, 0.35)')
          }
          .width(72)
          .height(72)
          .borderRadius(36)
          .border({ width: 1.5, color: 'rgba(214, 226, 223, 0.22)' })
          .backgroundColor('rgba(20, 26, 38, 0.15)')
          .shadow({ radius: 8, color: '#20000000', offsetX: 0, offsetY: 2 })

          Text('移动')
            .fontSize(10)
            .letterSpacing(2)
            .fontColor('rgba(229, 236, 234, 0.35)')
            .fontWeight(FontWeight.Lighter)
        }
        .alignItems(HorizontalAlign.Center)
        .margin({ left: 52, bottom: 168 })
      }
      .width('100%')
      .height('100%')
      .hitTestBehavior(HitTestMode.Transparent)
      .opacity(this.active ? 0 : 1)
      .animation({ duration: 150, curve: Curve.EaseOut })

      // 左半屏仅观察触摸以绘制摇杆；透明命中让事件继续到 XComponent。
      // 右半屏不被覆盖，相机触摸同样由 XComponent native 回调处理。
      Stack()
        .width('50%')
        .height('100%')
        .backgroundColor(Color.Transparent)
        .onTouch((event: TouchEvent) => {
          this.handleTouch(event);
        })

      // 底盘：按下位置浮出，中心为触点原点。
      if (this.active) {
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
        }
        .width(BASE_SIZE)
        .height(BASE_SIZE)
        .borderRadius(BASE_SIZE / 2)
        .backgroundColor('rgba(20, 26, 38, 0.35)')
        .border({ width: 1.5, color: 'rgba(214, 226, 223, 0.38)' })
        .shadow({ radius: 10, color: '#30000000', offsetX: 0, offsetY: 2 })
        .position({ x: this.baseCenterX - BASE_SIZE / 2, y: this.baseCenterY - BASE_SIZE / 2 })
        .hitTestBehavior(HitTestMode.Transparent)
      }

      // 旋钮：跟手移动，行程限制在 KNOB_TRAVEL 内。
      if (this.active) {
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
        .position({ x: this.knobCenterX - KNOB_SIZE / 2, y: this.knobCenterY - KNOB_SIZE / 2 })
        .hitTestBehavior(HitTestMode.Transparent)
      }
    }
    .width('100%')
    .height('100%')
    .hitTestBehavior(HitTestMode.Transparent)
  }
}

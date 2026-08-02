import { vibrator } from '@kit.SensorServiceKit';

// 战斗触感反馈封装：基于鸿蒙 vibrator 的时长振动。
// 所有失败静默忽略（不支持振动的设备/权限缺失），绝不打断游戏运行。
export class Haptics {
  // 最小触发间隔，防止高频事件（如连续受击）造成振动糊化。
  private static readonly MIN_INTERVAL_MS: number = 40;
  private static lastVibrateAt: number = 0;

  private static buzz(duration: number): void {
    // 设置中的振动开关：关闭后所有触感反馈静默。
    if (AppStorage.get<boolean>('hapticsEnabled') === false) {
      return;
    }
    const now: number = Date.now();
    if (now - Haptics.lastVibrateAt < Haptics.MIN_INTERVAL_MS) {
      return;
    }
    Haptics.lastVibrateAt = now;
    try {
      vibrator.startVibration(
        { type: 'time', duration: duration },
        { id: 0, usage: 'physicalFeedback' }
      ).catch((): void => {
        // 设备不支持或调用失败时静默降级。
      });
    } catch (error) {
      // 参数或环境异常时静默降级。
    }
  }

  // 轻触：技能命中、普攻落点。
  static light(): void {
    Haptics.buzz(18);
  }

  // 利落短振：精准闪避（洞察触发）。
  static sharp(): void {
    Haptics.buzz(28);
  }

  // 受击：普通伤害。
  static hit(): void {
    Haptics.buzz(45);
  }

  // 重振：大额伤害或破韧。
  static heavy(): void {
    Haptics.buzz(75);
  }

  // 终结技就绪：双段脉冲。
  static ultimateReady(): void {
    Haptics.buzz(35);
    setTimeout((): void => {
      Haptics.buzz(55);
    }, 90);
  }
}

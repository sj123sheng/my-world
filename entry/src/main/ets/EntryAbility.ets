import { UIAbility, Want, AbilityConstant } from '@kit.AbilityKit';
import { window } from '@kit.ArkUI';
import { nativeStart, nativeStop } from './napi/Bridge';

export default class EntryAbility extends UIAbility {
  onCreate(want: Want, launchParam: AbilityConstant.LaunchParam): void {
    console.info('[Ethelan] EntryAbility onCreate');
  }

  onDestroy(): void {
    console.info('[Ethelan] EntryAbility onDestroy');
  }

  onWindowStageCreate(windowStage: window.WindowStage): void {
    console.info('[Ethelan] EntryAbility onWindowStageCreate');
    // 游戏画面固定横屏，兼容左摇杆右技能的双手操控布局。
    try {
      const mainWindow = windowStage.getMainWindowSync();
      mainWindow.setPreferredOrientation(window.Orientation.LANDSCAPE);
      // 沉浸式全屏：隐藏状态栏与导航栏，消除上下黑/白条带，
      // 游戏内容延伸至整块屏幕。
      mainWindow.setWindowLayoutFullScreen(true);
      mainWindow.setWindowSystemBarEnable([]);
    } catch (error) {
      console.error('[Ethelan] Failed to set landscape orientation: ' + JSON.stringify(error));
    }
    windowStage.loadContent('pages/GamePage', (err, data) => {
      if (err.code) {
        console.error('[Ethelan] Failed to load GamePage: ' + JSON.stringify(err));
        return;
      }
      console.info('[Ethelan] GamePage loaded successfully');
    });
  }

  onWindowStageDestroy(): void {
    console.info('[Ethelan] EntryAbility onWindowStageDestroy');
  }

  onForeground(): void {
    console.info('[Ethelan] EntryAbility onForeground');
    nativeStart();
  }

  onBackground(): void {
    console.info('[Ethelan] EntryAbility onBackground');
    nativeStop();
  }

  // Game Service Kit 登录桩:待真机 SDK 验证后补全
  onGameServiceInit(): void {
    console.info('[Ethelan] Game Service Kit stub');
  }
}

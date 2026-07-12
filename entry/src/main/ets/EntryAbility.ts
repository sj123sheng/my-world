import UIAbility from '@ohos.app.ability.UIAbility';
import window from '@ohos.window';

export default class EntryAbility extends UIAbility {
  onCreate(want: any, launchParam: any): void {
    console.info('[Ethelan] EntryAbility onCreate');
  }

  onDestroy(): void {
    console.info('[Ethelan] EntryAbility onDestroy');
  }

  onWindowStageCreate(windowStage: window.WindowStage): void {
    console.info('[Ethelan] EntryAbility onWindowStageCreate');
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
  }

  onBackground(): void {
    console.info('[Ethelan] EntryAbility onBackground');
  }
}

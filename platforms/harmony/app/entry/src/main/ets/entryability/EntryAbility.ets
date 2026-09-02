// SPDX-License-Identifier: Apache-2.0
import { AbilityConstant, UIAbility, Want } from '@kit.AbilityKit';
import { BusinessError } from '@kit.BasicServicesKit';
import { window } from '@kit.ArkUI';
import { AudioService } from '../audio/AudioService';

export default class EntryAbility extends UIAbility {
  onCreate(want: Want, launchParam: AbilityConstant.LaunchParam): void {
    void want;
    void launchParam;
    void AudioService.getInstance().initialize(this.context);
  }

  onWindowStageCreate(windowStage: window.WindowStage): void {
    windowStage.loadContent('pages/Index', (error: BusinessError): void => {
      if (error.code !== 0) {
        console.error(`MoL page load failed: ${error.code} ${error.message}`);
      }
    });
  }

  onForeground(): void {
    void AudioService.getInstance().onForeground();
  }

  onBackground(): void {
    void AudioService.getInstance().onBackground();
  }

  onDestroy(): void {
    void AudioService.getInstance().onAbilityDestroy();
  }
}

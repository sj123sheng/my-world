import { pushAction } from '../napi/Bridge';

// 探索 HUD（阶段一）：体力条、小地图、运动状态标签与锚点交互提示。
// 样式刻意避开 linearGradient/mask 以规避已知 ArkTS 兼容性问题，
// 仅使用纯色背景与透明度。
@Component
export struct ExplorationHud {
  @Prop explorationStamina: number = 100;
  @Prop motionState: number = 0;
  @Prop interactionAnchorId: number = -1;
  @Prop interactionUnlocked: boolean = false;
  @Prop interactionLabel: string = '';
  // 交互种类：0=无 1=锚点 2=NPC 3=宝箱 4=采集物 5=秘境入口。
  @Prop interactionKind: number = 0;
  @Prop unlockedAnchorCount: number = 0;
  // 阶段二验收补齐：支线完成数与秘境进度。
  @Prop completedSideQuestCount: number = 0;
  @Prop dungeonState: number = 0;
  @Prop dungeonProgress: number = 0;
  @Prop dungeonRequired: number = 3;
  @Prop teleportFlashMs: number = 0;
  @Prop playerX: number = 0.5;
  @Prop playerY: number = 0.5;
  @Prop minimapAnchorX: number[] = [];
  @Prop minimapAnchorY: number[] = [];
  @Prop minimapAnchorUnlocked: number[] = [];
  // 优化批次：小地图可交互物标记与支线进度。
  @Prop minimapItemX: number[] = [];
  @Prop minimapItemY: number[] = [];
  @Prop minimapItemKind: number[] = [];
  @Prop sideQuestProgress: number[] = [];
  @Prop sideQuestRequired: number[] = [];
  // 第四轮优化：自动疾跑与每日委托。
  @Prop sprintActive: number = 0;
  @Prop dailyCompletedCount: number = 0;
  @Prop dailyQuestClaimed: number = 0;
  @Prop fateCount: number = 0;
  // 阶段四：出战角色、昼夜小时与天气。
  @Prop activeCharacterId: number = 1;
  @Prop dayNightHour: number = 8;
  @Prop weatherId: number = 0;
  // 打开抽卡/养成面板的回调，由 GamePage 注入。
  onOpenGacha: () => void = () => {};
  // 打开大地图面板的回调（快速传送）。
  onOpenMap: () => void = () => {};
  // 打开背包/圣遗物面板的回调（原神式养成）。
  onOpenInventory: () => void = () => {};
  onOpenArtifact: () => void = () => {};
  // 冒险等级与世界等级（原神式）。
  @Prop adventureRank: number = 1;
  @Prop adventureExp: number = 0;
  @Prop adventureExpRequired: number = 375;
  @Prop worldLevel: number = 0;
  @Prop explorationPoiCount: number = 0;
  @Prop explorationPuzzleCount: number = 0;
  @Prop explorationRewardCount: number = 0;
  @Prop explorationTraversalMask: number = 0;
  @Prop explorationCurrentTargetLabel: string = '';
  @Prop explorationCurrentTargetDistrict: string = '';

  private activeCharacterName(): string {
    switch (this.activeCharacterId) {
      case 1: return '辉印·莉拉';
      case 2: return '脉流·凯恩';
      case 3: return '蚀质·莫拉';
      case 4: return '雾谷猎手·希娅';
      case 5: return '遗迹守卫·奥尔丁';
      case 6: return '辉光祭司·努恩';
      default: return '巡脉者';
    }
  }

  private weatherLabel(): string {
    switch (this.weatherId) {
      case 1: return '多云';
      case 2: return '雨';
      case 3: return '雪';
      default: return '晴';
    }
  }

  private sideQuestName(index: number): string {
    switch (index) {
      case 0: return '雾谷肃清';
      case 1: return '脉流采集';
      case 2: return '远行之路';
      default: return '支线';
    }
  }

  private motionLabel(): string {
    if (this.sprintActive === 1) return '疾跑';
    switch (this.motionState) {
      case 1: return '空中';
      case 2: return '滑翔';
      case 3: return '攀爬';
      case 4: return '游泳';
      default: return '';
    }
  }

  private traversalCount(): number {
    let count = 0;
    let mask = this.explorationTraversalMask;
    for (let index = 0; index < 5; index += 1) {
      if ((mask & 1) === 1) count += 1;
      mask = Math.floor(mask / 2);
    }
    return count;
  }

  private staminaVisible(): boolean {
    return this.explorationStamina < 99.9 || this.motionState !== 0 ||
           this.sprintActive === 1;
  }

  // 天气色调（探索优化）：雨/雪/多云叠半透明色层，晴不叠。
  private weatherTint(): string {
    switch (this.weatherId) {
      case 1: return 'rgba(90, 100, 110, 0.12)';
      case 2: return 'rgba(60, 80, 110, 0.2)';
      case 3: return 'rgba(220, 228, 235, 0.16)';
      default: return 'rgba(0, 0, 0, 0)';
    }
  }

  private interactVerb(): string {
    switch (this.interactionKind) {
      case 1: return this.interactionUnlocked ? '传送' : '解锁';
      case 2: return '对话';
      case 3: return '开启';
      case 4: return '采集';
      case 5: return '进入';
      default: return '交互';
    }
  }

  private interactPrefix(): string {
    switch (this.interactionKind) {
      case 1: return this.interactionUnlocked ? '传送' : '解锁';
      case 2: return '对话';
      case 3: return '宝箱';
      case 4: return '采集';
      case 5: return '秘境';
      default: return '';
    }
  }

  build() {
    Stack() {
      // 天气色调覆盖层（探索优化）：雨/雪/多云时叠半透明色层，不拦截触摸。
      Column()
        .width('100%')
        .height('100%')
        .backgroundColor(this.weatherTint())
        .hitTestBehavior(HitTestMode.Transparent)

      // 传送/解锁反馈：全屏白色闪光，透明度随剩余时间衰减。
      Column()
        .width('100%')
        .height('100%')
        .backgroundColor('#FFFFFF')
        .opacity(this.teleportFlashMs > 0
          ? Math.min(this.teleportFlashMs / 1200, 1) * 0.35 : 0)
        .hitTestBehavior(HitTestMode.Transparent)

      // 小地图：左上角，锚点按世界坐标映射；已解锁金色、未解锁灰色。
      Stack() {
        Circle()
          .width(116)
          .height(116)
          .fill('rgba(10, 14, 22, 0.6)')
        Circle()
          .width(116)
          .height(116)
          .fill('rgba(0, 0, 0, 0)')
          .stroke('rgba(199, 208, 206, 0.4)')
          .strokeWidth(1.5)
        ForEach(this.minimapAnchorX, (ax: number, index: number) => {
          Circle()
            .width(this.minimapAnchorUnlocked[index] === 1 ? 8 : 6)
            .height(this.minimapAnchorUnlocked[index] === 1 ? 8 : 6)
            .fill(this.minimapAnchorUnlocked[index] === 1
              ? '#E5B84A' : 'rgba(199, 208, 206, 0.5)')
            .position({ x: 8 + ax * 100, y: 8 + (this.minimapAnchorY[index] ?? 0) * 100 })
        })
        // 可交互物标记：宝箱金色方块、采集绿色、秘境紫色。
        ForEach(this.minimapItemX, (ix: number, index: number) => {
          Rect()
            .width(6)
            .height(6)
            .fill(this.minimapItemKind[index] === 3 ? '#E5B84A'
              : this.minimapItemKind[index] === 4 ? '#7FD88F' : '#B08FE5')
            .position({ x: 8 + ix * 100, y: 8 + (this.minimapItemY[index] ?? 0) * 100 })
        })
        // 玩家标记：白色圆点 + 方向无关，位置按世界坐标映射。
        Circle()
          .width(9)
          .height(9)
          .fill('#FFFFFF')
          .position({ x: 8 + this.playerX * 100 - 2, y: 8 + this.playerY * 100 - 2 })
      }
      .width(116)
      .height(116)
      .position({ x: 14, y: 14 })
      .hitTestBehavior(HitTestMode.Transparent)

      // 召唤入口：右上角，展示共鸣之契数量。
      Button(`召唤 ${this.fateCount}`)
        .width(72)
        .height(36)
        .fontSize(12)
        .type(ButtonType.Normal)
        .borderRadius(18)
        .backgroundColor('rgba(122, 90, 34, 0.8)')
        .fontColor('#F2E0B8')
        .fontWeight(FontWeight.Bold)
        .stateEffect(true)
        .position({ x: '100%', y: 14 })
        .markAnchor({ x: 176, y: 0 })
        .hitTestBehavior(HitTestMode.Block)
        .onClick(() => { this.onOpenGacha(); })

      // 地图入口：召唤按钮左侧，打开大地图快速传送。
      Button('地图')
        .width(56)
        .height(36)
        .fontSize(12)
        .type(ButtonType.Normal)
        .borderRadius(18)
        .backgroundColor('rgba(31, 90, 80, 0.8)')
        .fontColor('#B8F2E6')
        .fontWeight(FontWeight.Bold)
        .stateEffect(true)
        .position({ x: '100%', y: 14 })
        .markAnchor({ x: 244, y: 0 })
        .hitTestBehavior(HitTestMode.Block)
        .onClick(() => { this.onOpenMap(); })

      // 背包入口：地图按钮左侧（原神式背包）。
      Button('背包')
        .width(56)
        .height(36)
        .fontSize(12)
        .type(ButtonType.Normal)
        .borderRadius(18)
        .backgroundColor('rgba(74, 52, 94, 0.8)')
        .fontColor('#E0D0F2')
        .fontWeight(FontWeight.Bold)
        .stateEffect(true)
        .position({ x: '100%', y: 14 })
        .markAnchor({ x: 312, y: 0 })
        .hitTestBehavior(HitTestMode.Block)
        .onClick(() => { this.onOpenInventory(); })

      // 圣遗物入口：背包按钮左侧。
      Button('圣遗物')
        .width(64)
        .height(36)
        .fontSize(12)
        .type(ButtonType.Normal)
        .borderRadius(18)
        .backgroundColor('rgba(94, 62, 52, 0.8)')
        .fontColor('#F2D8C8')
        .fontWeight(FontWeight.Bold)
        .stateEffect(true)
        .position({ x: '100%', y: 14 })
        .markAnchor({ x: 388, y: 0 })
        .hitTestBehavior(HitTestMode.Block)
        .onClick(() => { this.onOpenArtifact(); })

      // 冒险等级徽标：右上角按钮行下方，AR + 世界等级 + 经验条。
      Column({ space: 2 }) {
        Text(`冒险等阶 ${this.adventureRank} · 世界等级 ${this.worldLevel}`)
          .fontSize(10)
          .fontColor('#F2E0B8')
          .fontWeight(FontWeight.Bold)
        Stack({ alignContent: Alignment.Start }) {
          Row()
            .width(160)
            .height(5)
            .borderRadius(3)
            .backgroundColor('rgba(20, 26, 38, 0.6)')
          Row()
            .width(Math.max(2, 160 * (this.adventureExpRequired > 0
              ? this.adventureExp / this.adventureExpRequired : 0)))
            .height(5)
            .borderRadius(3)
            .backgroundColor('#E5B84A')
        }
        Text(`${this.adventureExp}/${this.adventureExpRequired}`)
          .fontSize(9)
          .fontColor('rgba(199, 208, 206, 0.8)')
      }
      .padding(6)
      .borderRadius(8)
      .backgroundColor('rgba(20, 26, 38, 0.55)')
      .position({ x: '100%', y: 58 })
      .markAnchor({ x: 176, y: 0 })
      .hitTestBehavior(HitTestMode.None)

      // 支线任务追踪条：小地图下方，三条支线进度与每日委托。
      if (this.explorationCurrentTargetLabel.length > 0) {
        Column({ space: 2 }) {
          Text(this.explorationCurrentTargetLabel)
            .fontSize(12)
            .fontWeight(FontWeight.Bold)
            .fontColor('#F2E0B8')
          Text(`${this.explorationCurrentTargetDistrict} · 地标 ${this.explorationPoiCount}/5 · 机关 ${this.explorationPuzzleCount}/4`)
            .fontSize(9)
            .fontColor('rgba(199, 208, 206, 0.82)')
          Text(`奖励 ${this.explorationRewardCount}/4 · 移动能力 ${this.traversalCount()}/5`)
            .fontSize(9)
            .fontColor('#9FD8CB')
        }
        .alignItems(HorizontalAlign.Start)
        .padding({ left: 8, right: 8, top: 6, bottom: 6 })
        .backgroundColor('rgba(10, 14, 22, 0.62)')
        .borderRadius(8)
        .position({ x: 140, y: 84 })
        .hitTestBehavior(HitTestMode.Transparent)
      }

      if (this.sideQuestProgress.length > 0) {
        Column({ space: 4 }) {
          Text(`每日委托 ${this.dailyCompletedCount}/4${this.dailyQuestClaimed === 1 ? ' · 已领取' : ''}`)
            .fontSize(10)
            .fontColor(this.dailyQuestClaimed === 1 ? '#E5B84A' : '#9FD8CB')
          ForEach(this.sideQuestProgress, (progress: number, index: number) => {
            Text(`${this.sideQuestName(index)} ${Math.min(progress, this.sideQuestRequired[index] ?? 0)}/${this.sideQuestRequired[index] ?? 0}`)
              .fontSize(10)
              .fontColor(progress >= (this.sideQuestRequired[index] ?? 1)
                ? '#E5B84A' : 'rgba(199, 208, 206, 0.85)')
          })
        }
        .padding({ left: 8, right: 8, top: 6, bottom: 6 })
        .backgroundColor('rgba(10, 14, 22, 0.55)')
        .borderRadius(10)
        .alignItems(HorizontalAlign.Start)
        .position({ x: 14, y: 138 })
        .hitTestBehavior(HitTestMode.Transparent)
      }

      // 出战角色卡片：小地图右侧，点击循环切换角色。
      Button(this.activeCharacterName())
        .width(96)
        .height(36)
        .fontSize(11)
        .type(ButtonType.Normal)
        .borderRadius(18)
        .backgroundColor('rgba(31, 90, 80, 0.75)')
        .fontColor('#B8F2E6')
        .fontWeight(FontWeight.Medium)
        .stateEffect(true)
        .position({ x: 140, y: 14 })
        .hitTestBehavior(HitTestMode.Block)
        .onClick(() => { pushAction(10); })

      // 昼夜与天气指示：出战角色下方。
      Text(`${this.dayNightHour >= 6 && this.dayNightHour < 18
        ? `白昼 ${Math.floor(this.dayNightHour)}时`
        : `夜晚 ${Math.floor(this.dayNightHour)}时`} · ${this.weatherLabel()}`)
        .fontSize(10)
        .fontColor('#C7D0CE')
        .backgroundColor('rgba(10, 14, 22, 0.5)')
        .borderRadius(8)
        .padding({ left: 8, right: 8, top: 2, bottom: 2 })
        .position({ x: 140, y: 56 })
        .hitTestBehavior(HitTestMode.Transparent)

      // 秘境进度：进行中时显示击杀目标。
      if (this.dungeonState >= 1) {
        Text(`秘境·回声回廊 ${this.dungeonProgress}/${this.dungeonRequired}`)
          .fontSize(10)
          .fontColor(this.dungeonState === 2 ? '#E5B84A' : '#9FD8CB')
          .backgroundColor('rgba(10, 14, 22, 0.5)')
          .borderRadius(8)
          .padding({ left: 8, right: 8, top: 2, bottom: 2 })
          .position({ x: 140, y: 78 })
          .hitTestBehavior(HitTestMode.Transparent)
      }

      // 锚点解锁计数：小地图下方。
      if (this.unlockedAnchorCount > 0) {
        Text(`锚点 ${this.unlockedAnchorCount}`)
          .fontSize(11)
          .fontColor('#C7D0CE')
          .backgroundColor('rgba(10, 14, 22, 0.5)')
          .borderRadius(8)
          .padding({ left: 8, right: 8, top: 2, bottom: 2 })
          .position({ x: 14, y: 136 })
          .hitTestBehavior(HitTestMode.Transparent)
      }

      // 探索体力条：左下角摇杆上方；消耗中显示，满值且地面时隐藏。
      if (this.staminaVisible()) {
        Column({ space: 4 }) {
          Row({ space: 6 }) {
            Text(this.motionLabel())
              .fontSize(11)
              .fontColor('#B8F2E6')
              .backgroundColor('rgba(31, 90, 80, 0.65)')
              .borderRadius(8)
              .padding({ left: 8, right: 8, top: 2, bottom: 2 })
              .visibility(this.motionLabel().length > 0 ? Visibility.Visible : Visibility.None)
          }
          Stack({ alignContent: Alignment.Start }) {
            Row()
              .width(180)
              .height(8)
              .borderRadius(4)
              .backgroundColor('rgba(10, 14, 22, 0.6)')
            Row()
              .width(Math.max(this.explorationStamina / 100, 0) * 180)
              .height(8)
              .borderRadius(4)
              .backgroundColor(this.explorationStamina > 25 ? '#5EAB9E' : '#C86A5A')
              .animation({ duration: 100, curve: Curve.Linear })
          }
          .width(180)
          .height(8)
        }
        .position({ x: 30, y: '100%' })
        .markAnchor({ x: 0, y: 250 })
        .hitTestBehavior(HitTestMode.Transparent)
      }

      // 交互提示：底部中央，含交互按钮（锚点/NPC/宝箱/采集物）。
      if (this.interactionKind > 0) {
        Row({ space: 10 }) {
          Text(`${this.interactPrefix()} · ${this.interactionLabel}`)
            .fontSize(14)
            .fontColor('#E5ECEA')
          Button(this.interactVerb())
            .width(64)
            .height(34)
            .fontSize(13)
            .type(ButtonType.Normal)
            .borderRadius(17)
            .backgroundColor(this.interactionKind === 1 && this.interactionUnlocked
              ? '#C89B3C' : '#1F5A50')
            .fontColor('#FFFFFF')
            .fontWeight(FontWeight.Bold)
            .stateEffect(true)
            .hitTestBehavior(HitTestMode.Block)
            .onClick(() => { pushAction(7); })
        }
        .padding({ left: 16, right: 16, top: 8, bottom: 8 })
        .backgroundColor('rgba(10, 14, 22, 0.72)')
        .borderRadius(22)
        .border({ width: 1, color: 'rgba(229, 184, 74, 0.5)' })
        .position({ x: '50%', y: '100%' })
        .markAnchor({ x: '50%', y: 120 })
      }
    }
    .width('100%')
    .height('100%')
    .hitTestBehavior(HitTestMode.None)
  }
}

import { upgradeArtifact, equipArtifact } from '../napi/Bridge';

// 圣遗物面板（原神式养成）：
// 列表选择圣遗物，支持装备到出战角色与喂食强化（消耗未装备的低星圣遗物
// 并扣除金币）；展示套装件数效果。
// 样式避开 linearGradient/mask 以规避已知 ArkTS 兼容性问题。
@Component
export struct ArtifactPanel {
  @Prop open: boolean = false;
  @Prop goldCount: number = 0;
  @Prop artifactInstanceIds: number[] = [];
  @Prop artifactDefIds: number[] = [];
  @Prop artifactRarities: number[] = [];
  @Prop artifactLevels: number[] = [];
  @Prop artifactEquippedBy: number[] = [];
  @Prop activeCharacterId: number = 1;
  @State selectedIndex: number = -1;
  onClose: () => void = () => {};

  private characterName(id: number): string {
    switch (id) {
      case 1: return '辉印·莉拉';
      case 2: return '脉流·凯恩';
      case 3: return '蚀质·莫拉';
      case 4: return '雾谷猎手·希娅';
      case 5: return '遗迹守卫·奥尔丁';
      case 6: return '辉光祭司·努恩';
      default: return '未知角色';
    }
  }

  private artifactSetName(setId: number): string {
    switch (setId) {
      case 1: return '辉辉骑士';
      case 2: return '潮鸣之诗';
      default: return '蚀痕残响';
    }
  }

  private artifactName(defId: number): string {
    const setName = this.artifactSetName(Math.floor(defId / 10));
    const slot = defId % 10;
    switch (slot) {
      case 1: return `${setName}·生之花`;
      case 2: return `${setName}·死之羽`;
      case 3: return `${setName}·时之沙`;
      case 4: return `${setName}·空之杯`;
      default: return `${setName}·理之冠`;
    }
  }

  // 主属性文案：与 native ArtifactSystem 部位规则一致。
  private mainStatLabel(defId: number): string {
    const slot = defId % 10;
    switch (slot) {
      case 1: return '生命';
      case 2: return '攻击';
      case 5: return '暴击率';
      default: return '攻击%';
    }
  }

  private rarityColor(rarity: number): string {
    if (rarity >= 5) return '#E5B84A';
    if (rarity === 4) return '#B08FE5';
    return '#9FB6C6';
  }

  private maxLevel(rarity: number): number {
    return rarity * 4;
  }

  // 出战角色当前同套装件数（2件套+10%攻击，4件套+25%攻击）。
  private setPieceLabel(): string {
    if (this.selectedIndex < 0 ||
        this.selectedIndex >= this.artifactDefIds.length) {
      return '';
    }
    const setId = Math.floor(this.artifactDefIds[this.selectedIndex] / 10);
    let count = 0;
    for (let i = 0; i < this.artifactDefIds.length; ++i) {
      if (Math.floor(this.artifactDefIds[i] / 10) === setId &&
          this.artifactEquippedBy[i] === this.activeCharacterId) {
        count += 1;
      }
    }
    if (count >= 4) return `${this.artifactSetName(setId)}：4件套 攻击+25%`;
    if (count >= 2) return `${this.artifactSetName(setId)}：2件套 攻击+10%`;
    return `${this.artifactSetName(setId)}：${count}/2 件`;
  }

  // 可作为素材的未装备三星圣遗物实例 id。
  private feedCandidates(): number[] {
    const feeds: number[] = [];
    for (let i = 0; i < this.artifactInstanceIds.length; ++i) {
      if (i === this.selectedIndex) continue;
      if (this.artifactRarities[i] === 3 && this.artifactEquippedBy[i] === 0) {
        feeds.push(this.artifactInstanceIds[i]);
      }
    }
    return feeds;
  }

  build() {
    if (this.open) {
      Column({ space: 12 }) {
        // 标题栏。
        Row() {
          Text('圣遗物')
            .fontSize(18)
            .fontWeight(FontWeight.Bold)
            .fontColor('#F2E0B8')
          Blank()
          Text(`金币 ${this.goldCount} · 目标角色 ${this.characterName(this.activeCharacterId)}`)
            .fontSize(12)
            .fontColor('#C7D0CE')
          Button('关闭')
            .width(56)
            .height(30)
            .fontSize(12)
            .type(ButtonType.Normal)
            .borderRadius(15)
            .backgroundColor('#343A40')
            .fontColor('#E5ECEA')
            .stateEffect(true)
            .hitTestBehavior(HitTestMode.Block)
            .onClick(() => { this.onClose(); })
        }
        .width('100%')

        if (this.artifactInstanceIds.length === 0) {
          Text('暂无圣遗物：击败 Boss 必掉五星圣遗物，秘境也有机会产出')
            .fontSize(12)
            .fontColor('rgba(199, 208, 206, 0.6)')
            .width('100%')
            .margin({ top: 16 })
        } else {
          Row({ space: 12 }) {
            // 左列：圣遗物列表。
            Scroll() {
              Column({ space: 8 }) {
                ForEach(this.artifactInstanceIds, (instanceId: number, index: number) => {
                  Column({ space: 2 }) {
                    Text(this.artifactName(this.artifactDefIds[index]))
                      .fontSize(12)
                      .fontColor(this.rarityColor(this.artifactRarities[index]))
                    Text(`Lv.${this.artifactLevels[index]} · ${this.artifactRarities[index]}星${this.artifactEquippedBy[index] > 0
                      ? ` · ${this.characterName(this.artifactEquippedBy[index])}` : ''}`)
                      .fontSize(10)
                      .fontColor('#9FB6C6')
                  }
                  .alignItems(HorizontalAlign.Start)
                  .width('100%')
                  .padding(8)
                  .backgroundColor(this.selectedIndex === index
                    ? 'rgba(122, 90, 34, 0.5)' : 'rgba(20, 26, 38, 0.7)')
                  .borderRadius(10)
                  .onClick(() => { this.selectedIndex = index; })
                })
              }
              .width('100%')
            }
            .width('46%')
            .layoutWeight(1)

            // 右列：详情与操作。
            Column({ space: 10 }) {
              if (this.selectedIndex >= 0 &&
                  this.selectedIndex < this.artifactInstanceIds.length) {
                Text(this.artifactName(this.artifactDefIds[this.selectedIndex]))
                  .fontSize(15)
                  .fontWeight(FontWeight.Bold)
                  .fontColor(this.rarityColor(this.artifactRarities[this.selectedIndex]))
                Text(`主属性：${this.mainStatLabel(this.artifactDefIds[this.selectedIndex])}`)
                  .fontSize(12)
                  .fontColor('#C7D0CE')
                Text(`等级 ${this.artifactLevels[this.selectedIndex]}/${this.maxLevel(this.artifactRarities[this.selectedIndex])} · 强化费用为经验的十分之一`)
                  .fontSize(11)
                  .fontColor('#9FB6C6')
                Text(this.setPieceLabel())
                  .fontSize(11)
                  .fontColor('#B8F2E6')
                Text(this.artifactEquippedBy[this.selectedIndex] > 0
                  ? `已装备：${this.characterName(this.artifactEquippedBy[this.selectedIndex])}`
                  : '未装备')
                  .fontSize(11)
                  .fontColor('#E5B84A')
                Button(this.artifactEquippedBy[this.selectedIndex] === this.activeCharacterId
                  ? '已装备' : `装备给 ${this.characterName(this.activeCharacterId)}`)
                  .width('100%').height(34).fontSize(12)
                  .type(ButtonType.Normal).borderRadius(17)
                  .backgroundColor(this.artifactEquippedBy[this.selectedIndex] === this.activeCharacterId
                    ? '#343A40' : '#7A5A22')
                  .fontColor(this.artifactEquippedBy[this.selectedIndex] === this.activeCharacterId
                    ? '#7B8288' : '#F2E0B8')
                  .stateEffect(true)
                  .hitTestBehavior(HitTestMode.Block)
                  .onClick(() => {
                    equipArtifact(this.artifactInstanceIds[this.selectedIndex],
                      this.activeCharacterId);
                  })
                Button(`喂食强化（三星素材×${this.feedCandidates().length}）`)
                  .width('100%').height(34).fontSize(12)
                  .type(ButtonType.Normal).borderRadius(17)
                  .backgroundColor(this.feedCandidates().length > 0 &&
                      this.artifactLevels[this.selectedIndex] <
                        this.maxLevel(this.artifactRarities[this.selectedIndex])
                    ? '#1F5A50' : '#343A40')
                  .fontColor(this.feedCandidates().length > 0 &&
                      this.artifactLevels[this.selectedIndex] <
                        this.maxLevel(this.artifactRarities[this.selectedIndex])
                    ? '#B8F2E6' : '#7B8288')
                  .stateEffect(true)
                  .hitTestBehavior(HitTestMode.Block)
                  .onClick(() => {
                    upgradeArtifact(this.artifactInstanceIds[this.selectedIndex],
                      this.feedCandidates());
                    this.selectedIndex = -1;
                  })
              } else {
                Text('点击左侧列表选择圣遗物')
                  .fontSize(12)
                  .fontColor('rgba(199, 208, 206, 0.6)')
              }
            }
            .alignItems(HorizontalAlign.Start)
            .width('46%')
            .layoutWeight(1)
          }
          .width('100%')
          .layoutWeight(1)
        }
      }
      .width('88%')
      .height('72%')
      .padding(16)
      .backgroundColor('rgba(8, 12, 20, 0.94)')
      .borderRadius(18)
      .border({ width: 1, color: 'rgba(229, 184, 74, 0.35)' })
      .position({ x: '50%', y: '50%' })
      .markAnchor({ x: '50%', y: '50%' })
    }
  }
}

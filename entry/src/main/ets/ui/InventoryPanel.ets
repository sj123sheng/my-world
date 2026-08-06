import { useExpItem, equipArtifact, claimRankReward } from '../napi/Bridge';

// 背包面板（原神式养成）：
// 货币/材料/圣遗物/冒险四个页签；材料页签支持经验书使用，
// 圣遗物页签支持快捷装备，冒险页签展示等阶奖励并领取。
// 样式避开 linearGradient/mask 以规避已知 ArkTS 兼容性问题。
@Component
export struct InventoryPanel {
  @Prop open: boolean = false;
  @Prop fateCount: number = 0;
  @Prop goldCount: number = 0;
  @Prop expMaterialCount: number = 0;
  @Prop ascensionMaterialCount: number = 0;
  @Prop oreLowCount: number = 0;
  @Prop oreMidCount: number = 0;
  @Prop oreHighCount: number = 0;
  @Prop expSmallCount: number = 0;
  @Prop expMediumCount: number = 0;
  @Prop expLargeCount: number = 0;
  // 圣遗物清单（平行数组，快照驱动）。
  @Prop artifactInstanceIds: number[] = [];
  @Prop artifactDefIds: number[] = [];
  @Prop artifactRarities: number[] = [];
  @Prop artifactLevels: number[] = [];
  @Prop artifactEquippedBy: number[] = [];
  // 冒险等级（原神式）。
  @Prop adventureRank: number = 1;
  @Prop adventureExp: number = 0;
  @Prop adventureExpRequired: number = 375;
  @Prop worldLevel: number = 0;
  @Prop activeCharacterId: number = 1;
  @State tab: number = 0;
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

  // 圣遗物名称：defId = setId*10 + slot。
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

  private artifactSetName(setId: number): string {
    switch (setId) {
      case 1: return '辉辉骑士';
      case 2: return '潮鸣之诗';
      default: return '蚀痕残响';
    }
  }

  private rarityColor(rarity: number): string {
    if (rarity >= 5) return '#E5B84A';
    if (rarity === 4) return '#B08FE5';
    return '#9FB6C6';
  }

  // 冒险等阶奖励表：与 native AdventureRank::rankRewards 对齐。
  private rankRewardRanks(): number[] {
    return [2, 5, 10, 15, 20, 25, 30, 35, 40, 45, 50, 55, 60];
  }

  private rankRewardLabel(rank: number): string {
    switch (rank) {
      case 2: return '金币200 · 契约2 · 经验书2';
      case 5: return '金币400 · 契约3 · 经验书4 · 源晶1';
      case 10: return '金币800 · 契约4 · 经验书6 · 源晶2';
      case 15: return '金币1200 · 契约5 · 经验书8 · 源晶2';
      case 20: return '金币1600 · 契约6 · 经验书10 · 源晶3';
      case 25: return '金币2000 · 契约6 · 经验书12 · 源晶3';
      case 30: return '金币2400 · 契约8 · 经验书14 · 源晶4';
      case 35: return '金币2800 · 契约8 · 经验书16 · 源晶4';
      case 40: return '金币3200 · 契约10 · 经验书18 · 源晶5';
      case 45: return '金币3600 · 契约10 · 经验书20 · 源晶5';
      case 50: return '金币4000 · 契约12 · 经验书22 · 源晶6';
      case 55: return '金币4400 · 契约12 · 经验书24 · 源晶6';
      default: return '金币5000 · 契约15 · 经验书30 · 源晶8';
    }
  }

  build() {
    if (this.open) {
      Column({ space: 12 }) {
        // 标题栏。
        Row() {
          Text('背包')
            .fontSize(18)
            .fontWeight(FontWeight.Bold)
            .fontColor('#F2E0B8')
          Blank()
          Text(`金币 ${this.goldCount} · 契约 ${this.fateCount}`)
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

        // 页签：货币 / 材料 / 圣遗物 / 冒险。
        Row({ space: 8 }) {
          Button('货币')
            .width(72).height(32).fontSize(13)
            .type(ButtonType.Normal).borderRadius(16)
            .backgroundColor(this.tab === 0 ? '#7A5A22' : '#2A3441')
            .fontColor(this.tab === 0 ? '#F2E0B8' : '#C7D0CE')
            .stateEffect(true)
            .hitTestBehavior(HitTestMode.Block)
            .onClick(() => { this.tab = 0; })
          Button('材料')
            .width(72).height(32).fontSize(13)
            .type(ButtonType.Normal).borderRadius(16)
            .backgroundColor(this.tab === 1 ? '#7A5A22' : '#2A3441')
            .fontColor(this.tab === 1 ? '#F2E0B8' : '#C7D0CE')
            .stateEffect(true)
            .hitTestBehavior(HitTestMode.Block)
            .onClick(() => { this.tab = 1; })
          Button('圣遗物')
            .width(72).height(32).fontSize(13)
            .type(ButtonType.Normal).borderRadius(16)
            .backgroundColor(this.tab === 2 ? '#7A5A22' : '#2A3441')
            .fontColor(this.tab === 2 ? '#F2E0B8' : '#C7D0CE')
            .stateEffect(true)
            .hitTestBehavior(HitTestMode.Block)
            .onClick(() => { this.tab = 2; })
          Button('冒险')
            .width(72).height(32).fontSize(13)
            .type(ButtonType.Normal).borderRadius(16)
            .backgroundColor(this.tab === 3 ? '#7A5A22' : '#2A3441')
            .fontColor(this.tab === 3 ? '#F2E0B8' : '#C7D0CE')
            .stateEffect(true)
            .hitTestBehavior(HitTestMode.Block)
            .onClick(() => { this.tab = 3; })
        }
        .width('100%')

        if (this.tab === 0) {
          // 货币页签。
          Scroll() {
            Column({ space: 8 }) {
              Row({ space: 8 }) {
                Text('共鸣之契').fontSize(13).fontColor('#E5ECEA').layoutWeight(1)
                Text(`${this.fateCount}`).fontSize(13).fontColor('#E5B84A')
              }
              .width('100%').padding(8)
              .backgroundColor('rgba(20, 26, 38, 0.7)').borderRadius(10)
              Row({ space: 8 }) {
                Text('脉金币').fontSize(13).fontColor('#E5ECEA').layoutWeight(1)
                Text(`${this.goldCount}`).fontSize(13).fontColor('#E5B84A')
              }
              .width('100%').padding(8)
              .backgroundColor('rgba(20, 26, 38, 0.7)').borderRadius(10)
            }
            .width('100%')
          }
          .width('100%')
          .layoutWeight(1)
        } else if (this.tab === 1) {
          // 材料页签：经验书可直接使用给出战角色。
          Text(`经验书使用目标：${this.characterName(this.activeCharacterId)}`)
            .fontSize(11)
            .fontColor('rgba(199, 208, 206, 0.8)')
            .width('100%')
          Scroll() {
            Column({ space: 8 }) {
              Row({ space: 8 }) {
                Column({ space: 2 }) {
                  Text('辉光经验').fontSize(13).fontColor('#E5ECEA')
                  Text('每份 10 经验').fontSize(10).fontColor('#9FB6C6')
                }
                .alignItems(HorizontalAlign.Start).layoutWeight(1)
                Text(`${this.expMaterialCount}`).fontSize(13).fontColor('#E5B84A')
              }
              .width('100%').padding(8)
              .backgroundColor('rgba(20, 26, 38, 0.7)').borderRadius(10)
              Row({ space: 8 }) {
                Column({ space: 2 }) {
                  Text('源晶碎片').fontSize(13).fontColor('#E5ECEA')
                  Text('角色与武器突破材料').fontSize(10).fontColor('#9FB6C6')
                }
                .alignItems(HorizontalAlign.Start).layoutWeight(1)
                Text(`${this.ascensionMaterialCount}`).fontSize(13).fontColor('#E5B84A')
              }
              .width('100%').padding(8)
              .backgroundColor('rgba(20, 26, 38, 0.7)').borderRadius(10)
              Row({ space: 8 }) {
                Column({ space: 2 }) {
                  Text('流浪者笔记').fontSize(13).fontColor('#E5ECEA')
                  Text('每本 1000 经验').fontSize(10).fontColor('#9FB6C6')
                }
                .alignItems(HorizontalAlign.Start).layoutWeight(1)
                Text(`${this.expSmallCount}`).fontSize(13).fontColor('#E5B84A')
                Button('使用×1')
                  .width(64).height(28).fontSize(11)
                  .type(ButtonType.Normal).borderRadius(14)
                  .backgroundColor(this.expSmallCount >= 1 ? '#1F5A50' : '#343A40')
                  .fontColor(this.expSmallCount >= 1 ? '#B8F2E6' : '#7B8288')
                  .stateEffect(true)
                  .hitTestBehavior(HitTestMode.Block)
                  .onClick(() => { useExpItem(this.activeCharacterId, 8, 1); })
                Button('使用×5')
                  .width(64).height(28).fontSize(11)
                  .type(ButtonType.Normal).borderRadius(14)
                  .backgroundColor(this.expSmallCount >= 5 ? '#1F5A50' : '#343A40')
                  .fontColor(this.expSmallCount >= 5 ? '#B8F2E6' : '#7B8288')
                  .stateEffect(true)
                  .hitTestBehavior(HitTestMode.Block)
                  .onClick(() => { useExpItem(this.activeCharacterId, 8, 5); })
              }
              .width('100%').padding(8)
              .backgroundColor('rgba(20, 26, 38, 0.7)').borderRadius(10)
              Row({ space: 8 }) {
                Column({ space: 2 }) {
                  Text('冒险家手册').fontSize(13).fontColor('#E5ECEA')
                  Text('每本 5000 经验').fontSize(10).fontColor('#9FB6C6')
                }
                .alignItems(HorizontalAlign.Start).layoutWeight(1)
                Text(`${this.expMediumCount}`).fontSize(13).fontColor('#E5B84A')
                Button('使用×1')
                  .width(64).height(28).fontSize(11)
                  .type(ButtonType.Normal).borderRadius(14)
                  .backgroundColor(this.expMediumCount >= 1 ? '#1F5A50' : '#343A40')
                  .fontColor(this.expMediumCount >= 1 ? '#B8F2E6' : '#7B8288')
                  .stateEffect(true)
                  .hitTestBehavior(HitTestMode.Block)
                  .onClick(() => { useExpItem(this.activeCharacterId, 9, 1); })
              }
              .width('100%').padding(8)
              .backgroundColor('rgba(20, 26, 38, 0.7)').borderRadius(10)
              Row({ space: 8 }) {
                Column({ space: 2 }) {
                  Text('英雄之证').fontSize(13).fontColor('#E5ECEA')
                  Text('每本 20000 经验').fontSize(10).fontColor('#9FB6C6')
                }
                .alignItems(HorizontalAlign.Start).layoutWeight(1)
                Text(`${this.expLargeCount}`).fontSize(13).fontColor('#E5B84A')
                Button('使用×1')
                  .width(64).height(28).fontSize(11)
                  .type(ButtonType.Normal).borderRadius(14)
                  .backgroundColor(this.expLargeCount >= 1 ? '#1F5A50' : '#343A40')
                  .fontColor(this.expLargeCount >= 1 ? '#B8F2E6' : '#7B8288')
                  .stateEffect(true)
                  .hitTestBehavior(HitTestMode.Block)
                  .onClick(() => { useExpItem(this.activeCharacterId, 10, 1); })
              }
              .width('100%').padding(8)
              .backgroundColor('rgba(20, 26, 38, 0.7)').borderRadius(10)
              Row({ space: 8 }) {
                Column({ space: 2 }) {
                  Text('粗磨矿石').fontSize(13).fontColor('#E5ECEA')
                  Text('武器经验 400').fontSize(10).fontColor('#9FB6C6')
                }
                .alignItems(HorizontalAlign.Start).layoutWeight(1)
                Text(`${this.oreLowCount}`).fontSize(13).fontColor('#E5B84A')
              }
              .width('100%').padding(8)
              .backgroundColor('rgba(20, 26, 38, 0.7)').borderRadius(10)
              Row({ space: 8 }) {
                Column({ space: 2 }) {
                  Text('精锻矿石').fontSize(13).fontColor('#E5ECEA')
                  Text('武器经验 2000').fontSize(10).fontColor('#9FB6C6')
                }
                .alignItems(HorizontalAlign.Start).layoutWeight(1)
                Text(`${this.oreMidCount}`).fontSize(13).fontColor('#E5B84A')
              }
              .width('100%').padding(8)
              .backgroundColor('rgba(20, 26, 38, 0.7)').borderRadius(10)
              Row({ space: 8 }) {
                Column({ space: 2 }) {
                  Text('魔晶矿石').fontSize(13).fontColor('#E5ECEA')
                  Text('武器经验 10000').fontSize(10).fontColor('#9FB6C6')
                }
                .alignItems(HorizontalAlign.Start).layoutWeight(1)
                Text(`${this.oreHighCount}`).fontSize(13).fontColor('#E5B84A')
              }
              .width('100%').padding(8)
              .backgroundColor('rgba(20, 26, 38, 0.7)').borderRadius(10)
            }
            .width('100%')
          }
          .width('100%')
          .layoutWeight(1)
        } else if (this.tab === 2) {
          // 圣遗物页签：列表快捷装备到出战角色（强化在圣遗物面板）。
          Text(`装备目标：${this.characterName(this.activeCharacterId)} · 强化请前往圣遗物面板`)
            .fontSize(11)
            .fontColor('rgba(199, 208, 206, 0.8)')
            .width('100%')
          if (this.artifactInstanceIds.length === 0) {
            Text('暂无圣遗物：击败 Boss 必掉五星圣遗物')
              .fontSize(12)
              .fontColor('rgba(199, 208, 206, 0.6)')
              .width('100%')
              .margin({ top: 16 })
          }
          Scroll() {
            Column({ space: 8 }) {
              ForEach(this.artifactInstanceIds, (instanceId: number, index: number) => {
                Row({ space: 8 }) {
                  Column({ space: 2 }) {
                    Text(this.artifactName(this.artifactDefIds[index]))
                      .fontSize(13)
                      .fontColor(this.rarityColor(this.artifactRarities[index]))
                    Text(`Lv.${this.artifactLevels[index]} · ${this.artifactRarities[index]}星`)
                      .fontSize(11)
                      .fontColor('#9FB6C6')
                    Text(this.artifactEquippedBy[index] > 0
                      ? `已装备：${this.characterName(this.artifactEquippedBy[index])}`
                      : '未装备')
                      .fontSize(10)
                      .fontColor(this.artifactEquippedBy[index] > 0
                        ? '#E5B84A' : 'rgba(159, 182, 198, 0.7)')
                  }
                  .alignItems(HorizontalAlign.Start)
                  .layoutWeight(1)
                  Button('装备')
                    .width(60).height(30).fontSize(11)
                    .type(ButtonType.Normal).borderRadius(15)
                    .backgroundColor(this.artifactEquippedBy[index] === this.activeCharacterId
                      ? '#343A40' : '#7A5A22')
                    .fontColor(this.artifactEquippedBy[index] === this.activeCharacterId
                      ? '#7B8288' : '#F2E0B8')
                    .stateEffect(true)
                    .hitTestBehavior(HitTestMode.Block)
                    .onClick(() => { equipArtifact(instanceId, this.activeCharacterId); })
                }
                .width('100%')
                .padding(8)
                .backgroundColor('rgba(20, 26, 38, 0.7)')
                .borderRadius(10)
              })
            }
            .width('100%')
          }
          .width('100%')
          .layoutWeight(1)
        } else {
          // 冒险页签：等阶信息与等阶奖励领取（RankPanel）。
          Column({ space: 4 }) {
            Text(`冒险等阶 ${this.adventureRank} · 世界等级 ${this.worldLevel}`)
              .fontSize(14)
              .fontWeight(FontWeight.Bold)
              .fontColor('#F2E0B8')
            Text(`经验 ${this.adventureExp}/${this.adventureExpRequired} · 打怪与秘境可获得冒险经验`)
              .fontSize(11)
              .fontColor('#9FB6C6')
          }
          .alignItems(HorizontalAlign.Start)
          .width('100%')
          Scroll() {
            Column({ space: 8 }) {
              ForEach(this.rankRewardRanks(), (rank: number) => {
                Row({ space: 8 }) {
                  Column({ space: 2 }) {
                    Text(`等阶 ${rank}`)
                      .fontSize(13)
                      .fontColor(this.adventureRank >= rank ? '#E5ECEA' : '#7B8288')
                    Text(this.rankRewardLabel(rank))
                      .fontSize(10)
                      .fontColor('#9FB6C6')
                  }
                  .alignItems(HorizontalAlign.Start)
                  .layoutWeight(1)
                  Button('领取')
                    .width(60).height(30).fontSize(11)
                    .type(ButtonType.Normal).borderRadius(15)
                    .backgroundColor(this.adventureRank >= rank ? '#7A5A22' : '#343A40')
                    .fontColor(this.adventureRank >= rank ? '#F2E0B8' : '#7B8288')
                    .stateEffect(true)
                    .hitTestBehavior(HitTestMode.Block)
                    .onClick(() => { claimRankReward(rank); })
                }
                .width('100%')
                .padding(8)
                .backgroundColor('rgba(20, 26, 38, 0.7)')
                .borderRadius(10)
              })
            }
            .width('100%')
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

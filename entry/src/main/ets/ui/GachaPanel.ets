import { performGacha, performWeaponGacha, useExpMaterial, ascendCharacter, upgradeWeapon, equipWeapon,
  upgradeWeaponWithOre, ascendWeapon, refineWeapon } from '../napi/Bridge';

// 抽卡与养成面板（阶段三）：
// 召唤页签展示卡池与最近抽取结果；角色页签提供升级与突破。
// 样式避开 linearGradient/mask 以规避已知 ArkTS 兼容性问题。
@Component
export struct GachaPanel {
  @Prop open: boolean = false;
  @Prop fateCount: number = 0;
  @Prop goldCount: number = 0;
  @Prop expMaterialCount: number = 0;
  @Prop ascensionMaterialCount: number = 0;
  @Prop gachaPity5: number = 0;
  @Prop gachaResultIds: number[] = [];
  @Prop gachaResultRarities: number[] = [];
  @Prop gachaResultIsNew: number[] = [];
  @Prop rosterIds: number[] = [];
  @Prop rosterLevels: number[] = [];
  @Prop rosterAscensions: number[] = [];
  @Prop rosterHp: number[] = [];
  @Prop rosterAtk: number[] = [];
  // 养成深化：命之座与武器清单。
  @Prop rosterConstellations: number[] = [];
  @Prop weaponIds: number[] = [];
  @Prop weaponLevels: number[] = [];
  @Prop weaponEquippedBy: number[] = [];
  // 武器深化（原神式）：突破/精炼/精炼素材与矿石计数。
  @Prop weaponAscensions: number[] = [];
  @Prop weaponRefines: number[] = [];
  @Prop weaponRefineStocks: number[] = [];
  @Prop oreLowCount: number = 0;
  @Prop oreMidCount: number = 0;
  @Prop oreHighCount: number = 0;
  @Prop activeCharacterId: number = 1;
  // 当前抽卡结果所属卡池（0=角色 1=武器），由快照驱动。
  @Prop gachaPoolKind: number = 0;
  @State tab: number = 0;
  // 本地选择的召唤卡池（0=角色池 1=武器池）。
  @State selectedPool: number = 0;
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

  private weaponName(id: number): string {
    switch (id) {
      case 1: return '辉印之章';
      case 2: return '涟漪之刃';
      case 3: return '蚀灭之镰';
      case 4: return '雾猎之弓';
      case 5: return '守望大剑';
      default: return '未知武器';
    }
  }

  // 与 native WeaponSystem::atkOf 公式一致：五星的基础48成长8，四星的基础40成长5。
  private weaponAtk(id: number, level: number): number {
    const fiveStar = id >= 1 && id <= 3;
    return (fiveStar ? 48 : 40) + (level - 1) * (fiveStar ? 8 : 5);
  }

  // 突破阶段等级上限：与 native WeaponSystem::levelCap 一致。
  private weaponLevelCap(ascension: number): number {
    const caps = [20, 40, 50, 60, 70, 80, 90];
    const index = Math.max(0, Math.min(ascension, 6));
    return caps[index];
  }

  // 突破消耗（与 native 公式一致）。
  private weaponAscMaterialCost(ascension: number): number {
    return 2 + Math.max(0, Math.min(ascension, 5));
  }

  private weaponAscGoldCost(ascension: number): number {
    return 100 + Math.max(0, Math.min(ascension, 5)) * 100;
  }

  // 矿石强化：优先使用高档矿石。
  private bestOreId(): number {
    if (this.oreHighCount > 0) return 7;
    if (this.oreMidCount > 0) return 6;
    return 5;
  }

  private totalOre(): number {
    return this.oreLowCount + this.oreMidCount + this.oreHighCount;
  }

  private rarityColor(rarity: number): string {
    return rarity === 5 ? '#E5B84A' : '#9FB6C6';
  }

  // 抽卡结果名称：按结果所属卡池区分角色/武器。
  private resultName(id: number): string {
    return this.gachaPoolKind === 1 ? this.weaponName(id) : this.characterName(id);
  }

  build() {
    if (this.open) {
      Column({ space: 12 }) {
        // 标题栏。
        Row() {
          Text('共鸣召唤')
            .fontSize(18)
            .fontWeight(FontWeight.Bold)
            .fontColor('#F2E0B8')
          Blank()
          Text(`契约 ${this.fateCount} · 金币 ${this.goldCount}`)
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

        // 页签。
        Row({ space: 8 }) {
          Button('召唤')
            .width(80).height(32).fontSize(13)
            .type(ButtonType.Normal).borderRadius(16)
            .backgroundColor(this.tab === 0 ? '#7A5A22' : '#2A3441')
            .fontColor(this.tab === 0 ? '#F2E0B8' : '#C7D0CE')
            .stateEffect(true)
            .hitTestBehavior(HitTestMode.Block)
            .onClick(() => { this.tab = 0; })
          Button('角色')
            .width(80).height(32).fontSize(13)
            .type(ButtonType.Normal).borderRadius(16)
            .backgroundColor(this.tab === 1 ? '#7A5A22' : '#2A3441')
            .fontColor(this.tab === 1 ? '#F2E0B8' : '#C7D0CE')
            .stateEffect(true)
            .hitTestBehavior(HitTestMode.Block)
            .onClick(() => { this.tab = 1; })
          Button('武器')
            .width(80).height(32).fontSize(13)
            .type(ButtonType.Normal).borderRadius(16)
            .backgroundColor(this.tab === 2 ? '#7A5A22' : '#2A3441')
            .fontColor(this.tab === 2 ? '#F2E0B8' : '#C7D0CE')
            .stateEffect(true)
            .hitTestBehavior(HitTestMode.Block)
            .onClick(() => { this.tab = 2; })
        }
        .width('100%')

        if (this.tab === 0) {
          // 卡池说明与保底计数。
          Text(`五星 0.6%（${Math.max(90 - this.gachaPity5, 0)} 抽内保底）· 四星及以上 100%`)
            .fontSize(11)
            .fontColor('rgba(199, 208, 206, 0.8)')
            .width('100%')
          Text('重复角色自动转化：五星返契约×4+金币200，四星返契约×2+金币50')
            .fontSize(10)
            .fontColor('rgba(199, 208, 206, 0.6)')
            .width('100%')

          // 卡池切换（抽卡优化）：角色池与武器池。
          Row({ space: 8 }) {
            Button('角色池')
              .width(90).height(28).fontSize(12)
              .type(ButtonType.Normal).borderRadius(14)
              .backgroundColor(this.selectedPool === 0 ? '#7A5A22' : '#2A3441')
              .fontColor(this.selectedPool === 0 ? '#F2E0B8' : '#C7D0CE')
              .stateEffect(true)
              .hitTestBehavior(HitTestMode.Block)
              .onClick(() => { this.selectedPool = 0; })
            Button('武器池 0.75%')
              .width(110).height(28).fontSize(12)
              .type(ButtonType.Normal).borderRadius(14)
              .backgroundColor(this.selectedPool === 1 ? '#7A5A22' : '#2A3441')
              .fontColor(this.selectedPool === 1 ? '#F2E0B8' : '#C7D0CE')
              .stateEffect(true)
              .hitTestBehavior(HitTestMode.Block)
              .onClick(() => { this.selectedPool = 1; })
          }
          .width('100%')

          Row({ space: 12 }) {
            Button('单抽 ×1')
              .width(110).height(40).fontSize(14)
              .type(ButtonType.Normal).borderRadius(20)
              .backgroundColor(this.fateCount >= 1 ? '#7A5A22' : '#343A40')
              .fontColor(this.fateCount >= 1 ? '#F2E0B8' : '#7B8288')
              .fontWeight(FontWeight.Bold)
              .stateEffect(true)
              .hitTestBehavior(HitTestMode.Block)
              .onClick(() => {
                if (this.selectedPool === 1) { performWeaponGacha(1); } else { performGacha(1); }
              })
            Button('十连 ×10')
              .width(110).height(40).fontSize(14)
              .type(ButtonType.Normal).borderRadius(20)
              .backgroundColor(this.fateCount >= 10 ? '#C89B3C' : '#343A40')
              .fontColor(this.fateCount >= 10 ? '#1A1408' : '#7B8288')
              .fontWeight(FontWeight.Bold)
              .stateEffect(true)
              .hitTestBehavior(HitTestMode.Block)
              .onClick(() => {
                if (this.selectedPool === 1) { performWeaponGacha(10); } else { performGacha(10); }
              })
          }
          .width('100%')

          // 最近抽取结果。
          Scroll() {
            Flex({ wrap: FlexWrap.Wrap }) {
              ForEach(this.gachaResultIds, (id: number, index: number) => {
                Column({ space: 2 }) {
                  Text(this.gachaResultRarities[index] === 5 ? '★★★★★' : '★★★★')
                    .fontSize(10)
                    .fontColor(this.rarityColor(this.gachaResultRarities[index]))
                  Text(this.resultName(id))
                    .fontSize(11)
                    .fontColor('#E5ECEA')
                    .textAlign(TextAlign.Center)
                  if (this.gachaResultIsNew[index] === 1) {
                    Text('NEW')
                      .fontSize(9)
                      .fontColor('#1A1408')
                      .backgroundColor('#E5B84A')
                      .borderRadius(6)
                      .padding({ left: 6, right: 6, top: 1, bottom: 1 })
                  } else {
                    Text('转化')
                      .fontSize(9)
                      .fontColor('#C7D0CE')
                      .backgroundColor('rgba(199, 208, 206, 0.25)')
                      .borderRadius(6)
                      .padding({ left: 6, right: 6, top: 1, bottom: 1 })
                  }
                }
                .width(96)
                .padding(6)
                .backgroundColor('rgba(20, 26, 38, 0.7)')
                .borderRadius(10)
                .border({
                  width: 1,
                  color: this.rarityColor(this.gachaResultRarities[index])
                })
                .margin(4)
              })
            }
            .width('100%')
          }
          .width('100%')
          .layoutWeight(1)
        } else if (this.tab === 1) {
          // 材料行。
          Text(`经验 ${this.expMaterialCount} · 源晶 ${this.ascensionMaterialCount} · 金币 ${this.goldCount}`)
            .fontSize(11)
            .fontColor('rgba(199, 208, 206, 0.8)')
            .width('100%')

          Scroll() {
            Column({ space: 8 }) {
              ForEach(this.rosterIds, (id: number, index: number) => {
                Row({ space: 8 }) {
                  Column({ space: 2 }) {
                    Text(this.characterName(id))
                      .fontSize(13)
                      .fontColor('#E5ECEA')
                    Text(`Lv.${this.rosterLevels[index]} · 突破 ${this.rosterAscensions[index]}`)
                      .fontSize(11)
                      .fontColor('#9FB6C6')
                    Text(`生命 ${this.rosterHp[index] ?? 0} · 攻击 ${this.rosterAtk[index] ?? 0}`)
                      .fontSize(10)
                      .fontColor('rgba(159, 182, 198, 0.75)')
                    if ((this.rosterConstellations[index] ?? 0) > 0) {
                      Text(`命之座 ${this.rosterConstellations[index]}`)
                        .fontSize(10)
                        .fontColor('#E5B84A')
                    }
                  }
                  .alignItems(HorizontalAlign.Start)
                  .layoutWeight(1)
                  Button('升级×5')
                    .width(72).height(30).fontSize(11)
                    .type(ButtonType.Normal).borderRadius(15)
                    .backgroundColor(this.expMaterialCount >= 5 ? '#1F5A50' : '#343A40')
                    .fontColor(this.expMaterialCount >= 5 ? '#B8F2E6' : '#7B8288')
                    .stateEffect(true)
                    .hitTestBehavior(HitTestMode.Block)
                    .onClick(() => { useExpMaterial(id, 5); })
                  Button('突破')
                    .width(60).height(30).fontSize(11)
                    .type(ButtonType.Normal).borderRadius(15)
                    .backgroundColor(this.ascensionMaterialCount >= 2 && this.goldCount >= 100
                      ? '#7A5A22' : '#343A40')
                    .fontColor(this.ascensionMaterialCount >= 2 && this.goldCount >= 100
                      ? '#F2E0B8' : '#7B8288')
                    .stateEffect(true)
                    .hitTestBehavior(HitTestMode.Block)
                    .onClick(() => { ascendCharacter(id); })
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
          // 武器页签：强化、突破、精炼与装备。
          Text(`金币 ${this.goldCount} · 矿石 ${this.totalOre()} · 源晶 ${this.ascensionMaterialCount} · 装备到 ${this.characterName(this.activeCharacterId)}`)
            .fontSize(11)
            .fontColor('rgba(199, 208, 206, 0.8)')
            .width('100%')

          if (this.weaponIds.length === 0) {
            Text('暂无武器：完成主线任务获得奖励武器')
              .fontSize(12)
              .fontColor('rgba(199, 208, 206, 0.6)')
              .width('100%')
              .margin({ top: 16 })
          }

          Scroll() {
            Column({ space: 8 }) {
              ForEach(this.weaponIds, (id: number, index: number) => {
                Row({ space: 8 }) {
                  Column({ space: 2 }) {
                    Text(`${this.weaponName(id)} · 精炼 ${this.weaponRefines[index] ?? 1}`)
                      .fontSize(13)
                      .fontColor('#E5ECEA')
                    Text(`Lv.${this.weaponLevels[index]}/${this.weaponLevelCap(this.weaponAscensions[index] ?? 0)} · 突破 ${this.weaponAscensions[index] ?? 0} · 攻击 ${this.weaponAtk(id, this.weaponLevels[index])}`)
                      .fontSize(11)
                      .fontColor('#9FB6C6')
                    Text(`${(this.weaponRefineStocks[index] ?? 0) > 0
                      ? `精炼素材 ×${this.weaponRefineStocks[index]} · ` : ''}${this.weaponEquippedBy[index] > 0
                      ? `已装备：${this.characterName(this.weaponEquippedBy[index])}`
                      : '未装备'}`)
                      .fontSize(10)
                      .fontColor(this.weaponEquippedBy[index] > 0
                        ? '#E5B84A' : 'rgba(159, 182, 198, 0.7)')
                  }
                  .alignItems(HorizontalAlign.Start)
                  .layoutWeight(1)
                  Button('矿石强化')
                    .width(76).height(30).fontSize(11)
                    .type(ButtonType.Normal).borderRadius(15)
                    .backgroundColor(this.totalOre() > 0
                      ? '#1F5A50' : '#343A40')
                    .fontColor(this.totalOre() > 0
                      ? '#B8F2E6' : '#7B8288')
                    .stateEffect(true)
                    .hitTestBehavior(HitTestMode.Block)
                    .onClick(() => { upgradeWeaponWithOre(id, this.bestOreId(), 1); })
                  Button(`突破 ${this.weaponAscMaterialCost(this.weaponAscensions[index] ?? 0)}晶`)
                    .width(76).height(30).fontSize(11)
                    .type(ButtonType.Normal).borderRadius(15)
                    .backgroundColor(this.ascensionMaterialCount >=
                        this.weaponAscMaterialCost(this.weaponAscensions[index] ?? 0) &&
                      this.goldCount >= this.weaponAscGoldCost(this.weaponAscensions[index] ?? 0)
                      ? '#7A5A22' : '#343A40')
                    .fontColor(this.ascensionMaterialCount >=
                        this.weaponAscMaterialCost(this.weaponAscensions[index] ?? 0) &&
                      this.goldCount >= this.weaponAscGoldCost(this.weaponAscensions[index] ?? 0)
                      ? '#F2E0B8' : '#7B8288')
                    .stateEffect(true)
                    .hitTestBehavior(HitTestMode.Block)
                    .onClick(() => { ascendWeapon(id); })
                  Button('精炼')
                    .width(52).height(30).fontSize(11)
                    .type(ButtonType.Normal).borderRadius(15)
                    .backgroundColor((this.weaponRefineStocks[index] ?? 0) > 0
                      ? '#5E3E6E' : '#343A40')
                    .fontColor((this.weaponRefineStocks[index] ?? 0) > 0
                      ? '#E0D0F2' : '#7B8288')
                    .stateEffect(true)
                    .hitTestBehavior(HitTestMode.Block)
                    .onClick(() => { refineWeapon(id); })
                  Button('装备')
                    .width(52).height(30).fontSize(11)
                    .type(ButtonType.Normal).borderRadius(15)
                    .backgroundColor(this.weaponEquippedBy[index] === this.activeCharacterId
                      ? '#343A40' : '#7A5A22')
                    .fontColor(this.weaponEquippedBy[index] === this.activeCharacterId
                      ? '#7B8288' : '#F2E0B8')
                    .stateEffect(true)
                    .hitTestBehavior(HitTestMode.Block)
                    .onClick(() => { equipWeapon(id, this.activeCharacterId); })
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

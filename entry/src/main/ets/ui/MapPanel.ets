import { teleportToAnchor } from '../napi/Bridge';

// 大地图面板（优化批次）：原神式地图选点快速传送。
// 复用快照锚点坐标/解锁状态绘制 2D 地图，点击已解锁锚点即传送。
// 样式避开 linearGradient/mask 以规避已知 ArkTS 兼容性问题。
@Component
export struct MapPanel {
  @Prop open: boolean = false;
  @Prop minimapAnchorX: number[] = [];
  @Prop minimapAnchorY: number[] = [];
  @Prop minimapAnchorUnlocked: number[] = [];
  onClose: () => void = () => {};

  private anchorName(index: number): string {
    switch (index) {
      case 0: return '遗迹营地';
      case 1: return '共鸣祭坛';
      case 2: return '西侧断崖';
      case 3: return '东侧石阵';
      case 4: return '北部高地';
      case 5: return '雾谷入口';
      case 6: return '沉没湖岸';
      default: return '锚点';
    }
  }

  build() {
    if (this.open) {
      Column({ space: 10 }) {
        // 标题栏。
        Row() {
          Text('艾瑟兰大陆')
            .fontSize(18)
            .fontWeight(FontWeight.Bold)
            .fontColor('#F2E0B8')
          Blank()
          Text('点击金色锚点传送')
            .fontSize(11)
            .fontColor('rgba(199, 208, 206, 0.8)')
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

        // 地图画布：世界坐标 [0,1] 映射到画布区域。
        Stack() {
          Column()
            .width('100%')
            .height('100%')
            .backgroundColor('rgba(18, 28, 34, 0.9)')
            .borderRadius(14)
            .border({ width: 1, color: 'rgba(199, 208, 206, 0.25)' })

          ForEach(this.minimapAnchorX, (ax: number, index: number) => {
            Column({ space: 3 }) {
              Circle()
                .width(this.minimapAnchorUnlocked[index] === 1 ? 18 : 12)
                .height(this.minimapAnchorUnlocked[index] === 1 ? 18 : 12)
                .fill(this.minimapAnchorUnlocked[index] === 1
                  ? '#E5B84A' : 'rgba(199, 208, 206, 0.45)')
              Text(this.anchorName(index))
                .fontSize(10)
                .fontColor(this.minimapAnchorUnlocked[index] === 1
                  ? '#F2E0B8' : 'rgba(199, 208, 206, 0.6)')
            }
            .position({
              x: `${Math.max(4, Math.min(88, ax * 92 + 4))}%`,
              y: `${Math.max(6, Math.min(84, (this.minimapAnchorY[index] ?? 0) * 88 + 6))}%`
            })
            .hitTestBehavior(this.minimapAnchorUnlocked[index] === 1
              ? HitTestMode.Block : HitTestMode.Transparent)
            .onClick(() => {
              if (this.minimapAnchorUnlocked[index] === 1) {
                // 锚点 id = 布局序号 + 1。
                if (teleportToAnchor(index + 1)) {
                  this.onClose();
                }
              }
            })
          })
        }
        .width('100%')
        .layoutWeight(1)
      }
      .width('88%')
      .height('76%')
      .padding(16)
      .backgroundColor('rgba(8, 12, 20, 0.94)')
      .borderRadius(18)
      .border({ width: 1, color: 'rgba(91, 143, 168, 0.4)' })
      .position({ x: '50%', y: '50%' })
      .markAnchor({ x: '50%', y: '50%' })
    }
  }
}

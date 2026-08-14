// 扫描 config/ 与 assets/ 名称,标记与参考作品专有称呼重叠的条目(spec §2.4)
console.log("ORIGINALITY CHECK: 对比命名/视觉/系统清单,输出评审记录");

// 世界布局事实来源校验：assets/world/world.json 存在且基础结构合法
// （Phase 0 数据管线；详细校验与 C++ 头生成见 automation/assets/generate_world_layout.mjs）
const fs = require('fs');
const path = require('path');

function failWorldLayout(message) {
  console.error(`WORLD LAYOUT CHECK FAIL: ${message}`);
  process.exitCode = 1;
}

(function checkWorldLayout() {
  const root = path.resolve(__dirname, '../..');
  const worldPath = path.join(root, 'assets/world/world.json');
  if (!fs.existsSync(worldPath)) {
    failWorldLayout('assets/world/world.json 不存在');
    return;
  }
  let world;
  try {
    world = JSON.parse(fs.readFileSync(worldPath, 'utf8'));
  } catch (error) {
    failWorldLayout(`assets/world/world.json 解析失败: ${error.message}`);
    return;
  }
  for (const key of ['districts', 'anchors', 'npcs', 'spawnZones', 'chests', 'collectibles']) {
    if (!Array.isArray(world[key]) || world[key].length === 0) {
      failWorldLayout(`缺少非空数组字段: ${key}`);
    }
  }
  if (process.exitCode === 1) return;
  const coordOk = (value) => typeof value === 'number' && value >= 0.02 && value <= 0.98;
  for (const anchor of world.anchors) {
    if (!Number.isInteger(anchor.id) || anchor.id < 8) failWorldLayout(`锚点 id 必须 ≥8: ${anchor.id}`);
    if (!coordOk(anchor.x) || !coordOk(anchor.y)) failWorldLayout(`锚点 ${anchor.id} 坐标越界`);
  }
  for (const npc of world.npcs) {
    if (!Number.isInteger(npc.id) || npc.id < 32) failWorldLayout(`NPC id 必须 ≥32: ${npc.id}`);
    if (!coordOk(npc.x) || !coordOk(npc.y)) failWorldLayout(`NPC ${npc.id} 坐标越界`);
  }
  for (const chest of world.chests) {
    if (!Number.isInteger(chest.id) || chest.id < 32) failWorldLayout(`宝箱 id 必须 ≥32: ${chest.id}`);
  }
  if (process.exitCode !== 1) {
    console.log(`WORLD LAYOUT CHECK: districts=${world.districts.length} anchors=${world.anchors.length}` +
      ` npcs=${world.npcs.length} spawnZones=${world.spawnZones.length}` +
      ` chests=${world.chests.length} collectibles=${world.collectibles.length} 校验通过`);
  }
})();

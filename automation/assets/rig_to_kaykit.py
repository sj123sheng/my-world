# rig_to_kaykit.py: 把 AI 生成角色 GLB（如 Tripo 导出）一键转换为符合
# docs/asset_spec.md 契约的资产：脱掉源骨架 → 清理减面 → 绑定 KayKit 41 骨
# （带 handslot.r 与 76 条 clip）→ 权重压 4 影响 → 导出 GLB。
#
# 用法（headless，一条命令）：
#   /Applications/Blender.app/Contents/MacOS/Blender -b -P \
#     automation/assets/rig_to_kaykit.py -- \
#     --tripo <AI导出.glb> \
#     --kaykit entry/src/main/resources/rawfile/models/player.glb \
#     --out /tmp/player_new.glb [--target-tris 40000]
#
# 也可在 Blender GUI 的脚本编辑器里直接运行（先填好下方 argv 默认值）。

import argparse
import bpy
import bmesh
import sys
from mathutils import Matrix, Vector


def parse_args():
    argv = sys.argv
    argv = argv[argv.index("--") + 1:] if "--" in argv else []
    p = argparse.ArgumentParser()
    p.add_argument("--tripo", required=True)
    p.add_argument("--kaykit", required=True)
    p.add_argument("--out", required=True)
    p.add_argument("--target-tris", type=int, default=40000)
    return p.parse_args(argv)


def data_bbox(obj):
    vs = [obj.matrix_world @ v.co for v in obj.data.vertices]
    lo = Vector((min(v.x for v in vs), min(v.y for v in vs), min(v.z for v in vs)))
    hi = Vector((max(v.x for v in vs), max(v.y for v in vs), max(v.z for v in vs)))
    return lo, hi


def main():
    args = parse_args()
    bpy.ops.wm.read_factory_settings(use_empty=True)

    # ---- 1. 导入 AI 资产，脱掉源蒙皮 ----
    bpy.ops.import_scene.gltf(filepath=args.tripo)
    # Tripo 桥接插件会往场景塞预览垃圾（Icosphere/棱角球），先清掉
    for o in [o for o in bpy.context.scene.objects
              if o.type == "MESH" and
              (o.name.split(".")[0] == "Icosphere" or "棱角" in o.name)]:
        bpy.data.objects.remove(o, do_unlink=True)
    meshes = [o for o in bpy.context.scene.objects if o.type == "MESH"]
    for o in [o for o in bpy.context.scene.objects if o.type == "ARMATURE"]:
        bpy.data.objects.remove(o, do_unlink=True)
    for m in meshes:
        for mod in [mod for mod in m.modifiers if mod.type == "ARMATURE"]:
            m.modifiers.remove(mod)
        m.vertex_groups.clear()
    assert meshes, "AI GLB 内没有网格"
    if len(meshes) > 1:
        bpy.ops.object.select_all(action="DESELECT")
        for m in meshes:
            m.select_set(True)
        bpy.context.view_layer.objects.active = meshes[0]
        bpy.ops.object.join()
    mesh = meshes[0]

    # 烘焙变换 + 清理（合并重合点/删松散/法线朝外）
    mesh.data.transform(mesh.matrix_world)
    mesh.matrix_world = Matrix.Identity(4)
    bm = bmesh.new()
    bm.from_mesh(mesh.data)
    bmesh.ops.remove_doubles(bm, verts=bm.verts, dist=1e-4)
    loose_v = [v for v in bm.verts if not v.link_faces]
    bmesh.ops.delete(bm, geom=loose_v, context="VERTS")
    loose_e = [e for e in bm.edges if not e.link_faces]
    bmesh.ops.delete(bm, geom=loose_e, context="EDGES")
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    bm.to_mesh(mesh.data)
    bm.free()

    # 可选减面（AI 导出已低于预算时跳过）
    tris = sum(len(p.vertices) - 2 for p in mesh.data.polygons)
    if tris > args.target_tris:
        mod = mesh.modifiers.new("decimate", "DECIMATE")
        mod.ratio = args.target_tris / tris
        bpy.context.view_layer.objects.active = mesh
        bpy.ops.object.modifier_apply(modifier=mod.name)
        tris = sum(len(p.vertices) - 2 for p in mesh.data.polygons)
    print(f"[rig] AI mesh tris={tris}")

    # ---- 2. 导入 KayKit 骨架，删其网格，保留骨架与全部 action ----
    before = set(bpy.data.objects)
    bpy.ops.import_scene.gltf(filepath=args.kaykit)
    for o in [o for o in bpy.data.objects
              if o not in before and o.type == "MESH" and
              (o.name.split(".")[0] == "Icosphere" or "棱角" in o.name)]:
        bpy.data.objects.remove(o, do_unlink=True)
    new = [o for o in bpy.data.objects if o not in before]
    kay_arm = next(o for o in new if o.type == "ARMATURE")
    # 身高基准只认挂在 KayKit 骨架下的蒙皮网格，排除插件垃圾与游离物
    kay_meshes = [o for o in new if o.type == "MESH" and o.parent == kay_arm]
    # KayKit 身高取全部网格（本体+挂件）的并集包围盒，避免拿到单个挂件的高度
    los, his = zip(*[data_bbox(o) for o in kay_meshes])
    kay_lo = Vector((min(l.x for l in los), min(l.y for l in los), min(l.z for l in los)))
    kay_hi = Vector((max(h.x for h in his), max(h.y for h in his), max(h.z for h in his)))
    for o in kay_meshes:
        bpy.data.objects.remove(o, do_unlink=True)
    actions = list(bpy.data.actions)
    for a in actions:
        a.use_fake_user = True  # 防止清 NLA 后被回收
    print(f"[rig] KayKit joints={len(kay_arm.data.bones)} clips={len(actions)}")

    # ---- 3. 对齐：按 KayKit 身高缩放，脚底/水平居中归位 ----
    lo, hi = data_bbox(mesh)
    scale = (kay_hi.z - kay_lo.z) / (hi.z - lo.z)
    mesh.data.transform(Matrix.Scale(scale, 4))
    lo, hi = data_bbox(mesh)
    center = (lo + hi) / 2
    mesh.data.transform(Matrix.Translation(
        Vector((-center.x, -center.y, kay_lo.z - lo.z))))

    # ---- 4. 自动权重绑定 ----
    bpy.ops.object.select_all(action="DESELECT")
    mesh.select_set(True)
    kay_arm.select_set(True)
    bpy.context.view_layer.objects.active = kay_arm
    try:
        bpy.ops.object.parent_set(type="ARMATURE_AUTO")
    except RuntimeError:
        # headless 上下文兜底
        with bpy.context.temp_override(
                view_layer=bpy.context.view_layer, active_object=kay_arm,
                selected_objects=[mesh, kay_arm],
                selected_editable_objects=[mesh, kay_arm]):
            bpy.ops.object.parent_set(type="ARMATURE_AUTO")

    # ---- 5. 权重清理：top-4 + 归一化（纯 Python，headless 稳定） ----
    names = [vg.name for vg in mesh.vertex_groups]
    per_vert = []
    unbound = 0
    for v in mesh.data.vertices:
        ws = sorted(((g.group, g.weight) for g in v.groups),
                    key=lambda t: t[1], reverse=True)[:4]
        total = sum(w for _, w in ws)
        if total <= 1e-6:
            unbound += 1
            ws = [(0, 1.0)]  # 兜底挂到根骨，满足权重和不为 0
        else:
            ws = [(i, w / total) for i, w in ws]
        per_vert.append(ws)
    mesh.vertex_groups.clear()
    groups = [mesh.vertex_groups.new(name=n) for n in names]
    for vid, ws in enumerate(per_vert):
        for gi, w in ws:
            groups[gi].add([vid], w, "REPLACE")
    print(f"[rig] unbound verts fallback={unbound}")

    # ---- 6. 76 条 clip 挂 NLA，导出时一条 track = 一个 clip ----
    ad = kay_arm.animation_data_create()
    ad.action = None
    while ad.nla_tracks:
        ad.nla_tracks.remove(ad.nla_tracks[0])
    for a in actions:
        track = ad.nla_tracks.new()
        track.name = a.name
        track.strips.new(a.name, 0, a)

    # ---- 7. 导出 GLB（场景里此刻只有 mesh + KayKit 骨架） ----
    # 纹理压到规格书要求的 1024（AI 工具常出 2K/4K，移动端包体吃不消）
    for img in bpy.data.images:
        if img.size[0] > 1024 or img.size[1] > 1024:
            img.scale(1024, 1024)
    bpy.ops.object.select_all(action="DESELECT")
    mesh.select_set(True)
    kay_arm.select_set(True)
    bpy.context.view_layer.objects.active = kay_arm
    bpy.ops.export_scene.gltf(
        filepath=args.out, export_format="GLB",
        export_apply=True, export_animations=True,
        export_animation_mode="NLA_TRACKS", export_skins=True,
        export_yup=True)
    print(f"[rig] done -> {args.out}")


main()

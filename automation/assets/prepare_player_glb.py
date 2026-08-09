# prepare_player_glb.py: 把重制的主角 GLB（UE 风格 41 骨、NlaTrack*
# 动画、+X 朝向、带根运动）转换为引擎可直接消费的 player.glb：
#
#   1. 动画按动作语义重命名（NlaTrack* → idle/walk/run/Jump_Idle/
#      glide/cast/Dive/Turn_180/climb），裁剪 Turn_180 与 climb；
#   2. 剥离根运动：水平位移归零（游戏逻辑驱动位移），垂直弹跳按
#      KayKit 约定保留在 Hip 平移通道；跳跃/攀爬整体钉死；
#   3. 循环 clip（walk/run/glide/Jump_Idle）端点不闭合时自动搜索
#      最优循环窗口并裁剪；
#   4. 补 handslot.r 武器挂点关节：世界朝向对齐 KayKit handslot.r
#      （从 enemy.glb 取参考），位置按手部局部偏移复刻；
#   5. 整体绕 Y 旋转 -90°：新模型 +X 前向对齐引擎 +Z 前向约定；
#   6. 合成静态 idle（源资产无待机 clip）。
#
# 用法：
#   python3 automation/assets/prepare_player_glb.py <in.glb> <out.glb> \
#       [--kaykit entry/src/main/resources/rawfile/models/enemy.glb]
#
# 只依赖标准库；写完自动回读校验（骨架/clip/根运动/循环接缝/挂点）。

import json
import math
import struct
import sys

# ---------------------------------------------------------------- GLB IO


def load_glb(path):
    with open(path, "rb") as f:
        data = f.read()
    magic, version, _ = struct.unpack("<III", data[:12])
    assert magic == 0x46546C67 and version == 2, "not a GLB v2"
    json_len = struct.unpack("<I", data[12:16])[0]
    gltf = json.loads(data[20:20 + json_len].decode("utf-8"))
    bin_off = 20 + json_len
    bin_len = struct.unpack("<I", data[bin_off:bin_off + 4])[0]
    return gltf, data[bin_off + 8:bin_off + 8 + bin_len]


def write_glb(path, gltf, bin_chunk):
    payload = json.dumps(gltf, separators=(",", ":")).encode("utf-8")
    while len(payload) % 4 != 0:
        payload += b" "
    while len(bin_chunk) % 4 != 0:
        bin_chunk += b"\x00"
    total = 12 + 8 + len(payload) + 8 + len(bin_chunk)
    with open(path, "wb") as f:
        f.write(struct.pack("<III", 0x46546C67, 2, total))
        f.write(struct.pack("<II", len(payload), 0x4E4F534A))
        f.write(payload)
        f.write(struct.pack("<II", len(bin_chunk), 0x004E4942))
        f.write(bin_chunk)


COMP = {5120: ("b", 1), 5121: ("B", 1), 5122: ("h", 2), 5123: ("H", 2),
        5125: ("I", 4), 5126: ("f", 4)}
COUNT = {"SCALAR": 1, "VEC2": 2, "VEC3": 3, "VEC4": 4, "MAT4": 16}


def decode_accessor(gltf, bin_chunk, idx):
    acc = gltf["accessors"][idx]
    view = gltf["bufferViews"][acc["bufferView"]]
    comp, csize = COMP[acc["componentType"]]
    n = COUNT[acc["type"]]
    stride = view.get("byteStride") or csize * n
    base = view.get("byteOffset", 0) + acc.get("byteOffset", 0)
    out = []
    for i in range(acc["count"]):
        off = base + i * stride
        vals = struct.unpack("<" + comp * n,
                             bin_chunk[off:off + csize * n])
        out.append(list(vals) if n > 1 else vals[0])
    return out


# ------------------------------------------------------------- 四元数数学


def qmul(a, b):
    ax, ay, az, aw = a
    bx, by, bz, bw = b
    return [aw * bx + ax * bw + ay * bz - az * by,
            aw * by - ax * bz + ay * bw + az * bx,
            aw * bz + ax * by - ay * bx + az * bw,
            aw * bw - ax * bx - ay * by - az * bz]


def qconj(a):
    return [-a[0], -a[1], -a[2], a[3]]


def qnorm(a):
    n = math.sqrt(sum(x * x for x in a))
    return [x / n for x in a]


def qrot(q, v):
    x, y, z, w = q
    vx, vy, vz = v
    tx = 2.0 * (y * vz - z * vy)
    ty = 2.0 * (z * vx - x * vz)
    tz = 2.0 * (x * vy - y * vx)
    return [vx + w * tx + y * tz - z * ty,
            vy + w * ty + z * tx - x * tz,
            vz + w * tz + x * ty - y * tx]


def qangle(a, b):
    dot = min(1.0, abs(sum(x * y for x, y in zip(a, b))))
    return 2.0 * math.acos(dot)


def qslerp(a, b, t):
    dot = sum(x * y for x, y in zip(a, b))
    if dot < 0.0:
        b = [-x for x in b]
        dot = -dot
    if dot > 0.9995:
        out = [x + (y - x) * t for x, y in zip(a, b)]
        return qnorm(out)
    theta = math.acos(max(-1.0, min(1.0, dot)))
    s = math.sin(theta)
    sa, sb = math.sin((1.0 - t) * theta) / s, math.sin(t * theta) / s
    return [sa * x + sb * y for x, y in zip(a, b)]


def vsub(a, b):
    return [a[0] - b[0], a[1] - b[1], a[2] - b[2]]


def vadd(a, b):
    return [a[0] + b[0], a[1] + b[1], a[2] + b[2]]


def rigid_inverse_mat4(pos, rot):
    """刚体变换（位置 + 四元数）的逆矩阵，列主序 16 元素（glTF MAT4）。"""
    x, y, z, w = rot
    r = [[1 - 2 * (y * y + z * z), 2 * (x * y - z * w), 2 * (x * z + y * w)],
         [2 * (x * y + z * w), 1 - 2 * (x * x + z * z), 2 * (y * z - x * w)],
         [2 * (x * z - y * w), 2 * (y * z + x * w), 1 - 2 * (x * x + y * y)]]
    # 逆变换 = 转置旋转 + (-R^T * pos)；列主序输出。
    t = [-sum(r[row][col] * pos[row] for row in range(3))
         for col in range(3)]
    flat = []
    for col in range(3):
        flat.extend([r[col][0], r[col][1], r[col][2], 0.0])
    flat.extend([t[0], t[1], t[2], 1.0])
    return flat


# ------------------------------------------------------- 动画通道采样


class ClipChannels:
    """一个动画的全部通道（node, path）→ (times, values, interp)。"""

    def __init__(self, gltf, bin_chunk, anim):
        self.name = anim.get("name", "")
        self.channels = {}
        self.duration = 0.0
        for ch in anim["channels"]:
            sampler = anim["samplers"][ch["sampler"]]
            times = decode_accessor(gltf, bin_chunk, sampler["input"])
            values = decode_accessor(gltf, bin_chunk, sampler["output"])
            key = (ch["target"]["node"], ch["target"]["path"])
            interp = sampler.get("interpolation", "LINEAR")
            self.channels[key] = (times, values, interp)
            self.duration = max(self.duration, times[-1])

    def sample(self, node, path, default, t):
        key = (node, path)
        if key not in self.channels:
            return list(default)
        times, values, interp = self.channels[key]
        if t <= times[0]:
            return list(values[0])
        if t >= times[-1]:
            return list(values[-1])
        hi = 0
        while times[hi] < t:
            hi += 1
        lo = hi - 1
        frac = (t - times[lo]) / max(times[hi] - times[lo], 1e-9)
        if interp == "STEP":
            return list(values[lo])
        if path == "rotation":
            return qslerp(values[lo], values[hi], frac)
        a, b = values[lo], values[hi]
        return [x + (y - x) * frac for x, y in zip(a, b)]


def node_rest_trs(gltf, node):
    nd = gltf["nodes"][node]
    return (list(nd.get("translation", [0.0, 0.0, 0.0])),
            list(nd.get("rotation", [0.0, 0.0, 0.0, 1.0])),
            list(nd.get("scale", [1.0, 1.0, 1.0])))


def root_hip_fk(gltf, clip, t, root_node, hip_node):
    """Armature 空间下 Root 世界旋转与 Hip 世界位置（根运动剥离用）。"""
    rt, rr, _ = node_rest_trs(gltf, root_node)
    root_trans = clip.sample(root_node, "translation", rt, t)
    root_rot = qnorm(clip.sample(root_node, "rotation", rr, t))
    ht, _, _ = node_rest_trs(gltf, hip_node)
    hip_local = clip.sample(hip_node, "translation", ht, t)
    hip_world = vadd(root_trans, qrot(root_rot, hip_local))
    return root_rot, hip_local, hip_world


# ------------------------------------------------------------- 裁剪/剥离


def trim_clip(gltf, clip, t0, t1):
    """把 clip 裁剪到 [t0, t1]：窗口内关键帧 + 边界采样帧，时间归零。"""
    trimmed = {}
    for (node, path), (times, values, interp) in clip.channels.items():
        new_times = [0.0]
        new_values = [clip.sample(node, path,
                                  node_rest_trs(gltf, node)[
                                      {"translation": 0, "rotation": 1,
                                       "scale": 2}[path]], t0)]
        for time, value in zip(times, values):
            if t0 < time < t1:
                new_times.append(time - t0)
                new_values.append(list(value))
        new_times.append(t1 - t0)
        new_values.append(clip.sample(node, path,
                                      node_rest_trs(gltf, node)[
                                          {"translation": 0, "rotation": 1,
                                           "scale": 2}[path]], t1))
        trimmed[(node, path)] = (new_times, new_values, interp)
    out = ClipChannels.__new__(ClipChannels)
    out.name = clip.name
    out.channels = trimmed
    out.duration = t1 - t0
    return out


def drop_static_channels(gltf, clip, epsilon=1e-6):
    """删除恒定通道：与节点静息值一致直接删；恒定但不同则压成两帧。"""
    kept = {}
    for (node, path), (times, values, interp) in clip.channels.items():
        rest = node_rest_trs(gltf, node)[
            {"translation": 0, "rotation": 1, "scale": 2}[path]]
        flat = True
        matches_rest = True
        for value in values:
            if path == "rotation":
                if qangle(value, values[0]) > epsilon * 100.0:
                    flat = False
                if qangle(value, rest) > epsilon * 100.0:
                    matches_rest = False
            else:
                if max(abs(a - b) for a, b in zip(value, values[0])) > epsilon:
                    flat = False
                if max(abs(a - b) for a, b in zip(value, rest)) > epsilon:
                    matches_rest = False
            if not flat and not matches_rest:
                break
        if flat and matches_rest:
            continue  # 恒等于静息：无需通道
        if flat:
            kept[(node, path)] = ([0.0, clip.duration],
                                  [list(values[0]), list(values[0])], interp)
        else:
            kept[(node, path)] = (times, values, interp)
    clip.channels = kept
    return clip


def strip_root_motion(gltf, clip, root_node, hip_node, policy):
    """根运动剥离（KayKit 约定：根静止，垂直弹跳留在 Hip 平移）。

    strip_horizontal: 世界系水平漂移归零、保留垂直弹跳（走/跑/滑翔/俯冲）；
    pin_all: Hip 平移整体钉死在首帧（跳跃/攀爬，高度由游戏物理驱动）。
    """
    ht, _, _ = node_rest_trs(gltf, hip_node)
    hip_key = (hip_node, "translation")
    if hip_key not in clip.channels:
        return clip
    times, values, interp = clip.channels[hip_key]
    if policy == "pin_all":
        first = clip.sample(hip_node, "translation", ht, times[0])
        clip.channels[hip_key] = ([times[0], times[-1]],
                                  [first, list(first)], interp)
    elif policy == "strip_horizontal":
        _, _, ref_world = root_hip_fk(gltf, clip, times[0], root_node,
                                      hip_node)
        new_values = []
        for time, value in zip(times, values):
            root_rot, hip_local, hip_world = root_hip_fk(
                gltf, clip, time, root_node, hip_node)
            drift = [hip_world[0] - ref_world[0], 0.0,
                     hip_world[2] - ref_world[2]]
            corr_local = qrot(qconj(root_rot), drift)
            new_values.append(vsub(hip_local, corr_local))
        clip.channels[hip_key] = (times, new_values, interp)
    else:
        assert policy == "keep"
    # Root 平移统一钉死（源资产 Root 漂移 < 0.02，钉死杜绝残余漂移）。
    root_key = (root_node, "translation")
    if root_key in clip.channels:
        rt, _, _ = node_rest_trs(gltf, root_node)
        rtimes, _, rinterp = clip.channels[root_key]
        first = clip.sample(root_node, "translation", rt, rtimes[0])
        clip.channels[root_key] = ([rtimes[0], rtimes[-1]],
                                   [first, list(first)], rinterp)
    return clip


def joint_rotation_samples(gltf, clip, joints, frames):
    samples = []
    for i in range(frames):
        t = clip.duration * i / (frames - 1)
        pose = []
        for node in joints:
            _, rr, _ = node_rest_trs(gltf, node)
            pose.append(clip.sample(node, "rotation", rr, t))
        samples.append(pose)
    return samples


def pose_diff(a, b):
    return max(qangle(qa, qb) for qa, qb in zip(a, b))


def fit_loop_window(gltf, clip, joints, max_seam_deg=7.0):
    """端点不闭合时在 clip 内搜索最优循环窗口（≥60% 时长，接缝最小）。"""
    frames = 48
    samples = joint_rotation_samples(gltf, clip, joints, frames)
    seam = pose_diff(samples[0], samples[-1])
    if math.degrees(seam) <= max_seam_deg:
        return None, math.degrees(seam)
    best = None
    min_span = int(frames * 0.6)
    for i in range(frames - min_span):
        for j in range(i + min_span, frames):
            diff = pose_diff(samples[i], samples[j])
            if best is None or diff < best[0]:
                best = (diff, i, j)
    diff, i, j = best
    t0 = clip.duration * i / (frames - 1)
    t1 = clip.duration * j / (frames - 1)
    return (t0, t1), math.degrees(diff)


def enforce_loop(gltf, clip, joints, max_seam_deg=3.0):
    """循环 clip 端点闭合：接缝超阈值时把末段 20% 平滑收敛到首帧姿态，
    重采样为 30fps 均匀关键帧（引擎在线性插值下消费）。"""
    frames_probe = 24
    samples = joint_rotation_samples(gltf, clip, joints, frames_probe)
    seam = math.degrees(pose_diff(samples[0], samples[-1]))
    if seam <= max_seam_deg:
        return clip, 0.0
    rest_index = {"translation": 0, "rotation": 1, "scale": 2}
    first_pose = {}
    for (node, path) in clip.channels:
        rest = node_rest_trs(gltf, node)[rest_index[path]]
        first_pose[(node, path)] = clip.sample(node, path, rest, 0.0)
    frame_count = max(8, int(math.ceil(clip.duration * 30.0)) + 1)
    new_channels = {}
    blend_start = 0.8 * clip.duration
    for (node, path), (_, _, interp) in clip.channels.items():
        rest = node_rest_trs(gltf, node)[rest_index[path]]
        times = []
        values = []
        for i in range(frame_count):
            t = clip.duration * i / (frame_count - 1)
            value = clip.sample(node, path, rest, t)
            if t > blend_start:
                w = (t - blend_start) / max(clip.duration - blend_start, 1e-9)
                s = w * w * (3.0 - 2.0 * w)
                target = first_pose[(node, path)]
                if path == "rotation":
                    value = qslerp(value, target, s)
                else:
                    value = [a + (b - a) * s for a, b in zip(value, target)]
            times.append(t)
            values.append(value)
        new_channels[(node, path)] = (times, values, "LINEAR")
    clip.channels = new_channels
    return clip, seam



# ------------------------------------------------------- handslot.r 挂点


def rest_world_transform(gltf, node_idx):
    """静息姿态下节点的世界变换（位置 + 旋转），场景根为参考系。"""
    parent = {}
    for i, nd in enumerate(gltf["nodes"]):
        for c in nd.get("children", []):
            parent[c] = i
    pos = [0.0, 0.0, 0.0]
    rot = [0.0, 0.0, 0.0, 1.0]
    chain = []
    i = node_idx
    while True:
        chain.append(i)
        if i not in parent:
            break
        i = parent[i]
    for n in reversed(chain):
        t, r, s = node_rest_trs(gltf, n)
        pos = vadd(pos, qrot(rot, [t[k] * s[k] for k in range(3)]))
        rot = qmul(rot, r)
    return pos, qnorm(rot)


def compute_handslot(gltf, kaykit_gltf, r_hand_node):
    """按 KayKit handslot.r 的世界朝向/手部局部偏移计算新挂点局部 TRS。"""
    def find(g, name):
        for i, nd in enumerate(g["nodes"]):
            if nd.get("name") == name:
                return i
        raise AssertionError(f"{name} not found")

    k_hand_pos, k_hand_rot = rest_world_transform(
        kaykit_gltf, find(kaykit_gltf, "hand.r"))
    k_slot_pos, k_slot_rot = rest_world_transform(
        kaykit_gltf, find(kaykit_gltf, "handslot.r"))
    # 挂点相对手骨的局部偏移（解剖位置，跨骨架复刻）。
    offset_hand_frame = qrot(qconj(k_hand_rot), vsub(k_slot_pos, k_hand_pos))

    # 新模型引擎空间 = 绕 Y -90° × Armature 空间（朝向对齐在之后写入）。
    orient = [0.0, -math.sqrt(0.5), 0.0, math.sqrt(0.5)]
    orient_inv = qconj(orient)
    hand_pos_arm, hand_rot_arm = rest_world_transform(gltf, r_hand_node)
    hand_pos_eng = qrot(orient, hand_pos_arm)
    hand_rot_eng = qmul(orient, hand_rot_arm)
    slot_pos_eng = vadd(hand_pos_eng, qrot(hand_rot_eng, offset_hand_frame))
    slot_rot_eng = k_slot_rot  # 武器世界朝向与 KayKit 完全一致
    slot_pos_arm = qrot(orient_inv, slot_pos_eng)
    slot_rot_arm = qmul(orient_inv, slot_rot_eng)
    local_rot = qmul(qconj(hand_rot_arm), slot_rot_arm)
    local_pos = qrot(qconj(hand_rot_arm), vsub(slot_pos_arm, hand_pos_arm))
    return local_pos, qnorm(local_rot), offset_hand_frame, slot_rot_eng


# ------------------------------------------------------------- 主流程

# 源 clip 索引 → (目标名, 固定裁剪窗口, 根运动策略, 是否循环拟合)
CLIP_PLAN = [
    (0, "Dive", None, "strip_horizontal", False),
    (1, "glide", None, "strip_horizontal", True),
    (2, "Jump_Idle", None, "pin_all", True),
    (3, "walk", None, "strip_horizontal", True),
    (4, "cast", None, "keep", False),
    (5, "run", None, "strip_horizontal", True),
    (6, "Turn_180", (1.29, 2.91), "keep", False),
    (7, "climb", (1.0, 3.5), "pin_all", False),
]


def main():
    args = sys.argv[1:]
    kaykit_path = "entry/src/main/resources/rawfile/models/enemy.glb"
    if "--kaykit" in args:
        ki = args.index("--kaykit")
        kaykit_path = args[ki + 1]
        del args[ki:ki + 2]
    src_path, out_path = args

    gltf, bin_chunk = load_glb(src_path)
    kaykit_gltf, _ = load_glb(kaykit_path)
    nodes = gltf["nodes"]
    name_to_node = {nd.get("name", str(i)): i for i, nd in enumerate(nodes)}
    skin = gltf["skins"][0]
    joints = skin["joints"]
    joint_names = [nodes[j].get("name", str(j)) for j in joints]
    root_node = joints[joint_names.index("Root")]
    hip_node = joints[joint_names.index("Hip")]

    # ---- 逐 clip 处理：裁剪 → 根运动剥离 → 循环窗口拟合 ----
    out_clips = []
    report = []
    for src_idx, name, window, policy, fit in CLIP_PLAN:
        clip = ClipChannels(gltf, bin_chunk, gltf["animations"][src_idx])
        if window is not None:
            clip = trim_clip(gltf, clip, window[0], window[1])
        if fit:
            fit_window, seam = fit_loop_window(gltf, clip, joints)
            if fit_window is not None:
                clip = trim_clip(gltf, clip, fit_window[0], fit_window[1])
                report.append(f"{name}: loop fit to "
                              f"[{fit_window[0]:.2f},{fit_window[1]:.2f}] "
                              f"seam={seam:.1f}deg")
            clip, closed = enforce_loop(gltf, clip, joints)
            if closed > 0.0:
                report.append(f"{name}: loop seam {closed:.1f}deg closed "
                              f"by endpoint blend")
        clip = strip_root_motion(gltf, clip, root_node, hip_node, policy)
        clip = drop_static_channels(gltf, clip)
        clip.name = name
        out_clips.append(clip)

    # ---- 合成静态 idle（源资产无待机：取转身 clip 起始站姿）----
    turn_src = ClipChannels(gltf, bin_chunk, gltf["animations"][6])
    idle_channels = {}
    for node in joints:
        t, r, s = node_rest_trs(gltf, node)
        pose_t = turn_src.sample(node, "translation", t, 0.0)
        pose_r = qnorm(turn_src.sample(node, "rotation", r, 0.0))
        if max(abs(a - b) for a, b in zip(pose_t, t)) > 1e-6:
            idle_channels[(node, "translation")] = ([0.0, 1.0],
                                                    [pose_t, list(pose_t)],
                                                    "LINEAR")
        if qangle(pose_r, r) > 1e-4:
            idle_channels[(node, "rotation")] = ([0.0, 1.0],
                                                 [pose_r, list(pose_r)],
                                                 "LINEAR")
    idle = ClipChannels.__new__(ClipChannels)
    idle.name = "idle"
    idle.channels = idle_channels
    idle.duration = 1.0
    out_clips.insert(0, idle)

    # ---- 重建 GLB：新动画数据追加到 BIN 尾部，旧采样器数据留为孤儿 ----
    new_bin = bytearray(bin_chunk)
    next_view = len(gltf["bufferViews"])
    next_acc = len(gltf["accessors"])

    def append_accessor(values, type_name):
        nonlocal new_bin, next_view, next_acc
        n = COUNT[type_name]
        blob = b"".join(struct.pack("<" + "f" * n,
                                    *(v if n > 1 else [v]))
                        for v in values)
        while len(new_bin) % 4 != 0:
            new_bin += b"\x00"
        view_idx = next_view
        next_view += 1
        gltf["bufferViews"].append({"buffer": 0,
                                    "byteOffset": len(new_bin),
                                    "byteLength": len(blob)})
        new_bin += blob
        acc_idx = next_acc
        next_acc += 1
        gltf["accessors"].append({"bufferView": view_idx,
                                  "componentType": 5126,
                                  "count": len(values),
                                  "type": type_name})
        return acc_idx

    new_animations = []
    for clip in out_clips:
        channels = []
        for (node, path), (times, values, interp) in clip.channels.items():
            input_acc = append_accessor(times, "SCALAR")
            output_acc = append_accessor(
                values, "VEC3" if path != "rotation" else "VEC4")
            sampler = {"input": input_acc, "output": output_acc,
                       "interpolation": interp}
            channels.append({"sampler": 0,
                             "target": {"node": node, "path": path},
                             "_sampler": sampler})
        anim = {"name": clip.name, "samplers": [], "channels": []}
        for ch in channels:
            anim["samplers"].append(ch.pop("_sampler"))
            ch["sampler"] = len(anim["samplers"]) - 1
            anim["channels"].append(ch)
        new_animations.append(anim)
    gltf["animations"] = new_animations

    # ---- handslot.r 挂点关节 ----
    r_hand_node = name_to_node["R_Hand"]
    slot_pos, slot_rot, offset, slot_rot_eng = compute_handslot(
        gltf, kaykit_gltf, r_hand_node)
    slot_idx = len(gltf["nodes"])
    gltf["nodes"].append({"name": "handslot.r",
                          "translation": slot_pos,
                          "rotation": slot_rot})
    gltf["nodes"][r_hand_node].setdefault("children", []).append(slot_idx)
    skin["joints"].append(slot_idx)

    # ---- 整体朝向：Armature 绕 Y -90°，+X 前向 → 引擎 +Z 前向 ----
    scene_roots = gltf["scenes"][gltf.get("scene", 0)]["nodes"]
    assert len(scene_roots) == 1, "expected a single scene root"
    armature = scene_roots[0]
    assert "rotation" not in gltf["nodes"][armature]
    gltf["nodes"][armature]["rotation"] = [0.0, -math.sqrt(0.5), 0.0,
                                           math.sqrt(0.5)]

    # ---- inverseBindMatrices 补齐：关节数与 IBM 数必须一致 ----
    slot_pos_world, slot_rot_world = rest_world_transform(gltf, slot_idx)
    matrices = decode_accessor(gltf, bin_chunk, skin["inverseBindMatrices"])
    matrices.append(rigid_inverse_mat4(slot_pos_world, slot_rot_world))
    skin["inverseBindMatrices"] = append_accessor(matrices, "MAT4")

    gltf["buffers"][0]["byteLength"] = len(new_bin)
    write_glb(out_path, gltf, bytes(new_bin))

    for line in report:
        print(line)
    print(f"wrote {out_path}: clips={[c.name for c in out_clips]} "
          f"joints={len(skin['joints'])}")
    verify(out_path, kaykit_path, slot_rot_eng, offset)


# ------------------------------------------------------------- 回读校验


def verify(path, kaykit_path, expect_slot_rot_eng, expect_offset):
    gltf, bin_chunk = load_glb(path)
    kaykit_gltf, _ = load_glb(kaykit_path)
    nodes = gltf["nodes"]
    skin = gltf["skins"][0]
    joint_names = [nodes[j].get("name", str(j)) for j in skin["joints"]]
    failures = []

    def check(cond, message):
        if not cond:
            failures.append(message)

    check(len(skin["joints"]) == 42, "joint count must be 42")
    check("handslot.r" in joint_names, "handslot.r joint missing")
    expected = {"idle", "walk", "run", "Jump_Idle", "glide", "cast",
                "Dive", "Turn_180", "climb"}
    names = {a["name"] for a in gltf["animations"]}
    check(names == expected, f"clip names {names} != {expected}")

    name_to_node = {nd.get("name", str(i)): i for i, nd in enumerate(nodes)}
    root_node = name_to_node["Root"]
    hip_node = name_to_node["Hip"]
    ibm = gltf["accessors"][skin["inverseBindMatrices"]]
    check(ibm["count"] == len(skin["joints"]),
          "inverseBindMatrices count must equal joint count")
    # cast/Turn_180 为 keep 策略（原地重心位移是动作语言），不查漂移。
    drift_checked = {"idle", "walk", "run", "Jump_Idle", "glide", "Dive",
                     "climb"}
    for anim in gltf["animations"]:
        clip = ClipChannels(gltf, bin_chunk, anim)
        # 根运动：Hip 世界水平漂移 < 0.02（剥离/钉死 clip）。
        _, _, ref = root_hip_fk(gltf, clip, 0.0, root_node, hip_node)
        drift = 0.0
        for i in range(24):
            t = clip.duration * i / 23.0
            _, _, w = root_hip_fk(gltf, clip, t, root_node, hip_node)
            drift = max(drift, math.hypot(w[0] - ref[0], w[2] - ref[2]))
        if anim["name"] in drift_checked:
            check(drift < 0.02,
                  f"{anim['name']}: horizontal drift {drift:.3f}")
        # 循环 clip 端点接缝。
        if anim["name"] in {"idle", "walk", "run", "glide", "cast",
                            "Jump_Idle"}:
            seam = 0.0
            for node in skin["joints"]:
                _, rr, _ = node_rest_trs(gltf, node)
                a = clip.sample(node, "rotation", rr, 0.0)
                b = clip.sample(node, "rotation", rr, clip.duration)
                seam = max(seam, math.degrees(qangle(a, b)))
            check(seam < 7.0, f"{anim['name']}: loop seam {seam:.1f}deg")
        # 走/跑保留垂直弹跳（KayKit 语言）。
        if anim["name"] in {"walk", "run"}:
            ys = []
            for i in range(24):
                t = clip.duration * i / 23.0
                _, _, w = root_hip_fk(gltf, clip, t, root_node, hip_node)
                ys.append(w[1])
            check(max(ys) - min(ys) > 0.02,
                  f"{anim['name']}: vertical bounce lost")

    # 挂点世界朝向对齐 KayKit：转换后 Armature 已带 -90° 朝向旋转，
    # rest_world_transform 直接给出引擎空间朝向。
    slot_pos_arm, slot_rot_eng = rest_world_transform(
        gltf, name_to_node["handslot.r"])
    ang = math.degrees(qangle(slot_rot_eng, expect_slot_rot_eng))
    check(ang < 0.5, f"handslot.r orientation off by {ang:.2f}deg")
    hand_pos, hand_rot = rest_world_transform(gltf, name_to_node["R_Hand"])
    off = qrot(qconj(hand_rot), vsub(slot_pos_arm, hand_pos))
    off_err = max(abs(a - b) for a, b in zip(off, expect_offset))
    check(off_err < 1e-3, f"handslot.r offset off by {off_err:.4f}")

    if failures:
        for f in failures:
            print("VERIFY FAIL:", f)
        sys.exit(1)
    print("verify: all checks passed")


if __name__ == "__main__":
    main()

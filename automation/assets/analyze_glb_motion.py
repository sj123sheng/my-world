# analyze_glb_motion.py: 匿名动画 clip 动作识别（纯 Python，无第三方依赖）。
# 用法：python3 analyze_glb_motion.py <file.glb> [--fps 30]
#
# 对每条 animation 做 FK 采样，输出关键关节运动特征：
#   - 髋部高度范围 / 净位移 / 净偏航（转身）
#   - 双脚同时离地比例（跳跃/俯冲）
#   - 手部高于头部比例（攀爬/施法举手）
#   - 步频（脚步抬升峰数/秒，区分走/跑）
#   - 躯干前倾角（俯冲）
#   - 首尾帧姿态差（循环 vs 一次性）
# 用于把 NlaTrack* 之类的无名 clip 映射回动作语义。
import json
import math
import struct
import sys

COMPONENT_SIZE = {5120: 1, 5121: 1, 5122: 2, 5123: 2, 5125: 4, 5126: 4}
TYPE_COUNT = {"SCALAR": 1, "VEC2": 2, "VEC3": 3, "VEC4": 4, "MAT4": 16}


def load_glb(path):
    with open(path, "rb") as f:
        data = f.read()
    magic, version, _ = struct.unpack_from("<III", data, 0)
    assert magic == 0x46546C67, "not a GLB"
    offset = 12
    gltf, bin_chunk = None, b""
    while offset < len(data):
        length, ctype = struct.unpack_from("<II", data, offset)
        payload = data[offset + 8:offset + 8 + length]
        if ctype == 0x4E4F534A:
            gltf = json.loads(payload)
        elif ctype == 0x004E4942:
            bin_chunk = payload
        offset += 8 + length
        offset = (offset + 3) & ~3
    assert gltf is not None
    return gltf, bin_chunk


def accessor_values(gltf, bin_chunk, index):
    acc = gltf["accessors"][index]
    view = gltf["bufferViews"][acc["bufferView"]]
    comp = COMPONENT_SIZE[acc["componentType"]]
    n = TYPE_COUNT[acc["type"]]
    count = acc["count"]
    stride = view.get("byteStride", comp * n)
    base = view.get("byteOffset", 0) + acc.get("byteOffset", 0)
    fmt = {5126: "f", 5123: "H", 5125: "I", 5121: "B"}[acc["componentType"]]
    out = []
    for i in range(count):
        start = base + i * stride
        vals = struct.unpack_from("<" + fmt * n, bin_chunk, start)
        out.append(vals if n > 1 else vals[0])
    return out


# ---- 四元数/向量工具 ----
def qidentity():
    return (0.0, 0.0, 0.0, 1.0)  # (x, y, z, w)


def qslerp(a, b, t):
    dot = sum(x * y for x, y in zip(a, b))
    if dot < 0.0:
        b = tuple(-x for x in b)
        dot = -dot
    if dot > 0.9995:
        r = tuple(x + (y - x) * t for x, y in zip(a, b))
        n = math.sqrt(sum(x * x for x in r)) or 1.0
        return tuple(x / n for x in r)
    theta = math.acos(max(-1.0, min(1.0, dot)))
    s = math.sin(theta)
    wa = math.sin((1.0 - t) * theta) / s
    wb = math.sin(t * theta) / s
    return tuple(wa * x + wb * y for x, y in zip(a, b))


def qmul(a, b):
    ax, ay, az, aw = a
    bx, by, bz, bw = b
    return (
        aw * bx + ax * bw + ay * bz - az * by,
        aw * by - ax * bz + ay * bw + az * bx,
        aw * bz + ax * by - ay * bx + az * bw,
        aw * bw - ax * bx - ay * by - az * bz,
    )


def qrotate(q, v):
    x, y, z, w = q
    vx, vy, vz = v
    tx = 2.0 * (y * vz - z * vy)
    ty = 2.0 * (z * vx - x * vz)
    tz = 2.0 * (x * vy - y * vx)
    return (
        vx + w * tx + (y * tz - z * ty),
        vy + w * ty + (z * tx - x * tz),
        vz + w * tz + (x * ty - y * tx),
    )


def yaw_of(q):
    # glTF y-up：绕 Y 的偏航角
    x, y, z, w = q
    siny = 2.0 * (w * y - x * z)
    cosy = 1.0 - 2.0 * (x * x + z * z)
    return math.atan2(siny, cosy)


def pitch_of(q):
    # 绕 X 的前倾角（正值 = 前倾）
    x, y, z, w = q
    sinp = 2.0 * (w * x + y * z)
    return math.asin(max(-1.0, min(1.0, sinp)))


class ClipSampler:
    def __init__(self, gltf, bin_chunk, anim_index):
        self.anim = gltf["animations"][anim_index]
        self.channels = {}
        self.cache = {}
        for ch in self.anim["channels"]:
            node = ch["target"]["node"]
            path = ch["target"]["path"]
            sampler = self.anim["samplers"][ch["sampler"]]
            times = accessor_values(gltf, bin_chunk, sampler["input"])
            values = accessor_values(gltf, bin_chunk, sampler["output"])
            self.channels[(node, path)] = (
                times, values, sampler.get("interpolation", "LINEAR"))
        self.duration = max(ts[-1] for ts, _, _ in self.channels.values())

    def sample(self, node, path, t, default):
        key = (node, path)
        if key not in self.channels:
            return default
        times, values, interp = self.channels[key]
        if t <= times[0]:
            return values[0]
        if t >= times[-1]:
            return values[-1]
        lo, hi = 0, len(times) - 1
        while hi - lo > 1:
            mid = (lo + hi) // 2
            if times[mid] <= t:
                lo = mid
            else:
                hi = mid
        if interp == "STEP":
            return values[lo]
        span = times[hi] - times[lo]
        frac = (t - times[lo]) / span if span > 0 else 0.0
        a, b = values[lo], values[hi]
        if path == "rotation":
            return qslerp(a, b, frac)
        return tuple(x + (y - x) * frac for x, y in zip(a, b))


def analyze(path, fps):
    gltf, bin_chunk = load_glb(path)
    nodes = gltf["nodes"]
    name_to_node = {n.get("name", str(i)): i for i, n in enumerate(nodes)}
    children = [[] for _ in nodes]
    for i, n in enumerate(nodes):
        for c in n.get("children", []):
            children[i].append(c)
    roots = [i for i, n in enumerate(nodes)
             if not any(i in nodes[j].get("children", [])
                        for j in range(len(nodes)) if j != i)]
    # 只取骨架根（含最多关节子树的根），避免场景 mesh 节点干扰
    skin_joints = set(gltf["skins"][0]["joints"])

    def subtree_joint_count(i):
        stack, count = [i], 0
        while stack:
            cur = stack.pop()
            if cur in skin_joints:
                count += 1
            stack.extend(children[cur])
        return count

    root = max(roots, key=subtree_joint_count)
    watch = ["Hip", "Root", "L_Foot", "R_Foot", "L_Hand", "R_Hand", "Head",
             "Spine02"]
    watch_idx = {name: name_to_node[name] for name in watch
                 if name in name_to_node}

    for ai, anim in enumerate(gltf["animations"]):
        sampler = ClipSampler(gltf, bin_chunk, ai)
        frames = max(2, int(sampler.duration * fps) + 1)
        tracks = {name: [] for name in watch_idx}
        yaws = []
        for fi in range(frames):
            t = sampler.duration * fi / (frames - 1)
            world_pos = {}
            world_rot = {}

            def fk(node, parent_pos, parent_rot):
                node_def = nodes[node]
                if "matrix" in node_def:
                    raise AssertionError("matrix nodes unsupported")
                lt = list(sampler.sample(node, "translation", t,
                                         node_def.get("translation",
                                                      [0, 0, 0])))
                lr = list(sampler.sample(node, "rotation", t,
                                         node_def.get("rotation",
                                                      [0, 0, 0, 1])))
                ls = list(sampler.sample(node, "scale", t,
                                         node_def.get("scale", [1, 1, 1])))
                rot = qmul(parent_rot, lr)
                pos = tuple(parent_pos[k] + qrotate(parent_rot, tuple(
                    lt[k] * ls[k] for k in range(3)))[k] for k in range(3))
                world_pos[node] = pos
                world_rot[node] = rot
                for c in children[node]:
                    fk(c, pos, rot)

            fk(root, (0.0, 0.0, 0.0), qidentity())
            for name, idx in watch_idx.items():
                tracks[name].append(world_pos[idx])
            yaws.append(yaw_of(world_rot[watch_idx["Hip"]]))

        hip = tracks["Hip"]
        hip_y = [p[1] for p in hip]
        foot_y = [min(tracks["L_Foot"][i][1], tracks["R_Foot"][i][1])
                  for i in range(frames)]
        both_feet = [tracks["L_Foot"][i][1] for i in range(frames)]
        ground = min(min(tracks["L_Foot"], key=lambda p: p[1])[1],
                     min(tracks["R_Foot"], key=lambda p: p[1])[1])
        airborne = sum(1 for i in range(frames)
                       if tracks["L_Foot"][i][1] > ground + 0.12 and
                       tracks["R_Foot"][i][1] > ground + 0.12) / frames
        head_y = [p[1] for p in tracks["Head"]]
        hands_up = sum(1 for i in range(frames)
                       if max(tracks["L_Hand"][i][1],
                              tracks["R_Hand"][i][1]) > head_y[i]) / frames
        hand_reach = sum(
            math.dist(tracks["R_Hand"][i], tracks["L_Hand"][i])
            for i in range(frames)) / frames
        # 步频：左脚高度信号的峰值数
        lf = [p[1] for p in tracks["L_Foot"]]
        peaks = 0
        for i in range(1, frames - 1):
            if lf[i] > lf[i - 1] and lf[i] >= lf[i + 1] and \
                    lf[i] > ground + 0.08:
                peaks += 1
        cadence = peaks / sampler.duration
        # 净偏航（展开连续角）
        unwrapped = [yaws[0]]
        for i in range(1, frames):
            d = yaws[i] - yaws[i - 1]
            while d > math.pi:
                d -= 2 * math.pi
            while d < -math.pi:
                d += 2 * math.pi
            unwrapped.append(unwrapped[-1] + d)
        net_yaw = unwrapped[-1] - unwrapped[0]
        # 水平净位移（root motion）
        start = hip[0]
        end = hip[-1]
        disp = math.dist((start[0], start[2]), (end[0], end[2]))
        # 躯干前倾
        spine_vec = [tuple(tracks["Spine02"][i][k] - tracks["Hip"][i][k]
                           for k in range(3)) for i in range(frames)]
        leans = []
        for v in spine_vec:
            horiz = math.hypot(v[0], v[2])
            leans.append(math.atan2(horiz, max(v[1], 1e-6)))
        lean = sum(leans) / frames
        # 循环性：首尾髋位置差 + 偏航差
        loop_pos = math.dist(hip[0], hip[-1])
        loop_yaw = abs(net_yaw) if abs(net_yaw) > 0.1 else 0.0
        print(f"[{ai}] {anim.get('name', '?')}: dur={sampler.duration:.3f}s")
        print(f"    hip_y=[{min(hip_y):.3f},{max(hip_y):.3f}] "
              f"airborne={airborne:.2f} hands_up={hands_up:.2f} "
              f"hand_span={hand_reach:.3f}")
        print(f"    cadence={cadence:.2f} steps/s net_yaw={math.degrees(net_yaw):.1f}deg "
              f"disp={disp:.3f} lean={math.degrees(lean):.1f}deg")
        print(f"    loop_gap: pos={loop_pos:.3f} yaw={math.degrees(loop_yaw):.1f}deg")


if __name__ == "__main__":
    args = [a for a in sys.argv[1:] if not a.startswith("--")]
    fps_arg = [a for a in sys.argv[1:] if a.startswith("--fps")]
    analyze(args[0], int(fps_arg[0].split("=")[1]) if fps_arg else 30)

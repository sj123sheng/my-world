# clip_detail.py: 逐 clip 输出通道分布与关键关节逐帧序列，用于
# 识别 NlaTrack* 动画语义与根运动归属（只读，纯标准库）。
import json
import math
import struct
import sys


def load_glb(path):
    with open(path, "rb") as f:
        data = f.read()
    json_len = struct.unpack("<I", data[12:16])[0]
    gltf = json.loads(data[20:20 + json_len].decode("utf-8"))
    bin_off = 20 + json_len
    bin_len = struct.unpack("<I", data[bin_off:bin_off + 4])[0]
    return gltf, data[bin_off + 8:bin_off + 8 + bin_len]


COMP = {5120: ("b", 1), 5121: ("B", 1), 5122: ("h", 2), 5123: ("H", 2),
        5125: ("I", 4), 5126: ("f", 4)}
COUNT = {"SCALAR": 1, "VEC2": 2, "VEC3": 3, "VEC4": 4, "MAT4": 16}


def accessor(gltf, bin_chunk, idx):
    acc = gltf["accessors"][idx]
    view = gltf["bufferViews"][acc["bufferView"]]
    comp, csize = COMP[acc["componentType"]]
    n = COUNT[acc["type"]]
    stride = view.get("byteStride") or csize * n
    base = view.get("byteOffset", 0) + acc.get("byteOffset", 0)
    out = []
    for i in range(acc["count"]):
        off = base + i * stride
        vals = struct.unpack("<" + comp * n, bin_chunk[off:off + csize * n])
        out.append(vals if n > 1 else vals[0])
    return out


def main():
    gltf, bin_chunk = load_glb(sys.argv[1])
    nodes = gltf["nodes"]
    node_name = [n.get("name", str(i)) for i, n in enumerate(nodes)]
    mode = sys.argv[2] if len(sys.argv) > 2 else "channels"

    if mode == "channels":
        for ai, anim in enumerate(gltf["animations"]):
            trans_nodes = []
            for ch in anim["channels"]:
                if ch["target"]["path"] == "translation":
                    node = ch["target"]["node"]
                    sampler = anim["samplers"][ch["sampler"]]
                    vals = accessor(gltf, bin_chunk, sampler["output"])
                    xs = [v[0] for v in vals]
                    ys = [v[1] for v in vals]
                    zs = [v[2] for v in vals]
                    rng = (max(xs) - min(xs), max(ys) - min(ys),
                           max(zs) - min(zs))
                    trans_nodes.append((node_name[node], rng))
            big = [(n, r) for n, r in trans_nodes
                   if max(r) > 0.02]
            print(f"[{ai}] {anim.get('name', '?')}: translation channels "
                  f"with range>0.02: {big}")
        return

    if mode == "rest":
        # 静息姿态下指定关节的世界变换（位置 + 轴）。
        def mat_from_node(i):
            n = nodes[i]
            if "matrix" in n:
                return [list(n["matrix"][r::4]) for r in range(4)]
            t = n.get("translation", [0, 0, 0])
            r = n.get("rotation", [0, 0, 0, 1])
            s = n.get("scale", [1, 1, 1])
            x, y, z, w = r
            R = [
                [1 - 2 * (y * y + z * z), 2 * (x * y - z * w),
                 2 * (x * z + y * w)],
                [2 * (x * y + z * w), 1 - 2 * (x * x + z * z),
                 2 * (y * z - x * w)],
                [2 * (x * z - y * w), 2 * (y * z + x * w),
                 1 - 2 * (x * x + y * y)],
            ]
            RS = [[R[a][b] * s[b] for b in range(3)] for a in range(3)]
            return [RS[0] + [t[0]], RS[1] + [t[1]], RS[2] + [t[2]],
                    [0, 0, 0, 1]]

        def mmul(a, b):
            out = [[0.0] * 4 for _ in range(4)]
            for r in range(4):
                for c in range(4):
                    out[r][c] = sum(a[r][k] * b[k][c] for k in range(4))
            return out

        parent = {}
        for i, n in enumerate(nodes):
            for c in n.get("children", []):
                parent[c] = i

        def world(i):
            m = mat_from_node(i)
            while i in parent:
                i = parent[i]
                m = mmul(mat_from_node(i), m)
            return m

        for name in sys.argv[3:]:
            idx = node_name.index(name)
            m = world(idx)
            print(f"{name}: pos=({m[0][3]:.4f},{m[1][3]:.4f},{m[2][3]:.4f})")
            for a, axis in enumerate(("X", "Y", "Z")):
                col = (m[0][a], m[1][a], m[2][a])
                print(f"  local {axis} axis in world: "
                      f"({col[0]:.3f},{col[1]:.3f},{col[2]:.3f})")
        return

    if mode == "frames":
        # 逐帧序列：clip 索引列表，输出 Hip 高度/双手高度/前向位移。
        want = [int(x) for x in sys.argv[3].split(",")]
        skin_joints = gltf["skins"][0]["joints"]
        name_to_node = {node_name[i]: i for i in range(len(nodes))}
        watch = ["Hip", "L_Hand", "R_Hand", "L_Foot", "R_Foot", "Head"]
        watch_idx = [name_to_node[n] for n in watch]
        children = [[] for _ in nodes]
        for i, n in enumerate(nodes):
            for c in n.get("children", []):
                children[i].append(c)
        roots = [i for i in range(len(nodes)) if i not in parent_of(nodes)]
        root = max(roots, key=lambda r: subtree(nodes, children, r,
                                                 set(skin_joints)))
        for ai in want:
            anim = gltf["animations"][ai]
            print(f"== [{ai}] {anim.get('name', '?')} ==")
            dump_frames(gltf, bin_chunk, nodes, children, root, anim,
                        watch, watch_idx)


def parent_of(nodes):
    p = {}
    for i, n in enumerate(nodes):
        for c in n.get("children", []):
            p[c] = i
    return p


def subtree(nodes, children, root, joints):
    stack, count = [root], 0
    while stack:
        cur = stack.pop()
        if cur in joints:
            count += 1
        stack.extend(children[cur])
    return count


def dump_frames(gltf, bin_chunk, nodes, children, root, anim, watch,
               watch_idx):
    # 采样所有 channel 到统一时间轴。
    chans = {}
    duration = 0.0
    for ch in anim["channels"]:
        sampler = anim["samplers"][ch["sampler"]]
        times = accessor(gltf, bin_chunk, sampler["input"])
        vals = accessor(gltf, bin_chunk, sampler["output"])
        key = (ch["target"]["node"], ch["target"]["path"])
        chans[key] = (times, vals, sampler.get("interpolation", "LINEAR"))
        duration = max(duration, times[-1])
    frames = 13
    for fi in range(frames):
        t = duration * fi / (frames - 1)
        pos = {}
        rot = {}

        def sample(node, path, default):
            key = (node, path)
            if key not in chans:
                return default
            times, vals, interp = chans[key]
            if t <= times[0]:
                return vals[0]
            if t >= times[-1]:
                return vals[-1]
            hi = 0
            while times[hi] < t:
                hi += 1
            lo = hi - 1
            frac = (t - times[lo]) / max(times[hi] - times[lo], 1e-9)
            if interp == "STEP":
                return vals[lo]
            a, b = vals[lo], vals[hi]
            if path == "rotation":
                return slerp(a, b, frac)
            return tuple(x + (y - x) * frac for x, y in zip(a, b))

        world_pos = {}

        def fk(node, ppos, prot):
            nd = nodes[node]
            lt = sample(node, "translation", nd.get("translation",
                                                    [0, 0, 0]))
            lr = sample(node, "rotation", nd.get("rotation",
                                                 [0, 0, 0, 1]))
            ls = sample(node, "scale", nd.get("scale", [1, 1, 1]))
            r = qmul(prot, lr)
            local = tuple(lt[k] * ls[k] for k in range(3))
            p = tuple(ppos[k] + qrot(prot, local)[k] for k in range(3))
            world_pos[node] = p
            for c in children[node]:
                fk(c, p, r)

        fk(root, (0.0, 0.0, 0.0), (0, 0, 0, 1))
        row = [f"t={t:5.2f}"]
        for name, idx in zip(watch, watch_idx):
            p = world_pos[idx]
            row.append(f"{name}=({p[0]:6.2f},{p[1]:5.2f},{p[2]:6.2f})")
        print(" ".join(row))


def qmul(a, b):
    ax, ay, az, aw = a
    bx, by, bz, bw = b
    return (aw * bx + ax * bw + ay * bz - az * by,
            aw * by - ax * bz + ay * bw + az * bx,
            aw * bz + ax * by - ay * bx + az * bw,
            aw * bw - ax * bx - ay * by - az * bz)


def qrot(q, v):
    x, y, z, w = q
    vx, vy, vz = v
    tx, ty, tz = 2 * (y * vz - z * vy), 2 * (z * vx - x * vz), \
        2 * (x * vy - y * vx)
    return (vx + w * tx + y * tz - z * ty,
            vy + w * ty + z * tx - x * tz,
            vz + w * tz + x * ty - y * tx)


def slerp(a, b, t):
    dot = sum(x * y for x, y in zip(a, b))
    if dot < 0:
        b = tuple(-x for x in b)
        dot = -dot
    if dot > 0.9995:
        out = tuple(x + (y - x) * t for x, y in zip(a, b))
        n = math.sqrt(sum(x * x for x in out))
        return tuple(x / n for x in out)
    theta = math.acos(max(-1.0, min(1.0, dot)))
    sa, sb = math.sin((1 - t) * theta), math.sin(t * theta)
    s = math.sin(theta)
    return tuple((sa * x + sb * y) / s for x, y in zip(a, b))


if __name__ == "__main__":
    main()

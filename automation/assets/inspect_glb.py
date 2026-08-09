# inspect_glb.py: GLB 契约快检（独立高模资产入库前自检）。
# 用法：python3 inspect_glb.py <file.glb>
# 只读 JSON chunk + accessor 统计，不依赖外部库。
import json
import struct
import sys


def main(path):
    with open(path, "rb") as f:
        data = f.read()
    magic, version, length = struct.unpack("<III", data[:12])
    assert magic == 0x46546C67, "not a GLB"
    pos = 12
    gltf = None
    bin_data = b""
    while pos < length:
        chunk_len, chunk_type = struct.unpack("<II", data[pos:pos + 8])
        if chunk_type == 0x4E4F534A:
            gltf = json.loads(data[pos + 8:pos + 8 + chunk_len])
        elif chunk_type == 0x004E4942:
            bin_data = data[pos + 8:pos + 8 + chunk_len]
        pos += 8 + chunk_len
    assert gltf is not None, "no JSON chunk"
    global _BIN
    _BIN = bin_data

    meshes = gltf.get("meshes", [])
    tris = 0
    attrs = set()
    for m in meshes:
        for p in m["primitives"]:
            if p.get("mode", 4) != 4:
                print("WARN: non-TRIANGLES primitive mode", p.get("mode"))
            attrs.update(p["attributes"].keys())
            acc = gltf["accessors"][p["indices"]]
            tris += acc["count"] // 3
    skins = gltf.get("skins", [])
    joint_counts = [len(s["joints"]) for s in skins]
    joint_names = []
    if skins:
        joint_names = _names(gltf, skins[0])
    anims = [a.get("name", i) for i, a in enumerate(gltf.get("animations", []))]
    interps = set()
    for a in gltf.get("animations", []):
        for s in a.get("samplers", []):
            interps.add(s.get("interpolation", "LINEAR"))
    max_infl = _max_influences(gltf)
    print("meshes:", len(meshes), "total tris:", tris)
    print("primitive attrs:", sorted(attrs))
    print("skins:", len(skins), "joint counts:", joint_counts)
    print("has handslot.r:", "handslot.r" in joint_names)
    print("animations:", len(anims), anims[:12])
    print("interpolations:", interps)
    print("materials:", len(gltf.get("materials", [])), "images:", len(gltf.get("images", [])))
    print("JOINTS_1 present:", "JOINTS_1" in attrs)
    print("max influences per vertex:", max_infl)


def _names(gltf, skin):
    # 关节名存在 skin 的 extras 或 nodes 里；glTF 标准用 node name。
    return [gltf["nodes"][j].get("name", "") for j in skin["joints"]]


def _accessor_strings(gltf, idx):
    return "[]"


def _max_influences(gltf):
    # 统计每顶点非零权重数上限。
    worst = 0
    for m in gltf.get("meshes", []):
        for p in m["primitives"]:
            w_idx = p["attributes"].get("WEIGHTS_0")
            if w_idx is None:
                continue
            acc = gltf["accessors"][w_idx]
            values = _float_accessor(gltf, acc)
            comps = acc["type"] == "VEC4" and 4 or 3
            for i in range(0, len(values), comps):
                n = sum(1 for v in values[i:i + comps] if v > 1e-6)
                worst = max(worst, n)
    return worst


def _float_accessor(gltf, acc):
    import array
    bv = gltf["bufferViews"][acc["bufferView"]]
    count = acc["count"] * {"SCALAR": 1, "VEC2": 2, "VEC3": 3, "VEC4": 4}[acc["type"]]
    a = array.array("f")
    off = bv.get("byteOffset", 0) + acc.get("byteOffset", 0)
    a.frombytes(_BIN[off:off + count * 4])
    return a.tolist()


_BIN = b""


if __name__ == "__main__":
    main(sys.argv[1])

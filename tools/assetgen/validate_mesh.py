"""FOOTCANDLE mesh validator - fails, never warns (docs/ROADMAP.md 10.3).

Usage: python validate_mesh.py <spec.json> <mesh.glb>

Pure-python GLB parser (no dependencies): checks the exported mesh against its
spec before it is allowed anywhere near the engine.
Checks: naming, triangle budget, dimensions vs spec (grid snap +-0.5 cm),
pivot placement, UVs inside [0,1], no embedded materials.
"""

import json
import struct
import sys


def read_glb(path):
    with open(path, "rb") as handle:
        data = handle.read()
    magic, _version, _length = struct.unpack_from("<III", data, 0)
    if magic != 0x46546C67:  # 'glTF'
        raise SystemExit("not a GLB file")
    offset = 12
    gltf = None
    binary = b""
    while offset < len(data):
        chunk_len, chunk_type = struct.unpack_from("<II", data, offset)
        chunk = data[offset + 8: offset + 8 + chunk_len]
        if chunk_type == 0x4E4F534A:  # JSON
            gltf = json.loads(chunk.decode("utf-8"))
        elif chunk_type == 0x004E4942:  # BIN
            binary = chunk
        offset += 8 + chunk_len
    return gltf, binary


def accessor_data(gltf, binary, index, fmt, components):
    accessor = gltf["accessors"][index]
    view = gltf["bufferViews"][accessor["bufferView"]]
    start = view.get("byteOffset", 0) + accessor.get("byteOffset", 0)
    count = accessor["count"]
    size = struct.calcsize(fmt) * components
    out = []
    for i in range(count):
        out.append(struct.unpack_from("<" + fmt * components, binary, start + i * size))
    return out


FMT = {5126: "f", 5123: "H", 5125: "I"}


def fail(msg):
    print(f"[validate] FAIL: {msg}")
    sys.exit(1)


def main():
    spec_path, glb_path = sys.argv[1], sys.argv[2]
    with open(spec_path, "r", encoding="utf-8") as handle:
        spec = json.load(handle)
    gltf, binary = read_glb(glb_path)

    meshes = gltf.get("meshes", [])
    if len(meshes) != 1:
        fail(f"expected exactly 1 mesh, got {len(meshes)}")
    mesh = meshes[0]
    if spec["name"] not in (mesh.get("name", ""),
                            gltf.get("nodes", [{}])[0].get("name", "")):
        fail(f"mesh name mismatch: spec={spec['name']} glb={mesh.get('name')}")
    if gltf.get("materials"):
        fail("embedded materials present - materials are engine-side instances")

    tri_count = 0
    min_pos = [float("inf")] * 3
    max_pos = [float("-inf")] * 3
    for prim in mesh["primitives"]:
        indices_accessor = gltf["accessors"][prim["indices"]]
        tri_count += indices_accessor["count"] // 3
        pos_accessor = gltf["accessors"][prim["attributes"]["POSITION"]]
        for axis in range(3):
            min_pos[axis] = min(min_pos[axis], pos_accessor["min"][axis])
            max_pos[axis] = max(max_pos[axis], pos_accessor["max"][axis])
        uv_index = prim["attributes"].get("TEXCOORD_0")
        if uv_index is None:
            fail("no UVs")
        uv_accessor = gltf["accessors"][uv_index]
        fmt = FMT[uv_accessor["componentType"]]
        for uv in accessor_data(gltf, binary, uv_index, fmt, 2):
            if not (0.0 <= uv[0] <= 1.0 and 0.0 <= uv[1] <= 1.0):
                fail(f"UV outside [0,1]: {uv}")

    if tri_count > spec["tri_budget"]:
        fail(f"triangle budget: {tri_count} > {spec['tri_budget']}")

    # glTF is Y-up meters; spec is Z-up cm. Exported with +Y up: Blender Z -> glTF Y.
    size_m = [max_pos[i] - min_pos[i] for i in range(3)]
    got_cm = [size_m[0] * 100.0, size_m[2] * 100.0, size_m[1] * 100.0]  # X, Z<-glTF z? map: glb(x,y,z)=(bl.x, bl.z, -bl.y)
    want_cm = spec["size_cm"]
    for axis in range(3):
        if abs(got_cm[axis] - want_cm[axis]) > 0.5:
            fail(f"dimension axis {axis}: got {got_cm[axis]:.2f} cm, want {want_cm[axis]} cm")

    if spec.get("pivot", "corner") == "corner":
        # Min corner at origin within tolerance (glTF axes).
        for axis in range(3):
            lo = min_pos[axis]
            if not (-0.006 <= lo <= 0.006) and not (axis == 2 and abs(max_pos[2]) <= 0.006):
                fail(f"corner pivot: min[{axis}] = {lo:.4f} m not at origin")

    print(f"[validate] PASS: {spec['name']} tris={tri_count} size_cm={[round(c,1) for c in got_cm]}")


if __name__ == "__main__":
    main()

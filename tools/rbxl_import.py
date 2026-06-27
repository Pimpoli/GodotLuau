#!/usr/bin/env python3
# ============================================================================
#  rbxl_import.py  -  GodotLuau
#  Reads a Roblox place file (.rbxl binary) and reconstructs its full instance
#  tree so it can be ported to Godot/GodotLuau "as-is".
#
#  Stage 1 (this file): lossless parse of the binary format.
#    - zstd chunks decompressed in a SINGLE call to the `zstd` CLI (no pip deps)
#    - full instance tree (referents + PRNT) with names
#    - Luau source recovered from every Script/LocalScript/ModuleScript
#  Outputs:
#    <out>/scripts/<tree path>.lua    one file per script, mirroring the tree
#    <out>/tree.json                  manifest of the whole tree (for stage 2)
#    prints an import report: class coverage (mapped / placeholder).
#
#  Usage (run from the repo root, with the MSYS2 python that has the `zstd` CLI
#  on PATH):
#    python tools/rbxl_import.py "C:/path/Game.rbxl" --out "C:/path/out"
#
#  Stage 2 (next): emit a Godot .tscn that instantiates GodotLuau nodes from
#  this tree, translating properties and upgrading placeholders to real nodes.
# ============================================================================
import sys, struct, subprocess, shutil, os, json, argparse

ZSTD = shutil.which("zstd") or r"C:\msys64\ucrt64\bin\zstd.exe"

# Roblox class -> GodotLuau registered class. Classes not present here are
# imported as placeholders (a node that keeps Name + hierarchy + props), so the
# tree is never lost. This table is the porting roadmap; grow it over time.
CLASS_MAP = {
    # Geometry
    "Part": "RobloxPart", "MeshPart": "RobloxPart", "WedgePart": "RobloxPart",
    "CornerWedgePart": "RobloxPart", "TrussPart": "RobloxPart",
    "UnionOperation": "RobloxPart", "SpawnLocation": "SpawnLocation",
    # Containers
    "Folder": "Folder", "Model": "Node3D", "Configuration": "Folder",
    # Characters / physics
    "Humanoid": "Humanoid", "Motor6D": "Motor6D", "Tool": "RobloxTool",
    "Backpack": "Backpack",
    "WeldConstraint": "WeldConstraint", "HingeConstraint": "HingeConstraint",
    "BallSocketConstraint": "BallSocketConstraint", "RodConstraint": "RodConstraint",
    "SpringConstraint": "SpringConstraint",
    "BodyVelocity": "BodyVelocity", "BodyPosition": "BodyPosition",
    "BodyForce": "BodyForce", "BodyAngularVelocity": "BodyAngularVelocity",
    "BodyGyro": "BodyGyro",
    # Lights
    "PointLight": "OmniLight3D", "SpotLight": "SpotLight3D",
    "SurfaceLight": "OmniLight3D",
    # Scripts
    "Script": "ServerScript", "LocalScript": "LocalScript",
    "ModuleScript": "ModuleScript",
    # Comms
    "RemoteEvent": "RemoteEventNode", "RemoteFunction": "RemoteFunctionNode",
    "BindableEvent": "BindableEventNode", "BindableFunction": "BindableEventNode",
    # UI
    "ScreenGui": "ScreenGui", "Frame": "RobloxFrame", "TextLabel": "RobloxTextLabel",
    "TextButton": "RobloxTextButton", "TextBox": "RobloxTextBox",
    "ImageLabel": "RobloxImageLabel", "ImageButton": "RobloxImageLabel",
    "ScrollingFrame": "RobloxScrollingFrame", "BillboardGui": "BillboardGui",
    "SurfaceGui": "SurfaceGui",
    # Interactive / sound / tween / anim
    "ClickDetector": "ClickDetector", "ProximityPrompt": "ProximityPrompt",
    "Sound": "RobloxSound", "SoundGroup": "RobloxSoundGroup",
    "Animation": "AnimationObject",
}

SCRIPT_CLASSES = ("Script", "LocalScript", "ModuleScript")

# ---------------------------------------------------------------- binary read
def rstr(buf, off):
    (ln,) = struct.unpack_from("<I", buf, off); off += 4
    return buf[off:off+ln], off + ln

def read_interleaved_u32(buf, off, n):
    vals = [0]*n
    for i in range(n):
        vals[i] = ((buf[off+i] << 24) | (buf[off+n+i] << 16)
                   | (buf[off+2*n+i] << 8) | buf[off+3*n+i])
    return vals, off + 4*n

def untransform_i32(u):
    return (u >> 1) ^ (-(u & 1))

def read_referents(buf, off, n):
    raw, off = read_interleaved_u32(buf, off, n)
    out = []; acc = 0
    for u in raw:
        acc += untransform_i32(u); out.append(acc)
    return out, off

def decode_str(b):
    try: return b.decode("utf-8")
    except UnicodeDecodeError: return b.decode("latin-1")

class Inst:
    __slots__ = ("ref","cls","name","parent","children","is_service","source")
    def __init__(s, ref, cls):
        s.ref=ref; s.cls=cls; s.name=cls; s.parent=-1
        s.children=[]; s.is_service=False; s.source=None

def load_chunks(path):
    """Headers are uncompressed; collect all zstd frames and decompress them in
    ONE `zstd` call (3000+ chunks -> 1 subprocess). Returns (header, chunks)."""
    data = open(path, "rb").read()
    if data[:8] != b"<roblox!":
        raise SystemExit("Not a binary .rbxl file (expected '<roblox!' magic). "
                         "If this is a .rbxlx XML file, a different reader is needed.")
    hdr = struct.unpack_from("<ii", data, 16)  # (class_count, inst_count)
    off = 32
    raw = []
    zblob = bytearray(); zlens = []; zidx = []
    while off < len(data):
        name = data[off:off+4]
        comp, uncomp, _ = struct.unpack_from("<III", data, off+4)
        stored = comp if comp != 0 else uncomp
        payload = data[off+16: off+16+stored]
        idx = len(raw); raw.append([name, None])
        if comp != 0 and payload[:4] == b"\x28\xb5\x2f\xfd":
            zblob += payload; zlens.append(uncomp); zidx.append(idx)
        elif comp != 0:
            raise SystemExit("Chunk uses LZ4 compression; this build only wires "
                             "zstd. (Re-save from a recent Studio, or extend the reader.)")
        else:
            raw[idx][1] = payload
        off += 16 + stored
        if name[:3] == b"END": break
    if zblob:
        p = subprocess.run([ZSTD,"-d","-c"], input=bytes(zblob),
                           stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        if p.returncode != 0:
            raise SystemExit("zstd decompress failed: "+p.stderr.decode("utf8","replace"))
        out = p.stdout; pos = 0
        for k, idx in enumerate(zidx):
            ln = zlens[k]; raw[idx][1] = out[pos:pos+ln]; pos += ln
    return hdr, [(r[0], r[1]) for r in raw]

def parse(path):
    hdr, chunks = load_chunks(path)
    insts = {}; class_refs = {}; class_names = {}; prnt = None; shared = []
    for name, body in chunks:
        if name == b"SSTR":
            cnt, = struct.unpack_from("<I", body, 4); o = 8
            for _ in range(cnt):
                o += 16
                s, o = rstr(body, o); shared.append(s)
    for name, body in chunks:
        if name == b"INST":
            cid, = struct.unpack_from("<i", body, 0)
            cname, o = rstr(body, 4); fmt = body[o]; o += 1
            n, = struct.unpack_from("<i", body, o); o += 4
            refs, o = read_referents(body, o, n)
            cname = cname.decode("latin1")
            class_names[cid] = cname; class_refs[cid] = refs
            for r in refs:
                ins = Inst(r, cname); ins.is_service = (fmt == 1); insts[r] = ins
    for name, body in chunks:
        if name != b"PROP": continue
        cid, = struct.unpack_from("<i", body, 0)
        pname, o = rstr(body, 4); tid = body[o]; o += 1
        refs = class_refs.get(cid, [])
        if pname == b"Name" and tid == 0x01:
            for r in refs:
                s, o = rstr(body, o)
                if r in insts: insts[r].name = decode_str(s)
        elif pname == b"Source":
            if tid == 0x01:
                for r in refs:
                    s, o = rstr(body, o)
                    if r in insts: insts[r].source = decode_str(s)
            elif tid == 0x1C:
                idxs, o = read_interleaved_u32(body, o, len(refs))
                for r, si in zip(refs, idxs):
                    if r in insts and si < len(shared):
                        insts[r].source = decode_str(shared[si])
    for name, body in chunks:
        if name == b"PRNT":
            m, = struct.unpack_from("<i", body, 1); o = 5
            childs, o = read_referents(body, o, m)
            parents, o = read_referents(body, o, m)
            prnt = (childs, parents)
    roots = []
    if prnt:
        for c, p in zip(*prnt):
            if c not in insts: continue
            insts[c].parent = p
            (roots if (p == -1 or p not in insts) else insts[p].children).append(c)
    return insts, roots

# ---------------------------------------------------------------- output
def safe(name):
    out = "".join(c if c.isalnum() or c in " _-" else "_" for c in name).strip()
    return out or "Unnamed"

def export_scripts(insts, roots, out_dir):
    sdir = os.path.join(out_dir, "scripts")
    count = 0
    def walk(ref, path_parts):
        nonlocal count
        ins = insts[ref]
        here = path_parts + [safe(ins.name)]
        if ins.cls in SCRIPT_CLASSES and ins.source is not None:
            rel = os.path.join(*here) + ".lua"
            full = os.path.join(sdir, rel)
            os.makedirs(os.path.dirname(full), exist_ok=True)
            with open(full, "w", encoding="utf-8") as f:
                f.write(ins.source)
            count += 1
        for c in ins.children:
            walk(c, here)
    for r in roots:
        walk(r, [])
    return count

def write_manifest(insts, roots, out_dir):
    def node(ref):
        ins = insts[ref]
        d = {"name": ins.name, "class": ins.cls}
        if ins.is_service: d["service"] = True
        if ins.cls in SCRIPT_CLASSES: d["script"] = True
        if ins.children:
            d["children"] = [node(c) for c in ins.children]
        return d
    tree = [node(r) for r in roots]
    with open(os.path.join(out_dir, "tree.json"), "w", encoding="utf-8") as f:
        json.dump(tree, f, ensure_ascii=False, indent=1)

def report(insts):
    from collections import Counter
    by_class = Counter(i.cls for i in insts.values())
    mapped = sum(c for k, c in by_class.items() if k in CLASS_MAP)
    placeholder = sum(c for k, c in by_class.items()
                      if k not in CLASS_MAP and not _is_service(k))
    services = sum(c for k, c in by_class.items() if _is_service(k))
    total = len(insts)
    print(f"\n=== Import report ===")
    print(f"  total instances : {total}")
    print(f"  mapped to nodes : {mapped} ({100*mapped//total}%)")
    print(f"  placeholders    : {placeholder} ({100*placeholder//total}%)")
    print(f"  services        : {services}")
    miss = sorted(((c, k) for k, c in by_class.items()
                   if k not in CLASS_MAP and not _is_service(k)), reverse=True)
    print(f"\n  Top unmapped classes (porting targets):")
    for c, k in miss[:20]:
        print(f"   {c:6d}  {k}")

# Services (singletons) are placed/merged, not instantiated as scene nodes.
_SERVICE_HINT = ("Service","Workspace","Lighting","Players","Teams","Debris",
                 "Chat","Selection","Terrain","Packages","StudioData","Instance")
def _is_service(cls):
    return cls.endswith("Service") or cls in (
        "Workspace","Lighting","Players","Teams","Debris","ReplicatedStorage",
        "ReplicatedFirst","ServerStorage","StarterGui","StarterPack","StarterPlayer",
        "StarterPlayerScripts","StarterCharacterScripts","Chat","Selection","Terrain",
        "Packages","StudioData","SoundService","RunService","TextChatService",
        "MaterialService","AvatarSettings")

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("place", help="path to the .rbxl file")
    ap.add_argument("--out", default=None, help="output dir (default: <place>_import)")
    args = ap.parse_args()
    out_dir = args.out or (os.path.splitext(args.place)[0] + "_import")
    os.makedirs(out_dir, exist_ok=True)

    insts, roots = parse(args.place)
    print(f"Parsed {len(insts)} instances, {len(roots)} top-level services.")
    n_scripts = export_scripts(insts, roots, out_dir)
    write_manifest(insts, roots, out_dir)
    print(f"Extracted {n_scripts} scripts -> {os.path.join(out_dir,'scripts')}")
    print(f"Tree manifest        -> {os.path.join(out_dir,'tree.json')}")
    report(insts)

if __name__ == "__main__":
    main()

#!/usr/bin/env python3

from __future__ import annotations

import json
import math
import re
from pathlib import Path


SCENE_DIR = Path("assets/models/the_blender_scene")
SOURCE_SCENE = SCENE_DIR / "the_blender_scene_table_close_split.scene"
SOURCE_CAMERA_SCENE = SCENE_DIR / "the_blender_scene_table_close.scene"
SPLIT_MANIFEST = SCENE_DIR / "split_manifest.json"
OUT_SCENE = SCENE_DIR / "the_blender_scene_table_close_split_essential.scene"
OUT_MANIFEST = SCENE_DIR / "the_blender_scene_table_close_split_essential_manifest.json"

ENV_BLOCK = """envmap "../../envmaps/typewriter_studio_soft.hdr" {
    intensity = 0.020
    rotationY = 0.0
}

"""

LIGHT_BLOCK = """light {
    type      = AREA_RECT
    pos       = 3.080 0.140 1.450
    normal    = -1.000 0.000 0.000
    size      = 0.850 0.560 0.000
    emission  = 0.620 0.760 1.000
    intensity = 0.28
    enabled   = true
}

light {
    type      = AREA_RECT
    pos       = 3.080 2.140 1.450
    normal    = -1.000 0.000 0.000
    size      = 0.850 0.560 0.000
    emission  = 0.620 0.760 1.000
    intensity = 0.28
    enabled   = true
}

light {
    type      = AREA_SPHERE
    pos       = 0.370 2.572 -0.040
    radius    = 0.120
    emission  = 1.000 0.823 0.533
    intensity = 120.0
    enabled   = true
}

light {
    type      = AREA_SPHERE
    pos       = 0.629 2.572 -0.492
    radius    = 0.120
    emission  = 1.000 0.778 0.485
    intensity = 120.0
    enabled   = true
}

light {
    type      = AREA_SPHERE
    pos       = 0.369 2.572 -0.921
    radius    = 0.120
    emission  = 1.000 0.752 0.450
    intensity = 120.0
    enabled   = true
}

light {
    type      = AREA_SPHERE
    pos       = -0.160 2.572 -0.942
    radius    = 0.110
    emission  = 1.000 0.731 0.389
    intensity = 145.0
    enabled   = true
}

light {
    type      = AREA_SPHERE
    pos       = -0.416 2.572 -0.471
    radius    = 0.120
    emission  = 1.000 0.773 0.354
    intensity = 120.0
    enabled   = true
}

light {
    type      = AREA_SPHERE
    pos       = 2.384 1.491 1.412
    radius    = 0.120
    emission  = 1.000 0.731 0.389
    intensity = 46.0
    enabled   = true
}

light {
    type      = AREA_SPHERE
    pos       = 2.373 1.520 1.410
    radius    = 0.045
    emission  = 1.000 0.900 0.700
    intensity = 260.0
    enabled   = true
}

light {
    type      = DIRECTIONAL
    dir       = -0.623 -0.613 0.485
    emission  = 1.000 0.929 0.849
    intensity = 3.8
    enabled   = true
}
"""


def sub(a, b):
    return tuple(x - y for x, y in zip(a, b))


def add(a, b):
    return tuple(x + y for x, y in zip(a, b))


def mul(a, s):
    return tuple(x * s for x in a)


def dot(a, b):
    return sum(x * y for x, y in zip(a, b))


def cross(a, b):
    return (
        a[1] * b[2] - a[2] * b[1],
        a[2] * b[0] - a[0] * b[2],
        a[0] * b[1] - a[1] * b[0],
    )


def norm(a):
    return math.sqrt(dot(a, a))


def normalize(a):
    n = norm(a)
    return tuple(x / n for x in a)


def parse_camera():
    text = SOURCE_CAMERA_SCENE.read_text(encoding="utf-8")
    match = re.search(
        r"camera \{.*?pos\s*=\s*([-\d.]+)\s+([-\d.]+)\s+([-\d.]+).*?"
        r"lookAt\s*=\s*([-\d.]+)\s+([-\d.]+)\s+([-\d.]+).*?"
        r"up\s*=\s*([-\d.]+)\s+([-\d.]+)\s+([-\d.]+).*?"
        r"fov\s*=\s*([-\d.]+)",
        text,
        re.S,
    )
    if not match:
        raise RuntimeError("camera block not found")
    vals = list(map(float, match.groups()))
    return {
        "pos": tuple(vals[:3]),
        "lookAt": tuple(vals[3:6]),
        "up": tuple(vals[6:9]),
        "fov": vals[9],
    }


def load_bounds(obj_rel_path: str):
    obj_path = SCENE_DIR / obj_rel_path
    xs, ys, zs = [], [], []
    with obj_path.open(encoding="utf-8") as f:
        for line in f:
            if line.startswith("v "):
                _, x, y, z = line.split()
                xs.append(float(x))
                ys.append(float(y))
                zs.append(float(z))
    return (min(xs), min(ys), min(zs)), (max(xs), max(ys), max(zs))


def inside_plane(bounds, normal, point):
    pmin, pmax = bounds
    center = tuple((a + b) * 0.5 for a, b in zip(pmin, pmax))
    extent = tuple((b - a) * 0.5 for a, b in zip(pmin, pmax))
    radius = extent[0] * abs(normal[0]) + extent[1] * abs(normal[1]) + extent[2] * abs(normal[2])
    signed_distance = dot(normal, sub(center, point))
    return signed_distance + radius >= 0.0


def compute_visible_objects():
    camera = parse_camera()
    pos = camera["pos"]
    look_at = camera["lookAt"]
    up = camera["up"]
    fov = camera["fov"]

    forward = normalize(sub(look_at, pos))
    right = normalize(cross(forward, up))
    up_dir = cross(right, forward)

    aspect = 1200.0 / 600.0
    tan_half_y = math.tan(math.radians(fov) * 0.5)
    tan_half_x = tan_half_y * aspect

    near_point = add(pos, mul(forward, 0.02))
    left_plane = normalize(add(mul(forward, tan_half_x), right))
    right_plane = normalize(sub(mul(forward, tan_half_x), right))
    bottom_plane = normalize(add(mul(forward, tan_half_y), up_dir))
    top_plane = normalize(sub(mul(forward, tan_half_y), up_dir))

    manifest = json.loads(SPLIT_MANIFEST.read_text(encoding="utf-8"))
    visible = []
    for obj in manifest["objects"]:
        bounds = load_bounds(obj["obj"])
        keep = (
            inside_plane(bounds, forward, near_point)
            and inside_plane(bounds, left_plane, pos)
            and inside_plane(bounds, right_plane, pos)
            and inside_plane(bounds, bottom_plane, pos)
            and inside_plane(bounds, top_plane, pos)
        )
        if keep:
            visible.append(obj["name"])
    return visible


def extract_mesh_blocks(lines):
    blocks = []
    i = 0
    while i < len(lines):
        if lines[i].lstrip().startswith('mesh "'):
            start = i
            depth = lines[i].count("{") - lines[i].count("}")
            i += 1
            while i < len(lines) and depth > 0:
                depth += lines[i].count("{") - lines[i].count("}")
                i += 1
            blocks.append(lines[start:i])
            continue
        i += 1
    return blocks


def mesh_name_from_block(block):
    line = block[0].strip()
    match = re.match(r'mesh "split_objs/([^"]+)\.obj"', line)
    if not match:
        raise RuntimeError(f"unexpected mesh line: {line}")
    return match.group(1)


def find_first_mesh_start(lines):
    for i, line in enumerate(lines):
        if line.lstrip().startswith('mesh "'):
            return i
    raise RuntimeError("no mesh block found")


def find_first_light_start(lines):
    for i, line in enumerate(lines):
        if line.lstrip().startswith("light {"):
            return i
    raise RuntimeError("no light block found")


def find_camera_block_end(lines):
    for i, line in enumerate(lines):
        if line.lstrip().startswith("camera {"):
            depth = line.count("{") - line.count("}")
            i += 1
            while i < len(lines) and depth > 0:
                depth += lines[i].count("{") - lines[i].count("}")
                i += 1
            return i
    raise RuntimeError("camera block not found")


def main():
    visible = compute_visible_objects()
    lines = SOURCE_SCENE.read_text(encoding="utf-8").splitlines(keepends=True)
    mesh_blocks = extract_mesh_blocks(lines)

    selected_blocks = []
    selected_names = []
    for block in mesh_blocks:
        name = mesh_name_from_block(block)
        if name in visible:
            selected_blocks.append(block)
            selected_names.append(name)

    first_mesh = find_first_mesh_start(lines)
    first_light = find_first_light_start(lines)
    camera_end = find_camera_block_end(lines)
    header_lines = list(lines[:first_mesh])
    if not any(line.lstrip().startswith("envmap ") for line in header_lines):
        header_lines[camera_end:camera_end] = [ENV_BLOCK]

    new_lines = header_lines
    for block in selected_blocks:
        new_lines.extend(block)
        if not block[-1].endswith("\n"):
            new_lines.append("\n")
        new_lines.append("\n")
    new_lines.append(LIGHT_BLOCK)

    OUT_SCENE.write_text("".join(new_lines), encoding="utf-8")
    OUT_MANIFEST.write_text(
        json.dumps(
            {
                "source_scene": SOURCE_SCENE.name,
                "camera_scene": SOURCE_CAMERA_SCENE.name,
                "selected_object_count": len(selected_names),
                "selected_objects": selected_names,
            },
            indent=2,
            ensure_ascii=False,
        )
        + "\n",
        encoding="utf-8",
    )

    print(f"Wrote {OUT_SCENE.name} with {len(selected_names)} objects.")
    for name in selected_names:
        print(name)


if __name__ == "__main__":
    main()

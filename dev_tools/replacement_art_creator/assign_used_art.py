#!/usr/bin/env python3
"""Assign independent source art to every map-visible wall appearance."""

import argparse
import csv
import json
import struct
from pathlib import Path

ROOT = Path(__file__).resolve().parent
CATALOG = ROOT.parent / "tile_art_viewer/data/tile_art_catalog.csv"
BRIEFS = ROOT / "data/briefs.json"
ART = ROOT / "godot/art"
PACK = ROOT.parents[1] / "data" / "art" / "walls"
VIEW_NAMES = ("Far front", "Far left", "Far right", "Middle front", "Middle left",
              "Middle right", "Near front", "Near left", "Near right", "Far extension")
VIEW_SIZES = ((8, 16), (8, 32), (8, 32), (24, 32), (16, 64),
              (16, 64), (56, 64), (16, 88), (16, 88), (8, 16))


def spec(base, title, feature="", kind="solid"):
    return {"base": base, "feature": feature, "kind": kind, "title": title}


# Record and appearance identifiers are technical slots in the game's wall
# definitions. Shared slots intentionally reuse one new design across archives.
SPECS = {
    "1:1": spec("base_stone", "Open masonry passage", "arch_overlay", "opening"),
    "1:2": spec("base_bluegray_limewash", "Stone fountain", "feature_fountain", "landmark"),
    "1:3": spec("base_bluegray_limewash", "Open stone doorway", "feature_stone_gate", "rect_opening"),
    "1:4": spec("base_stone", "Rugged masonry"),
    "1:5": spec("base_stone", "Closed wooden door", "door_overlay", "door"),
    "2:2": spec("base_timber_planks", "Timber wall with blue window", "feature_blue_window", "landmark"),
    "2:3": spec("base_timber_planks", "Timber facade with door and window", "feature_timber_facade", "facade"),
    "2:4": spec("base_timber_planks", "Vertical timber wall"),
    "2:5": spec("base_timber_planks", "Timber wall with wooden door", "feature_narrow_door", "door"),
    "3:1": spec("base_pale_limestone", "Pale weathered masonry"),
    "3:2": spec("base_pale_limestone", "Pale masonry with wooden door", "feature_narrow_door", "door"),
    "3:3": spec("base_pale_limestone", "Pale masonry arch", "arch_overlay", "opening"),
    "3:4": spec("base_gray_rock", "Vine covered masonry", "feature_ivy", "facade"),
    "3:5": spec("base_gray_rock", "Vine covered masonry with door", "feature_narrow_door", "door"),
    "4:1": spec("base_pale_limestone", "Storefront with awning", "feature_storefront", "facade"),
    "4:2": spec("base_pale_limestone", "Pale irregular stone"),
    "4:3": spec("base_pale_limestone", "Storefront door and window", "feature_storefront", "facade"),
    "4:4": spec("base_pale_limestone", "Pale stone wall"),
    "4:5": spec("base_pale_limestone", "Pale stone doorway", "feature_narrow_door", "door"),
    "5:1": spec("base_redbrick", "Brick facade with columns and door", "feature_temple_door", "facade"),
    "5:2": spec("base_redbrick", "Brick wall with pale columns", "feature_civic_columns", "facade"),
    "5:3": spec("base_redbrick", "Violet glass shrine", "feature_shrine", "landmark"),
    "5:4": spec("base_water", "Waterfront railing", "feature_pier", "facade"),
    "5:5": spec("base_mosaic_wall", "Abstract ceramic mosaic"),
    "6:1": spec("base_palisade", "Pointed timber palisade"),
    "6:2": spec("base_palisade", "Palisade gate", "feature_palisade_gate", "facade"),
    "6:3": spec("base_palisade", "Barred timber palisade"),
    "6:4": spec("base_palisade", "Crossbar palisade gate", "feature_palisade_gate", "facade"),
    "6:5": spec("base_palisade", "Upright timber stakes"),
    "7:1": spec("base_woven_wall", "Golden reed wall"),
    "7:2": spec("base_woven_wall", "Reed wall with dark doorway", kind="rect_opening"),
    "7:5": spec("base_low_woven_barrier", "Low woven barrier"),
    "8:1": spec("base_tree_path", "Woodland path"),
    "8:2": spec("feature_tree", "Slender leafy tree"),
    "8:3": spec("feature_tree", "Leafy tree with visible trunk"),
    "8:4": spec("base_foliage", "Dense green thicket"),
    "9:1": spec("base_bluegray_limewash", "Broad timber double doors", "feature_double_door", "facade"),
    "9:2": spec("base_bluegray_limewash", "Stone wall with teal molding", "feature_cyan_trim", "facade"),
    "9:3": spec("base_bluegray_limewash", "Open doorway with steps", "feature_stone_gate", "rect_opening"),
    "9:4": spec("base_bluegray_limewash", "Stone wall with teal top trim", "feature_cyan_trim", "facade"),
    "9:5": spec("base_bluegray_limewash", "Trimmed stone wall with door", "feature_narrow_door", "door"),
    "17:1": spec("base_red_cavern", "Jagged red cavern"),
    "17:2": spec("base_red_cavern", "Iron rich cavern fissures"),
    "17:3": spec("base_red_cavern", "Cavern opening", "feature_cave_opening", "cave_opening"),
    "17:4": spec("base_red_cavern", "Ochre cavern passage", "feature_cave_opening", "cave_opening"),
    "17:5": spec("base_red_cavern", "Cavern wooden door", "feature_narrow_door", "door"),
    "18:1": spec("base_red_cavern", "Dark red cavern"),
    "18:2": spec("base_red_cavern", "Cavern ladder", "feature_ladder", "facade"),
    "18:3": spec("base_brown_rock", "Mottled brown cavern"),
    "18:4": spec("base_red_cavern", "Cavern arched door", "feature_narrow_door", "door"),
    "18:5": spec("base_brown_rock", "Brown cavern wooden door", "feature_narrow_door", "door"),
    "19:1": spec("base_redbrick", "Civic brick facade with barred entrance", "feature_civic_entrance", "facade"),
    "19:2": spec("base_redbrick", "Weathered red brick"),
    "19:3": spec("base_bookcase", "Colorful bookcase"),
    "19:4": spec("base_bookcase", "Tall library shelves"),
    "19:5": spec("base_redbrick", "Brick wall with wooden door", "feature_narrow_door", "door"),
    "20:1": spec("base_broken_battlement", "Low broken battlement"),
    "20:2": spec("base_gray_rock", "Stone wall with colored lamp", "feature_recessed_lamp", "landmark"),
    "20:3": spec("base_gray_rock", "Stone steps and colored lamp", "feature_lamp_steps", "facade"),
    "20:4": spec("base_broken_battlement", "Broken stone battlement"),
    "20:5": spec("base_broken_battlement", "Broken battlement with wooden door", "feature_narrow_door", "door"),
    "21:1": spec("base_gray_rock", "Rough stone archway", "arch_overlay", "opening"),
    "21:2": spec("feature_stone_cross", "Weathered stone cross"),
    "21:3": spec("feature_gravestone", "Rounded gravestone"),
    "21:4": spec("base_gray_rock", "Mottled gray rock"),
    "21:5": spec("base_bluegray_limewash", "Uneven gray stone blocks"),
    "21:6": spec("base_stone_panels", "Stone wall with recessed panels"),
    "21:7": spec("base_broken_battlement", "Wide battlement gate", "feature_double_door", "facade"),
    "21:8": spec("feature_iron_fence", "Wrought iron fence"),
    "21:9": spec("base_gray_rock", "Rough open stone gateway", "feature_stone_gate", "rect_opening"),
    "21:10": spec("base_gray_rock", "Stone wall with wooden door", "feature_narrow_door", "door"),
    "23:1": spec("base_water", "Blue water"),
    "23:2": spec("base_water", "Wooden waterfront railing", "feature_pier", "facade"),
    "23:3": spec("base_reedy_bank", "Foliage on low earthen bank"),
    "23:4": spec("base_reedy_bank", "Reeds along earthen bank"),
    "24:1": spec("base_pale_limestone", "Tall wooden gate", "feature_temple_door", "facade"),
    "24:2": spec("base_pale_limestone", "Pale stone archway and steps", "feature_stone_gate", "rect_opening"),
    "24:3": spec("base_pale_limestone", "Plain pale stone"),
    "24:4": spec("base_pale_limestone", "Pale stone arched door", "feature_narrow_door", "door"),
    "24:5": spec("base_pale_broken_wall", "Broken pale stone wall"),
    "24:6": spec("base_bookcase", "Books in pale wall"),
    "24:7": spec("base_foliage", "Dense pale flowering foliage"),
    "24:8": spec("base_foliage", "Narrow hedge"),
    "24:9": spec("base_foliage", "Leafy thicket"),
    "24:10": spec("base_water", "Small water patch"),
    "0:11": spec("base_redbrick", "Brick civic facade with blue windows", "feature_civic_windows", "facade"),
    "0:12": spec("base_redbrick", "Brick hall with blue entrance", "feature_blue_entrance", "facade"),
    "0:15": spec("base_water", "Pale waterfront wall", "feature_waterfront_wall", "facade"),
}

RECORD_ZERO = {
    1: "21:9", 2: "9:2", 3: "9:5", 4: "3:4", 5: "3:5",
    6: "24:3", 7: "24:4", 8: "24:3", 9: "4:1", 10: "4:3",
    11: "0:11", 12: "0:12", 13: "5:1", 14: "23:2", 15: "0:15",
}
PILOTS = {"WALLDEF1.DAX:1:1", "WALLDEF1.DAX:1:4", "WALLDEF1.DAX:1:5", "WALLDEF1.DAX:5:3"}


def assignments():
    grouped = {}
    with CATALOG.open(newline="") as source:
        for row in csv.DictReader(source):
            key = f"{row['archive']}:{row['record']}:{row['appearance']}"
            grouped.setdefault(key, []).append(row)
    existing = json.loads(BRIEFS.read_text())
    result = {}
    for key, rows in grouped.items():
        if not any(row["usage"] == "visible" for row in rows):
            continue
        _, record, appearance = key.split(":")
        slot = f"{record}:{appearance}"
        if record == "0":
            slot = RECORD_ZERO[int(appearance)]
        if slot not in SPECS:
            raise ValueError(f"No art assignment for used appearance {key}")
        entry = SPECS[slot].copy()
        entry["base"] = f"res://art/{entry['base']}.png"
        entry["feature"] = f"res://art/{entry['feature']}.png" if entry["feature"] else ""
        entry["brief"] = (f"Original pixel-art {entry['title'].lower()} for a fantasy first-person map. "
                          "Use the listed independent source sprites; avoid existing game motifs.")
        if key in PILOTS:
            entry = existing[key]
        elif "draft_wall_prompt" in existing.get(key, {}):
            entry["draft_wall_prompt"] = existing[key]["draft_wall_prompt"]
        for path in (entry["base"], entry["feature"]):
            if path and not (ART / Path(path).name).is_file():
                raise ValueError(f"Missing source art {path} for {key}")
        result[key] = entry
    if len(result) != 168:
        raise ValueError(f"Expected 168 used appearances; found {len(result)}")
    if not PILOTS.issubset(result):
        raise ValueError("A pilot appearance is missing from the visible catalog")
    return result


def check_pack(expected):
    directories = {path.name: path for path in PACK.iterdir() if path.is_dir()}
    stems = {key.replace(".", "_").replace(":", "_") for key in expected}
    if set(directories) != stems:
        raise ValueError(f"Pack directories differ from used appearances: "
                         f"missing={len(stems - set(directories))}, extra={len(set(directories) - stems)}")
    pieces = 0
    for key, brief in expected.items():
        directory = directories[key.replace(".", "_").replace(":", "_")]
        manifest = json.loads((directory / "manifest.json").read_text())
        if manifest["format"] != "opengold-replacement-art-v1" or manifest["appearance_key"] != key:
            raise ValueError(f"Invalid manifest for {key}")
        if any(manifest[field] != brief[field] for field in ("title", "kind", "brief")):
            raise ValueError(f"Stale brief in manifest for {key}")
        files = manifest["files"]
        if set(files) != set(VIEW_NAMES):
            raise ValueError(f"Incomplete view names for {key}")
        if len(list(directory.glob("*.png"))) != 10:
            raise ValueError(f"Expected ten PNGs for {key}")
        for view, (width, height) in enumerate(VIEW_SIZES):
            image = directory / files[VIEW_NAMES[view]]
            header = image.read_bytes()[:24]
            if header[:8] != b"\x89PNG\r\n\x1a\n" or header[12:16] != b"IHDR":
                raise ValueError(f"Invalid PNG for {key}: {image.name}")
            actual = struct.unpack(">II", header[16:24])
            if actual != (width * 4, height * 4):
                raise ValueError(f"Wrong perspective dimensions for {key}: {image.name} {actual}")
            pieces += 1
    print(f"Verified {len(expected)} used appearances and {pieces} complete perspective PNGs")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--write", action="store_true", help="Write the complete used-art briefs")
    parser.add_argument("--check-pack", action="store_true", help="Verify the complete exported pack")
    args = parser.parse_args()
    output = assignments()
    if args.write:
        BRIEFS.write_text(json.dumps(output, indent=2, ensure_ascii=False) + "\n")
    print(f"Assigned {len(output)} used appearances from {len(SPECS)} technical slots")
    if args.check_pack:
        check_pack(output)

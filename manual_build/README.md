# Manual build & versioning (PICO2W PDX Step-1)

Automated User Manual packaging for Milestone 1.

## Layout

```
manual_build/
  VERSION                 # semantic version, e.g. 1.4.0
  CHANGELOG.md            # human-readable history
  build_manual.py         # builds the .docx
  bump_version.sh         # bump + changelog + rebuild
  figures/
    manifest.json         # figure registry (id → file, caption, type)
    *.png                 # screenshots, diagrams, placeholders
  output/
    PICO2W_PDX_Step1_User_Manual_vX.Y.Z.docx
    PICO2W_PDX_Step1_User_Manual_latest.docx
```

## Commands

```bash
cd manual_build

# Verify all figures listed in manifest exist
python3 build_manual.py --check

# Rebuild current VERSION (no bump)
python3 build_manual.py

# Bump and rebuild
./bump_version.sh patch "Fix caption on OLED figure"
./bump_version.sh minor "Add USB Audio section"
./bump_version.sh major "Step-2 manual baseline"
```

## Replacing a placeholder photo

1. Save your photo as e.g. `figures/ph_oled.png` (same filename).
2. Keep `type: "placeholder"` or change to `"photo"` in `manifest.json`.
3. Run `python3 build_manual.py` (or bump if you want a new version).

## Adding a new figure

1. Add the image under `figures/`.
2. Append an entry to `figures/manifest.json` with unique `id`, `file`, `caption`, `type`, `section`.
3. Reference that `id` in `build_manual.py` via `add_fig(doc, manifest, "your_id", ...)`.
4. Rebuild / bump.

## Version policy

| Part  | When to use                          |
|-------|--------------------------------------|
| patch | Typos, captions, figure swaps        |
| minor | New sections or screenshots          |
| major | New project milestone (e.g. Step-2)  |

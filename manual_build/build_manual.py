#!/usr/bin/env python3
"""
PICO2W PDX Step-1 User Manual builder
-------------------------------------
Reads VERSION, figures/manifest.json, CHANGELOG.md
Writes output/PICO2W_PDX_Step1_User_Manual_vX.Y.Z.docx

Usage:
  python3 build_manual.py              # build current VERSION
  python3 build_manual.py --check      # verify figures exist
"""

from __future__ import annotations

import argparse
import json
import sys
from datetime import date
from pathlib import Path

from docx import Document
from docx.shared import Inches, Pt, RGBColor, Cm
from docx.enum.text import WD_ALIGN_PARAGRAPH
from docx.enum.table import WD_TABLE_ALIGNMENT
from docx.oxml.ns import qn
from docx.oxml import OxmlElement

ROOT = Path(__file__).resolve().parent
FIGURES = ROOT / "figures"
OUTPUT = ROOT / "output"
VERSION_FILE = ROOT / "VERSION"
MANIFEST = FIGURES / "manifest.json"
CHANGELOG = ROOT / "CHANGELOG.md"

STATION = "VU2UPX"
GRID = "MK69KE"
FIRMWARE = "PICO2W_PDX_Step1"


def read_version() -> str:
    return VERSION_FILE.read_text().strip()


def load_manifest() -> dict:
    return json.loads(MANIFEST.read_text())


def fig_path(name: str) -> Path:
    return FIGURES / name


def set_run_font(run, name="Calibri", size=11, bold=False, color=None):
    run.font.name = name
    run._element.rPr.rFonts.set(qn("w:eastAsia"), name)
    run.font.size = Pt(size)
    run.bold = bold
    if color:
        run.font.color.rgb = RGBColor(*color)


def add_heading(doc, text, level=1):
    p = doc.add_heading(text, level=level)
    for run in p.runs:
        set_run_font(
            run,
            size=16 if level == 1 else (13 if level == 2 else 12),
            bold=True,
            color=(0, 70, 127),
        )


def add_para(doc, text, bold=False, size=11, space_after=6):
    p = doc.add_paragraph()
    run = p.add_run(text)
    set_run_font(run, size=size, bold=bold)
    p.paragraph_format.space_after = Pt(space_after)


def add_bullet(doc, text):
    p = doc.add_paragraph(text, style="List Bullet")
    for run in p.runs:
        set_run_font(run, size=11)
    p.paragraph_format.space_after = Pt(2)


def shade_cell(cell, hex_color):
    tc = cell._tc
    tcPr = tc.get_or_add_tcPr()
    shd = OxmlElement("w:shd")
    shd.set(qn("w:fill"), hex_color)
    shd.set(qn("w:val"), "clear")
    tcPr.append(shd)


def add_table(doc, headers, rows):
    table = doc.add_table(rows=1 + len(rows), cols=len(headers))
    table.style = "Table Grid"
    table.alignment = WD_TABLE_ALIGNMENT.CENTER
    for i, h in enumerate(headers):
        cell = table.rows[0].cells[i]
        cell.text = ""
        run = cell.paragraphs[0].add_run(h)
        set_run_font(run, size=10, bold=True, color=(255, 255, 255))
        shade_cell(cell, "00467F")
    for r_i, row in enumerate(rows):
        for c_i, val in enumerate(row):
            cell = table.rows[r_i + 1].cells[c_i]
            cell.text = ""
            run = cell.paragraphs[0].add_run(str(val))
            set_run_font(run, size=10)
            if r_i % 2 == 1:
                shade_cell(cell, "E8F0F8")
    doc.add_paragraph()


def add_image(doc, path: Path, width_in=4.0, caption=None):
    p = doc.add_paragraph()
    p.alignment = WD_ALIGN_PARAGRAPH.CENTER
    if path.exists():
        p.add_run().add_picture(str(path), width=Inches(width_in))
    else:
        run = p.add_run(f"[MISSING FIGURE: {path.name}]")
        set_run_font(run, bold=True, color=(180, 0, 0))
    if caption:
        cp = doc.add_paragraph()
        cp.alignment = WD_ALIGN_PARAGRAPH.CENTER
        r = cp.add_run(caption)
        set_run_font(r, size=9, color=(100, 100, 100))


def fig_by_id(manifest, fid: str) -> dict | None:
    for f in manifest["figures"]:
        if f["id"] == fid:
            return f
    return None


def add_fig(doc, manifest, fid: str, width_in=4.0, fig_num: int | None = None):
    meta = fig_by_id(manifest, fid)
    if not meta:
        add_para(doc, f"[Unknown figure id: {fid}]", bold=True)
        return
    path = fig_path(meta["file"])
    tag = meta.get("type", "")
    prefix = f"Figure {fig_num} — " if fig_num is not None else ""
    suffix = f" [{tag}]" if tag == "placeholder" else ""
    caption = f"{prefix}{meta['caption']}{suffix}"
    add_image(doc, path, width_in=width_in, caption=caption)


def check_figures(manifest) -> list[str]:
    missing = []
    for f in manifest["figures"]:
        p = fig_path(f["file"])
        if not p.exists():
            missing.append(f["file"])
    return missing


def build(version: str, manifest: dict) -> Path:
    doc = Document()
    for section in doc.sections:
        section.top_margin = Cm(2)
        section.bottom_margin = Cm(2)
        section.left_margin = Cm(2.2)
        section.right_margin = Cm(2.2)

    # Title
    p = doc.add_paragraph()
    p.alignment = WD_ALIGN_PARAGRAPH.CENTER
    run = p.add_run("PICO-2W PDX")
    set_run_font(run, size=28, bold=True, color=(0, 70, 127))

    p = doc.add_paragraph()
    p.alignment = WD_ALIGN_PARAGRAPH.CENTER
    run = p.add_run("Digital Modes Transceiver")
    set_run_font(run, size=16, bold=True)

    p = doc.add_paragraph()
    p.alignment = WD_ALIGN_PARAGRAPH.CENTER
    run = p.add_run("Step-1 User & Operations Manual")
    set_run_font(run, size=14, color=(80, 80, 80))

    p = doc.add_paragraph()
    p.alignment = WD_ALIGN_PARAGRAPH.CENTER
    run = p.add_run(f"Station: {STATION} · Grid: {GRID} · Pico 2W (RP2350)")
    set_run_font(run, size=11)

    p = doc.add_paragraph()
    p.alignment = WD_ALIGN_PARAGRAPH.CENTER
    run = p.add_run(f"Firmware: {FIRMWARE} · Manual version {version} · {date.today().isoformat()}")
    set_run_font(run, size=10, bold=True, color=(0, 100, 60))
    doc.add_paragraph()

    # 1
    add_heading(doc, "1. Purpose of Step-1", 1)
    add_para(
        doc,
        "Step-1 is the first milestone of the PICO-2W PDX project: a standalone digital-modes "
        "beacon platform on the Raspberry Pi Pico 2W with Si5351, OLED, optional TFT TX log, "
        "WiFi/NTP, web UI (Beacon On/Off), and FT8/WSPR beacons.",
    )
    add_heading(doc, "1.1 Hardware", 2)
    add_para(doc, "Photo of the assembled unit (placeholder until replaced).")
    add_fig(doc, manifest, "fig_hardware", width_in=5.0, fig_num=1)

    # 2
    add_heading(doc, "2. Web interface", 1)
    add_para(
        doc,
        "Open the STA IP shown on the OLED, or http://192.168.4.1 in AP/config portal mode. "
        "Screenshots below are rendered from the firmware web page markup.",
    )
    add_heading(doc, "2.1 Main page (Beacon OFF)", 2)
    add_fig(doc, manifest, "fig_web_off", width_in=3.5, fig_num=2)
    add_heading(doc, "2.2 Main page (Beacon ON)", 2)
    add_fig(doc, manifest, "fig_web_on", width_in=3.5, fig_num=3)
    add_heading(doc, "2.3 Soft buttons", 2)
    add_table(
        doc,
        ["Button", "Action"],
        [
            ["Mode (A)", "Cycle FT8 → FT4 → JS8 → WSPR"],
            ["Band Up / Dn", "Change HF band"],
            ["Tune / PTT", "Toggle carrier TX / RX"],
            ["Menu / Cal", "Enter menu / calibration"],
            ["Beacon ON/OFF", "Enable/disable beacon scheduler (default OFF)"],
        ],
    )

    # 3
    add_heading(doc, "3. Displays", 1)
    add_heading(doc, "3.1 OLED (primary)", 2)
    add_table(
        doc,
        ["Line", "Content"],
        [
            ["1", "Mode + IST time"],
            ["2", "Dial frequency"],
            ["3", "RX/TX, band, BCN/--- or TX progress"],
            ["4", "STA or AP IP"],
        ],
    )
    add_fig(doc, manifest, "fig_oled", width_in=4.0, fig_num=4)
    add_heading(doc, "3.2 TFT TX log", 2)
    add_para(doc, "Format: MODE  BAND  DD-MM-YYYY  HH:MM:SS (IST). Header shows BEACON ON/OFF.")
    add_fig(doc, manifest, "fig_tft", width_in=2.8, fig_num=5)

    # 4
    add_heading(doc, "4. Front-panel controls", 1)
    add_table(
        doc,
        ["Control", "Short", "Long"],
        [
            ["Switch A", "Mode cycle", "Save EEPROM & exit menu/cal"],
            ["Switch B", "Band up", "Cancel menu/cal"],
            ["Switch C", "Band down", "Cancel menu/cal"],
            ["Encoder SW", "—", "Enter CAL / menu"],
            ["Encoder", "Adjust corr in CAL", "—"],
        ],
    )

    # 5
    add_heading(doc, "5. FT8 and WSPR beacons", 1)
    add_para(doc, "Only when Beacon is ON and mode is FT8 or WSPR. Never concurrent.")
    add_table(
        doc,
        ["Mode", "Timing", "Notes"],
        [
            ["FT8", "Once per FT8_TX_EVERY_MIN at UTC ~:00", "79 symbols, Costas"],
            ["WSPR", "Even UTC minutes; every WSPR_TX_EVERY_MIN", "Type 1 call+grid+dBm"],
        ],
    )

    # 6
    add_heading(doc, "6. Configuration (config.h)", 1)
    add_table(
        doc,
        ["Define", "Purpose"],
        [
            ["MY_CALLSIGN / MY_GRID4", "Station identity"],
            ["WSPR_DBM / WSPR_TX_EVERY_MIN", "Power and WSPR interval"],
            ["FT8_TX_EVERY_MIN", "FT8 interval"],
            ["DISPLAY_OPTION", "1 = TFT log"],
            ["WIFI_AP_SSID / PASS", "Config portal"],
        ],
    )

    # 7
    add_heading(doc, "7. Getting started", 1)
    for i, t in enumerate(
        [
            "Install libraries; configure TFT_eSPI if needed.",
            "Upload with Pico 2W + Pico SDK USB stack.",
            "Connect WiFi; confirm IST on OLED.",
            "Calibrate Si5351 (Section 8).",
            "Select mode; Beacon ON; confirm TFT log.",
        ],
        1,
    ):
        add_para(doc, f"{i}. {t}")

    # 8
    add_heading(doc, "8. Si5351 calibration procedure", 1)
    add_para(
        doc,
        "CAL mode outputs 10.000 MHz on CLK1. RX_EN and Tx/Rx relay off; CLK0/CLK2 disabled.",
    )
    add_fig(doc, manifest, "fig_cal_flow", width_in=5.5, fig_num=6)
    add_fig(doc, manifest, "fig_cal_setup", width_in=5.0, fig_num=7)
    add_heading(doc, "8.1 Step-by-step", 2)
    add_para(doc, "1. Beacon OFF. Long-press encoder → CAL (OLED: 10MHz CLK1).")
    add_para(doc, "2. Measure CLK1; rotate encoder until counter reads 10,000,000 Hz.")
    add_para(doc, "3. Long-press Switch A to save and exit. B or C cancels.")
    add_para(doc, "4. Optional: set correction on web UI → Save EEPROM.")

    # 9
    add_heading(doc, "9. RF / antenna", 1)
    add_para(doc, "Use a dummy load or legal antenna. Match WSPR_DBM to real power.")
    add_fig(doc, manifest, "fig_antenna", width_in=5.0, fig_num=8)

    # 10
    add_heading(doc, "10. Operating notes", 1)
    add_bullet(doc, "Keep Beacon OFF while calibrating.")
    add_bullet(doc, "WSPR TX ~111 s — monitor PA temperature.")
    add_bullet(doc, "FT8 free-text may decode as free text in WSJT-X.")

    # 11
    add_heading(doc, "11. Known limits", 1)
    for t in [
        "No DC RX path yet",
        "No USB Audio for WSJT-X",
        "No standard FT8 CQ packing yet",
        "No Si4732/PCF8574 automation",
    ]:
        add_bullet(doc, t)

    # 12 versioning note
    add_heading(doc, "12. Manual versioning", 1)
    add_para(
        doc,
        "This document is produced by the automated builder in manual_build/. "
        "Version is read from VERSION; figures from figures/manifest.json.",
    )
    add_table(
        doc,
        ["Command", "Effect"],
        [
            ["./bump_version.sh patch", "1.4.0 → 1.4.1 and rebuild"],
            ["./bump_version.sh minor", "1.4.0 → 1.5.0 and rebuild"],
            ["./bump_version.sh major", "1.4.0 → 2.0.0 and rebuild"],
            ["python3 build_manual.py", "Rebuild current VERSION only"],
            ["python3 build_manual.py --check", "Verify all figures exist"],
        ],
    )

    # 13 doc control
    add_heading(doc, "13. Document control", 1)
    placeholders = sum(1 for f in manifest["figures"] if f.get("type") == "placeholder")
    screenshots = sum(1 for f in manifest["figures"] if f.get("type") == "screenshot")
    add_table(
        doc,
        ["Field", "Value"],
        [
            ["Manual version", version],
            ["Build date", date.today().isoformat()],
            ["Station", f"{STATION} · {GRID}"],
            ["Firmware", FIRMWARE],
            ["Figures total", str(len(manifest["figures"]))],
            ["Screenshots", str(screenshots)],
            ["Placeholders", str(placeholders)],
        ],
    )

    p = doc.add_paragraph()
    p.alignment = WD_ALIGN_PARAGRAPH.CENTER
    run = p.add_run(f"— End of Step-1 User & Operations Manual v{version} —")
    set_run_font(run, size=10, color=(100, 100, 100))

    OUTPUT.mkdir(parents=True, exist_ok=True)
    out = OUTPUT / f"PICO2W_PDX_Step1_User_Manual_v{version}.docx"
    # also write a stable "latest" copy
    latest = OUTPUT / "PICO2W_PDX_Step1_User_Manual_latest.docx"
    doc.save(out)
    doc.save(latest)
    # copy to artifacts root for easy download
    root_copy = ROOT.parent / f"PICO2W_PDX_Step1_User_Manual_v{version}.docx"
    doc.save(root_copy)
    return out


def main():
    ap = argparse.ArgumentParser(description="Build PICO2W PDX Step-1 user manual")
    ap.add_argument("--check", action="store_true", help="Only verify figures")
    args = ap.parse_args()

    version = read_version()
    manifest = load_manifest()
    missing = check_figures(manifest)
    if missing:
        print("Missing figures:", ", ".join(missing), file=sys.stderr)
        if args.check:
            sys.exit(1)
        print("Continuing; missing figures will show as [MISSING FIGURE] in the doc.")
    else:
        print(f"All {len(manifest['figures'])} figures present.")

    if args.check:
        print(f"VERSION {version} OK")
        sys.exit(0)

    out = build(version, manifest)
    print(f"Built: {out}")
    print(f"Latest: {OUTPUT / 'PICO2W_PDX_Step1_User_Manual_latest.docx'}")


if __name__ == "__main__":
    main()

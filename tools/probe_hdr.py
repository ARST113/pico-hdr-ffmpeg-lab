#!/usr/bin/env python3
"""Classify HDR metadata from ffprobe JSON.

Run:
    python tools/probe_hdr.py samples/hdr10_ffprobe.json
"""

from __future__ import annotations

import json
import sys
from dataclasses import dataclass
from pathlib import Path
from typing import Any


BT2020_VALUES = {"bt2020", "bt2020nc", "bt2020ncl", "bt2020c", "bt2020_cl", "bt2020_ncl"}
PQ_VALUES = {"smpte2084", "st2084", "pq"}
HLG_VALUES = {"arib-std-b67", "arib_std_b67", "hlg"}
DOLBY_KEYS = {"dovi", "dolby_vision", "dovi_profile", "dv_profile"}


@dataclass(frozen=True)
class HdrDecision:
    is_hdr: bool
    kind: str
    reasons: tuple[str, ...]


def _norm(value: Any) -> str:
    return str(value or "").strip().lower()


def _first_video_stream(document: dict[str, Any]) -> dict[str, Any]:
    for stream in document.get("streams", []):
        if stream.get("codec_type") == "video":
            return stream
    return {}


def _has_dovi(stream: dict[str, Any]) -> bool:
    tags = stream.get("tags") or {}
    side_data = stream.get("side_data_list") or []

    values: list[str] = []
    values.extend(_norm(v) for v in tags.values())
    for item in side_data:
        values.extend(_norm(v) for v in item.values())
        values.extend(_norm(k) for k in item.keys())

    return any(any(key in value for key in DOLBY_KEYS) for value in values)


def classify_stream(stream: dict[str, Any], force_hdr: bool = False) -> HdrDecision:
    color_space = _norm(stream.get("color_space"))
    color_transfer = _norm(stream.get("color_transfer"))
    color_primaries = _norm(stream.get("color_primaries"))
    pix_fmt = _norm(stream.get("pix_fmt"))

    reasons: list[str] = []
    is_bt2020 = color_space in BT2020_VALUES or color_primaries in BT2020_VALUES
    is_pq = color_transfer in PQ_VALUES
    is_hlg = color_transfer in HLG_VALUES
    is_10_bit = "10" in pix_fmt or pix_fmt.startswith("p010")
    has_dovi = _has_dovi(stream)

    if force_hdr:
        reasons.append("manual force HDR")
        return HdrDecision(True, "forced", tuple(reasons))

    if has_dovi:
        reasons.append("Dolby Vision metadata/profile present")
        return HdrDecision(True, "dolby-vision", tuple(reasons))

    if is_bt2020 and is_pq:
        reasons.append("BT.2020 + ST2084/PQ")
        if is_10_bit:
            reasons.append(f"10-bit pixel format: {pix_fmt}")
        return HdrDecision(True, "hdr10-pq", tuple(reasons))

    if is_bt2020 and is_hlg:
        reasons.append("BT.2020 + HLG")
        if is_10_bit:
            reasons.append(f"10-bit pixel format: {pix_fmt}")
        return HdrDecision(True, "hlg", tuple(reasons))

    if is_pq or is_hlg:
        reasons.append(f"HDR transfer without BT.2020 metadata: {color_transfer}")
        return HdrDecision(True, "hdr-transfer", tuple(reasons))

    reasons.append("no HDR color transfer/profile detected")
    return HdrDecision(False, "sdr", tuple(reasons))


def main(argv: list[str]) -> int:
    if len(argv) < 2:
        print("usage: probe_hdr.py FFPROBE_JSON [--force-hdr]", file=sys.stderr)
        return 2

    path = Path(argv[1])
    document = json.loads(path.read_text(encoding="utf-8"))
    decision = classify_stream(_first_video_stream(document), "--force-hdr" in argv[2:])

    print(json.dumps(decision.__dict__, ensure_ascii=False, indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv))


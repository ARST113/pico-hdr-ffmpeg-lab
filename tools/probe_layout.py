#!/usr/bin/env python3
"""Classify 2D/3D packing and VR projection from a media name."""

from __future__ import annotations

import json
import sys
from dataclasses import asdict, dataclass
from enum import Enum


class StereoPacking(str, Enum):
    MONO_2D = "mono-2d"
    HALF_SBS = "half-sbs"
    HALF_OU = "half-ou"
    FULL_SBS = "full-sbs"
    FULL_OU = "full-ou"
    MVC = "mvc"
    MV_HEVC = "mv-hevc"
    UNKNOWN_STEREO = "unknown-stereo"


class Projection(str, Enum):
    FLAT = "flat"
    EQUIRECT_180 = "equirect-180"
    EQUIRECT_360 = "equirect-360"
    YOUTUBE_360 = "youtube-360"
    FISHEYE_180 = "fisheye-180"
    FISHEYE_190 = "fisheye-190"
    FISHEYE_200 = "fisheye-200"
    FISHEYE_220 = "fisheye-220"
    CUBE = "cube"


class RenderGeometry(str, Enum):
    FLAT_QUAD = "flat-quad"
    CURVED_SCREEN = "curved-screen"
    EQUIRECT_HALF_DOME = "equirect-half-dome"
    EQUIRECT_SPHERE = "equirect-sphere"
    FISHEYE_DOME = "fisheye-dome"
    CUBE = "cube"


@dataclass(frozen=True)
class UvRect:
    u0: float
    v0: float
    u1: float
    v1: float


@dataclass(frozen=True)
class LayoutDecision:
    stereo_packing: StereoPacking
    projection: Projection
    render_geometry: RenderGeometry
    left_uv: UvRect
    right_uv: UvRect
    requires_two_eye_decode: bool
    fisheye_fov_degrees: float | None


def has_any(value: str, *needles: str) -> bool:
    return any(needle in value for needle in needles)


def detect_projection(name: str) -> Projection:
    if "cube" in name:
        return Projection.CUBE
    if "fisheye220" in name or "fisheye 220" in name:
        return Projection.FISHEYE_220
    if "fisheye200" in name or "fisheye 200" in name:
        return Projection.FISHEYE_200
    if "fisheye190" in name or "fisheye 190" in name:
        return Projection.FISHEYE_190
    if "fisheye" in name or ".180x180." in name:
        return Projection.FISHEYE_180
    if "youtube360" in name or "youtube 360" in name:
        return Projection.YOUTUBE_360
    if has_any(name, ".360.", "360vr", "vr360", "2d 360", "3d 360"):
        return Projection.EQUIRECT_360
    if has_any(name, ".180.", "180vr", "vr180", "2d 180", "3d 180"):
        return Projection.EQUIRECT_180
    if ".vr." in name:
        return Projection.EQUIRECT_360
    return Projection.FLAT


def detect_stereo_packing(name: str, stereo_mode: str = "") -> tuple[StereoPacking, bool]:
    reverse_eye = False
    if has_any(name, ".2d.", "_2d_", "-2d-", " 2d "):
        return StereoPacking.MONO_2D, reverse_eye
    if "mv-hevc" in name or "mvhevc" in name:
        return StereoPacking.MV_HEVC, reverse_eye
    if name.endswith(".ssif") or ".ssif." in name or ".mvc." in name:
        return StereoPacking.MVC, reverse_eye
    if has_any(name, ".fsbs.", "full.sbs", "fullsbs", "full sbs"):
        return StereoPacking.FULL_SBS, reverse_eye
    if has_any(name, ".hsbs.", "half.sbs", "halfsbs", "half sbs"):
        return StereoPacking.HALF_SBS, reverse_eye
    if has_any(name, ".fou.", "full.ou", "fullou", "full ou"):
        return StereoPacking.FULL_OU, reverse_eye
    if has_any(name, ".hou.", "half.ou", "halfou", "half ou", ".tb.", "topbottom"):
        return StereoPacking.HALF_OU, reverse_eye
    if has_any(name, "mvc 3d", "mvc3d"):
        return StereoPacking.MVC, reverse_eye
    if has_any(name, ".sbs.", " sbs ", "_sbs_", "-sbs-", "sbs3d"):
        return StereoPacking.HALF_SBS, reverse_eye
    if has_any(name, ".ou.", " ou ", "_ou_", "-ou-"):
        return StereoPacking.HALF_OU, reverse_eye
    if "block_rl" in stereo_mode:
        reverse_eye = True
        return StereoPacking.HALF_SBS, reverse_eye
    if "block_lr" in stereo_mode:
        return StereoPacking.HALF_SBS, reverse_eye
    if "top_bottom" in stereo_mode or "top-bottom" in stereo_mode:
        return StereoPacking.HALF_OU, reverse_eye
    if has_any(name, ".3d.", "3d180", "3d360", "3dfisheye", ".full3d."):
        return StereoPacking.UNKNOWN_STEREO, reverse_eye
    return StereoPacking.MONO_2D, reverse_eye


def render_geometry(projection: Projection, curved_screen: bool = False) -> RenderGeometry:
    if projection == Projection.FLAT:
        return RenderGeometry.CURVED_SCREEN if curved_screen else RenderGeometry.FLAT_QUAD
    if projection == Projection.EQUIRECT_180:
        return RenderGeometry.EQUIRECT_HALF_DOME
    if projection in (Projection.EQUIRECT_360, Projection.YOUTUBE_360):
        return RenderGeometry.EQUIRECT_SPHERE
    if projection in (
        Projection.FISHEYE_180,
        Projection.FISHEYE_190,
        Projection.FISHEYE_200,
        Projection.FISHEYE_220,
    ):
        return RenderGeometry.FISHEYE_DOME
    return RenderGeometry.CUBE


def eye_uv(
    packing: StereoPacking,
    reverse_eye: bool,
    force_2d: bool = False,
) -> tuple[UvRect, UvRect]:
    whole = UvRect(0.0, 0.0, 1.0, 1.0)
    if packing in (StereoPacking.HALF_SBS, StereoPacking.FULL_SBS):
        left, right = UvRect(0.0, 0.0, 0.5, 1.0), UvRect(0.5, 0.0, 1.0, 1.0)
    elif packing in (StereoPacking.HALF_OU, StereoPacking.FULL_OU):
        left, right = UvRect(0.0, 0.0, 1.0, 0.5), UvRect(0.0, 0.5, 1.0, 1.0)
    else:
        left, right = whole, whole
    if reverse_eye:
        left, right = right, left
    if force_2d:
        right = left
    return left, right


def fisheye_fov(projection: Projection) -> float | None:
    values = {
        Projection.FISHEYE_180: 180.0,
        Projection.FISHEYE_190: 190.0,
        Projection.FISHEYE_200: 200.0,
        Projection.FISHEYE_220: 220.0,
    }
    return values.get(projection)


def classify(name: str, stereo_mode: str = "") -> LayoutDecision:
    normalized = name.lower()
    projection = detect_projection(normalized)
    packing, reverse_eye = detect_stereo_packing(normalized, stereo_mode.lower())
    left_uv, right_uv = eye_uv(packing, reverse_eye)
    return LayoutDecision(
        stereo_packing=packing,
        projection=projection,
        render_geometry=render_geometry(projection),
        left_uv=left_uv,
        right_uv=right_uv,
        requires_two_eye_decode=packing in (StereoPacking.MVC, StereoPacking.MV_HEVC),
        fisheye_fov_degrees=fisheye_fov(projection),
    )


def main(argv: list[str]) -> int:
    if len(argv) < 2:
        print("usage: probe_layout.py <media-name> [stereo_mode]", file=sys.stderr)
        return 2

    decision = classify(argv[1], argv[2] if len(argv) > 2 else "")
    print(json.dumps(asdict(decision), indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv))

# Pico HDR FFmpeg Lab

Reference reconstruction of the HDR playback mechanics from an owner-provided
Pico VR player build.

The repository does not contain APKs, native binaries, or packaged assets. It
documents and recreates the playback pipeline with FFmpeg, Android, OpenXR, and
GLSL interfaces:

- FFmpeg/libavformat/libavcodec/libavutil metadata extraction.
- Android `MediaFormat` HDR keys for `MediaCodec`.
- 10-bit `COLOR_FormatYUVP010` hardware decode preference.
- OpenXR `XR_FB_color_space` runtime color-space selection.
- GLSL ST2084/PQ EOTF, Hable tone mapping, saturation recovery, and panel balance.
- Dolby Vision handling as RPU metadata plumbing into 2D/3D mapping textures.

## What Was Extracted

The local player build uses these recognizable mechanics:

- Java-side HDR decisions around `Is_HDR_Bt2020Fix`, `Is_HDR_Bt2020_Base`, `Is_HDR_HLG`.
- `MediaFormat` keys: `color-standard`, `color-transfer`, `color-range`, `hdr-static-info`.
- `MediaCodecInfo.CodecCapabilities` scan for `COLOR_FormatYUVP010` (`0x36`).
- FFmpeg symbols for mastering display metadata, content light metadata, dynamic HDR10+, DOVI config and Dolby profile.
- OpenXR symbols `xrEnumerateColorSpacesFB` / `xrSetColorSpaceFB`.
- Shader strings with ST2084 constants, `vColorMatHDR`, `vColorMatDolby`, Dolby LUT samplers and tone mapping.

## Layout

- `docs/mechanics.md` - architecture notes and decision rules.
- `docs/color-pipeline.md` - how the color look is selected and built.
- `docs/dolby-mapping.md` - Dolby RPU to mapping-texture notes.
- `docs/ffmpeg-to-android.md` - FFmpeg enum to Android HDR key mapping.
- `tools/probe_hdr.py` - a runnable ffprobe JSON HDR classifier.
- `samples/` - small ffprobe JSON examples.
- `android/` - Kotlin snippets for Android `MediaFormat` HDR setup.
- `cpp/` - FFmpeg/OpenXR reference snippets and Dolby mapping formulas.
- `shaders/` - GLSL HDR mapping code.

## Quick Check

```bash
python tools/probe_hdr.py samples/hdr10_ffprobe.json
python tools/probe_hdr.py samples/hlg_ffprobe.json
python tools/probe_hdr.py samples/sdr_ffprobe.json
```

## Intended Use

Use this as a blueprint for a Pico/Android VR player:

1. Extract stream color metadata and HDR side data with FFmpeg.
2. Preserve the metadata when creating `MediaFormat`.
3. Prefer 10-bit output when hardware supports P010.
4. Select an OpenXR color space that matches the runtime capabilities.
5. Render with YUV/P010 or FFmpeg-decoded 10-bit planes.
6. Choose the color look from the metadata and device capabilities.
7. Apply ST2084/HLG handling and tone mapping in shader only when the runtime path does not already present HDR correctly.

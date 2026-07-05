# Pico HDR FFmpeg Lab

Clean-room reconstruction of the HDR playback mechanics observed in a Pico VR player stack.

The repository does not contain proprietary 4XVR code, assets, or binaries. It recreates the same kind of pipeline with open interfaces:

- FFmpeg/libavformat/libavcodec/libavutil metadata extraction.
- Android `MediaFormat` HDR keys for `MediaCodec`.
- 10-bit `COLOR_FormatYUVP010` hardware decode preference.
- OpenXR `XR_FB_color_space` runtime color-space selection.
- GLSL ST2084/PQ EOTF and simple tone mapping.
- Dolby Vision handling as open metadata plumbing, with the proprietary mapping logic intentionally replaced by documented extension points.

## What Was Observed

The inspected player uses these recognizable mechanics:

- Java-side HDR decisions around `Is_HDR_Bt2020Fix`, `Is_HDR_Bt2020_Base`, `Is_HDR_HLG`.
- `MediaFormat` keys: `color-standard`, `color-transfer`, `color-range`, `hdr-static-info`.
- `MediaCodecInfo.CodecCapabilities` scan for `COLOR_FormatYUVP010` (`0x36`).
- FFmpeg symbols for mastering display metadata, content light metadata, dynamic HDR10+, DOVI config and Dolby profile.
- OpenXR symbols `xrEnumerateColorSpacesFB` / `xrSetColorSpaceFB`.
- Shader strings with ST2084 constants, `vColorMatHDR`, `vColorMatDolby`, Dolby LUT samplers and tone mapping.

## Layout

- `docs/mechanics.md` - architecture notes and decision rules.
- `docs/ffmpeg-to-android.md` - FFmpeg enum to Android HDR key mapping.
- `tools/probe_hdr.py` - a runnable ffprobe JSON HDR classifier.
- `samples/` - small ffprobe JSON examples.
- `android/` - Kotlin snippets for Android `MediaFormat` HDR setup.
- `cpp/` - FFmpeg/OpenXR reference snippets.
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
6. Apply ST2084/HLG handling and tone mapping in shader only when the runtime path does not already present HDR correctly.

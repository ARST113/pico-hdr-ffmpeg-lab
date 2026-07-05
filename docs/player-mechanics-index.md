# Player Mechanics Index

This is the high-level map of the recreated player stack. The details live in
the focused documents and reference snippets.

## Pipeline

```text
open media
-> analyze stream and filename tags
-> choose stereo packing and projection
-> configure decoder and two-eye path
-> extract HDR/Dolby metadata
-> choose color and tone-map mode
-> build geometry and per-eye UVs
-> bind video, Dolby, alpha, and subtitle textures
-> submit OpenXR layers
```

## Input And Container

The local build handles ordinary files, network paths, M3U8, Blu-ray/DVD style
inputs, and app-specific protocols. The important playback hooks are:

- FFmpeg open/read/seek wrappers for custom IO;
- `.m3u8` and MPEG-TS handling;
- `.ssif`, `.ssif.smap`, and `BDMV/STREAM/SSIF/` for Blu-ray 3D;
- codec setup for H.264, HEVC, VP8, VP9, AV1;
- bitstream filters `h264_mp4toannexb` and `hevc_mp4toannexb`.

## 3D And Projection

See `docs/video-layouts-and-geometry.md`.

Covered mechanics:

- half/full SBS;
- half/full OU/TB;
- MVC and SSIF;
- MV-HEVC two-eye decode;
- reverse-eye and force-2D controls;
- flat, curved, 180, 360, YouTube360, fisheye, cube, and theater geometry;
- OpenXR video layer placement.

## HDR And Color

See `docs/color-pipeline.md` and `docs/ffmpeg-to-android.md`.

Covered mechanics:

- FFmpeg color metadata extraction;
- Android `MediaFormat` HDR keys;
- P010/10-bit preference;
- OpenXR color-space selection;
- ST2084/PQ and HLG shader handling;
- panel-tuned tone-map and saturation constants.

## Dolby

See `docs/dolby-mapping.md`.

Covered mechanics:

- RPU side-data bridge;
- polynomial 2D reshape LUTs;
- MMR 3D reshape LUTs;
- I/Ct/Cp texture selection;
- shader order around Dolby mapping and HDR tone mapping.

## Passthrough And Alpha

See `docs/video-layouts-and-geometry.md`.

Covered mechanics:

- alpha passthrough from HSV/RGB/range masks;
- fisheye-specific HSV computation;
- passthrough alpha targets;
- shader uniforms `vPassthroughHsv`, `vPassthroughRgb`, `vPassthroughRange`.

## Subtitles, UI, And Theater

The render stack keeps video separate from the rest of the scene:

- subtitle layer data is built and copied with its own pose/extent;
- UI can be rendered as flat or curved panels;
- theater models and cube textures are loaded independently;
- all visible elements are gathered into an `XrCompositionLayerBaseHeader` list.

## Audio

The local build exposes three broad audio mechanics:

- FFmpeg audio decode and track switching;
- AAudio output for low-latency playback;
- SoundTouch tempo/pitch control and OVR spatial audio hooks.

This repository currently documents the playback stack around video, HDR, 3D,
and OpenXR. Audio can be expanded later as a separate reference module without
changing the video/color decisions.

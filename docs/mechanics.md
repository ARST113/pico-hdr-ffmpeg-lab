# HDR Mechanics

This document describes the HDR pipeline reconstructed from the local player
build, public API usage, and observable symbols.

## Detection Rules

Android constants used by the recreated logic:

- `MediaFormat.COLOR_STANDARD_BT2020 == 6`
- `MediaFormat.COLOR_TRANSFER_ST2084 == 6`
- `MediaFormat.COLOR_TRANSFER_HLG == 7`
- `MediaCodecInfo.CodecCapabilities.COLOR_FormatYUVP010 == 0x36`

The practical rules are:

- HDR10/PQ: `color-standard = BT2020` and `color-transfer = ST2084`.
- HLG: `color-standard = BT2020` and `color-transfer = HLG`.
- Dolby Vision: treat as HDR when FFmpeg reports a DOVI configuration/profile. Prefer a Dolby-specific metadata path if available.
- Manual force HDR: allow the user or caller to force HDR classification for malformed streams.

## FFmpeg Extraction

Use open FFmpeg structures:

- `AVCodecParameters.color_primaries`
- `AVCodecParameters.color_trc`
- `AVCodecParameters.color_space`
- `AVCodecParameters.color_range`
- `AV_FRAME_DATA_MASTERING_DISPLAY_METADATA`
- `AV_FRAME_DATA_CONTENT_LIGHT_LEVEL`
- DOVI configuration side data when present
- HDR10+ dynamic metadata when present

The player behavior is to map this metadata into Android `MediaFormat` before
configuring `MediaCodec`.

## Android MediaCodec

The hardware path should:

1. Build `MediaFormat.createVideoFormat(mime, width, height)`.
2. Copy codec-specific data (`csd-0`, `csd-1`, etc.).
3. Set HDR keys:
   - `MediaFormat.KEY_COLOR_STANDARD`
   - `MediaFormat.KEY_COLOR_TRANSFER`
   - `MediaFormat.KEY_COLOR_RANGE`
   - `hdr-static-info` as a `ByteBuffer` when CTA-861.3 HDR static info is available.
4. Query decoder color formats and prefer `COLOR_FormatYUVP010` for 10-bit HDR.
5. Configure `MediaCodec` with the HDR-aware format.

The local build also keeps a fallback FFmpeg path for 10/12-bit YUV conversion
and texture upload when hardware output is unsuitable.

## Rendering

The rendering path has three layers:

- OES/RGB `MediaCodec` output for ordinary SDR and some hardware paths.
- YUV/P010 output for HDR where 10-bit precision matters.
- FFmpeg-decoded 10/12-bit YUV planes uploaded to textures for fallback and Dolby paths.

When the runtime cannot present HDR natively, shader tone mapping is used:

- Decode ST2084/PQ to linear light.
- Apply a color matrix from BT.2020 into the renderer working space.
- Apply a brightness/tone-map curve.
- Optionally use Dolby mapping LUTs for DOVI-derived per-frame metadata.

The visible "good color" is mostly decided here, not at the metadata flag level. See
`docs/color-pipeline.md` for the color-mode selection policy and shader-side look
controls.

## OpenXR Color Space

Use `XR_FB_color_space`:

1. Check whether the extension is enabled.
2. Enumerate runtime-supported color spaces with `xrEnumerateColorSpacesFB`.
3. Choose the best supported target for the headset and content.
4. Call `xrSetColorSpaceFB(session, chosenColorSpace)`.

This repository intentionally keeps enum selection policy separate from decoding so the app can tune it per runtime.

## Dolby Vision

The local build has DOVI frame side-data handling and Dolby mapping textures. In
this repository that is represented as an extension point:

- Extract DOVI profile/configuration through FFmpeg.
- Classify DOVI as HDR.
- Provide per-frame metadata hooks.
- Do not ship proprietary mapping tables or copied implementation details.

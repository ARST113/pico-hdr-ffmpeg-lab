# Color Pipeline And Look Selection

The pleasant HDR look is not a single flag. It comes from choosing the right
color mode, preserving enough precision, then doing the display conversion in
the right order.

The local player build has this broad shape:

- classify BT.2020 + ST2084 as HDR10/PQ;
- classify BT.2020 + HLG as HLG;
- classify Dolby Vision profiles as a Dolby path;
- prefer 10-bit/P010 or FFmpeg 10/12-bit upload for HDR;
- use HDR color matrices, ST2084/HLG decode, tone mapping, brightness controls,
  and optional Dolby mapping textures.

This repository now keeps the concrete color constants from the local player
build. They are not generic textbook defaults; they are the actual look choices
that make the picture punchier in-headset.

## Decision Order

The runtime should choose the color path before it creates the renderer:

```text
if user_force_hdr:
    mode = FORCED_HDR
else if ffmpeg_reports_dolby_vision_profile:
    mode = DOLBY_VISION
else if color_standard == BT2020 and color_transfer == ST2084:
    mode = HDR10_PQ_BT2020
else if color_standard == BT2020 and color_transfer == HLG:
    mode = HLG_BT2020
else:
    mode = SDR_BT709

if mode is SDR:
    render_path = SDR
else if native_hdr_presentation_is_known_good:
    render_path = NATIVE_ANDROID_HDR
else if decoder_supports_p010:
    render_path = HARDWARE_P010_SHADER
else:
    render_path = FFMPEG_10BIT_SHADER
```

The important part is that detection and rendering are separate decisions. A
stream can be HDR, but the headset/runtime may still need a shader tone-map path
instead of native HDR presentation.

## Modes

| Mode | Trigger | Main color operation |
| --- | --- | --- |
| `SDR_BT709` | no HDR transfer/profile | BT.709 range/matrix, no HDR tone map |
| `HDR10_PQ_BT2020` | BT.2020 + ST2084/PQ | PQ EOTF, Hable shoulder, saturation recovery, HDR matrix |
| `HLG_BT2020` | BT.2020 + HLG | HLG inverse curve, gamma look, smoothstep-style cubic blend |
| `DOLBY_VISION` | DOVI profile/side data | RPU reshaping texture, Dolby matrix, PQ tone map |
| `FORCED_HDR` | manual override | same as HDR10/PQ unless the caller overrides it |

## Exact Look Constants

The shader layer uses these concrete decisions:

- PQ clamp range: `0.0..1.2` before ST2084 EOTF.
- ST2084 constants:
  - `M1 = 1 / 0.1593017578125`
  - `M2 = 1 / 78.84375`
  - `C1 = 0.8359375`
  - `C2 = 18.8515625`
  - `C3 = 18.6875`
- Hable tone-map constants:
  - `A = 0.15`
  - `B = 0.50`
  - `C = 0.10`
  - `D = 0.20`
  - `E = 0.02`
  - `F = 0.30`
- Tone-map gain: `vec3(100.0, 93.0, 100.0) * 0.70`.
- Strong HLG-like saturation:
  - `fcolorfix = 3.0`
  - RGB pre-gain `(1.036, 1.000, 1.036) * fcolorfix`
  - color recovery `0.6`
  - output gamma `0.85`
- Soft HLG-like saturation:
  - `fcolorfix = 1.11`
  - color recovery `0.3`
  - output gamma `0.45`
- Smooth blend:
  - `-2.0 * smooth`, `3.0 * smooth`, `1.0 - smooth`
  - applied as `x^3*a + x^2*b + x*c`.

The `100/93/100` channel gain is one of the big visible tricks. Green is trimmed
slightly against red/blue after the HDR shoulder, which makes bright cyan/white
areas less green and gives skin/water/sky a more pleasing VR balance.

## Why The Color Looks Better

Good HDR color usually comes from five small choices working together:

1. Preserve 10-bit data. If HDR is decoded into 8-bit SDR textures too early,
   gradients and highlight color are already damaged.
2. Convert gamut before the tone map. BT.2020 material should be moved into the
   renderer/display working space with a matrix, not guessed by eye.
3. Decode the transfer function before compressing brightness. PQ values are not
   linear light; tone mapping PQ directly gives dull or strange color.
4. Use a shoulder based on max channel or luminance. This keeps bright clouds,
   water, UI glows, and specular highlights from clipping flat white.
5. Rebalance saturation and panel response after compression. A small saturation
   recovery and per-channel trim can make the result look much closer to a
   tuned VR player than a raw textbook conversion.

The strongest practical detail is ordering:

```text
texture sample
-> optional Dolby 2D/3D reshape texture
-> vColorMat / vColorMatDolby / vColorMatHDR
-> PQ or HLG inverse transfer
-> Hable shoulder
-> gamma/power control
-> saturation recovery
-> cubic smoothing
-> final clamp
```

If the same operations are applied in a different order, the picture becomes
either washed out or overcooked very quickly.

## Dolby Vision Path

The local build does not appear to use a single fixed Dolby color table. The
binary exposes a 6 MiB `g_fMappingTexBuffer`, plus:

- `FFmpegMedia::CheckDolby`;
- `FFmpegMedia::GetDoviFrameSide`;
- `BuildMMR`;
- `GetImageDataPolynomial`;
- `GetImageDataMMR`;
- `bsDolbyMappingTex::UpdateTexFromMapping`;
- `sTextureDolbyI2D/Ct2D/Cp2D`;
- `sTextureDolbyI3D/Ct3D/Cp3D`.

So the table is generated from RPU reshaping metadata and uploaded into mapping
textures. See `docs/dolby-mapping.md`.

## Tuning Knobs

The shader path exposes controls that correspond to the visible "look":

- `vPowerValue.x`: saturation recovery strength for `rgb2sat`;
- `vPowerValue.y`: output gamma/power after tone map;
- `vPowerValue.z`: cubic smoothing strength;
- `vColorMat`: base YUV/RGB conversion matrix;
- `vColorMatInverse`: inverse matrix used before Dolby mapping;
- `vColorMatDolby`: Dolby post-EOTF matrix;
- `vColorMatHDR`: final HDR/display matrix.

Start conservative:

```text
vPowerValue.x = 0.3..0.6
vPowerValue.y = 0.45..0.85
vPowerValue.z = 0.19..0.52
```

Then tune on the headset with real HDR scenes: bright sky, skin, water, neon,
and dark gradients.

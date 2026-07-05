# Video Layouts, 3D Packing, And Geometry

This document describes the non-color side of the player pipeline: how the app
decides what kind of video it has, how it maps left/right eye images, and how it
builds the VR surface that receives the decoded texture.

The local player build exposes the following useful anchors:

- file/video analysis: `AnalysisVideoType`, `GetVideoTypeAndMapType`;
- playback state: `SetVideoAndMapType`, `GetVideoType`, `GetVideoMapType`;
- stereo controls: `Set3DReverseEye`, `Get3DReverseEye`,
  `SetForce3DVideoShow2D`, `GetForce3DVideoShow2D`;
- surface controls: `SetMovieSize`, `SetScreenSize`, `SetVRCurvedScreen`,
  `GetVRCurvedScreen`, `SetCur180IPD`, `GetCur180IPD`;
- geometry builders: `BuildLocalVertexBuf`, `BuildFishEyeVertexBuf`,
  `BuildCubePosition`, `BuildCubeUVCor`, `BuildLocalCubeVertexBuf`,
  `BuildLocal3DCubeVertexBuf`;
- layer placement: `BuildVideoLayer`, `FillVideoPos`, `FillCylinderPos`,
  `FillVideo360Pos`, `FillVideoCubePos`, `FixInPanoramaVideo`;
- special decode paths: `Is3DMovie`, `Is3DSsifVideo`, `Is3DMvHEVCMp4`,
  `CheckHaveMvc`, `CheckHaveMvHevc`, `SetMvHevcDecodeData`,
  `NeedWaitingMvHevc`, `IsTwoEyeHaveData`;
- passthrough/alpha: `nativeForceToAlphaPassthrough`,
  `nativeForceToFilterPassthrough`, `GetPassthroughRange`,
  `ComputeHSVForFisheyeVideo`.

## Two Separate Decisions

The player separates two questions:

1. `videoType`: where the left/right eye images come from.
2. `mapType`: what shape the texture is drawn onto.

That separation is important. A `Full SBS 3D` movie can be flat cinema, 180
equirect, 360 equirect, or fisheye. The stereo packing decides UV cropping; the
projection decides the mesh.

```text
input url / file / stream metadata
-> detect stereo packing
-> detect projection/map type
-> choose per-eye UV rects
-> choose render geometry
-> choose HDR/color path
-> build or update OpenXR composition layers
```

## Recognized Video Labels

The local build contains these user-visible type strings:

| Label | Stereo packing | Projection |
| --- | --- | --- |
| `2D` | mono | flat |
| `Half SBS 3D` | side by side, squeezed | flat unless map tag says otherwise |
| `Half OU 3D` | over under, squeezed | flat unless map tag says otherwise |
| `MVC 3D` | two decoded views | flat unless map tag says otherwise |
| `Full SBS 3D` | side by side, full per-eye width | flat unless map tag says otherwise |
| `Full OU 3D` | over under, full per-eye height | flat unless map tag says otherwise |
| `MV-HEVC` | two decoded HEVC views | flat unless map tag says otherwise |
| `2D 180` / `3D 180` | mono or stereo | 180 equirect dome |
| `2D 360` / `3D 360` | mono or stereo | 360 equirect sphere |
| `2D Youtube360` / `3D Youtube360` | mono or stereo | YouTube-style 360 sphere |
| `2D Fisheye180` / `3D Fisheye180` | mono or stereo | 180 fisheye dome |
| `2D Fisheye190` / `3D Fisheye190` | mono or stereo | 190 fisheye dome |
| `2D Fisheye200` / `3D Fisheye200` | mono or stereo | 200 fisheye dome |
| `2D Fisheye220` / `3D Fisheye220` | mono or stereo | 220 fisheye dome |

## Filename And Metadata Tags

The analysis path recognizes filename tokens around the media node:

| Token | Meaning |
| --- | --- |
| `.vr.` | VR/panorama hint |
| `.360.` | 360 equirect hint |
| `.180.` | 180 equirect hint |
| `.180x180.` | fisheye/dual-180 style hint |
| `.hsbs.`, `half.sbs`, `halfsbs` | half side-by-side |
| `.fsbs.`, `full.sbs`, `fullsbs` | full side-by-side |
| `.sbs.`, `sbs` | generic side-by-side, usually half unless overridden |
| `.hou.`, `half.ou`, `halfou`, `.tb.` | half over-under/top-bottom |
| `.fou.`, `full.ou`, `fullou` | full over-under |
| `left`, `right`, `.lr.` | left/right pair or eye-order hint |
| `fisheye190`, `fisheye200`, `fisheye220` | fisheye FOV override |
| `.2d.` | force mono classification |
| `.3d.`, `.full3d.` | force stereo classification if no better packing exists |

The FFmpeg/container side also contributes:

- `stereo_mode`, including `block_rl`, for Matroska 3D metadata;
- `.ssif`, `.ssif.smap`, and `BDMV/STREAM/SSIF/` for Blu-ray MVC;
- MV-HEVC detection through `CheckHaveMvHevc` and `SetMvHevcDecodeData`;
- `rotate` and black-border measurements for final presentation correction.

## Stereo UV Rules

All packed stereo paths reduce to per-eye UV rectangles.

```text
mono:
    left  = [0, 0] -> [1, 1]
    right = [0, 0] -> [1, 1]

SBS:
    left  = [0,   0] -> [0.5, 1]
    right = [0.5, 0] -> [1,   1]

OU / TB:
    left  = [0, 0]   -> [1, 0.5]
    right = [0, 0.5] -> [1, 1]
```

`reverseEye` swaps left and right. `force3DVideoShow2D` uses the selected left
eye for both eyes. MVC and MV-HEVC normally decode two separate eye images, so
their UV rect is full-frame for each eye after the two-eye decode stage.

The visible aspect ratio differs between half and full packed sources:

```text
mono aspect      = width / height
half SBS aspect  = width / height
full SBS aspect  = (width / 2) / height
half OU aspect   = width / height
full OU aspect   = width / (height / 2)
MVC/MV-HEVC      = decoded_eye_width / decoded_eye_height
```

The "half" formats are squeezed inside a normal-size frame, so the renderer
stretches each cropped eye back to the normal movie aspect.

## Flat And Curved Screen

The flat screen path is a tessellated quad:

```text
x = (u - 0.5) * screen_width
y = (0.5 - v) * screen_height
z = -screen_distance
uv = eye_uv_rect(u, v)
```

When `VRCurvedScreen` is enabled, the same grid is bent onto a cylinder:

```text
theta = lerp(-horizontal_angle / 2, horizontal_angle / 2, u)
x = radius * sin(theta)
z = -radius * cos(theta)
y = (0.5 - v) * screen_height
uv = eye_uv_rect(u, v)
```

This preserves the same texture mapping while making the screen wrap around the
viewer. The local symbols `FillVideoPos`, `FillCylinderPos`, `GetScreenInfo`,
and `GetScreenCameraAngle` belong to this placement path.

## Equirect 180 And 360

Equirectangular video is rendered on the inside of a sphere or half-sphere.

For 360:

```text
yaw   = lerp(-pi, pi, u)
pitch = lerp(pi / 2, -pi / 2, v)
x = cos(pitch) * sin(yaw)
y = sin(pitch)
z = -cos(pitch) * cos(yaw)
uv = eye_uv_rect(u, v)
```

For 180:

```text
yaw   = lerp(-pi / 2, pi / 2, u)
pitch = lerp(pi / 2, -pi / 2, v)
x = cos(pitch) * sin(yaw)
y = sin(pitch)
z = -cos(pitch) * cos(yaw)
```

The local player has 180-specific placement controls:

- `Get180Radius`;
- `Get180EquirectCenter`;
- `RecenterScreenProp180`;
- `SetCur180IPD` / `GetCur180IPD`.

So 180 is not just "half of 360". It gets its own radius, center and IPD tuning
to keep near-field stereo comfortable.

## Fisheye 180/190/200/220

The fisheye path uses a radial texture and a dome mesh. The FOV suffix changes
how far the dome reaches around the viewer.

For every mesh point:

```text
phi = angle around the disc
r = normalized disc radius, 0 at center and 1 at edge
theta = r * fisheye_fov / 2

x = sin(theta) * cos(phi)
y = sin(theta) * sin(phi)
z = -cos(theta)

tex_u = center_u + 0.5 * r * cos(phi)
tex_v = center_v + 0.5 * r * sin(phi)
uv = eye_uv_rect(tex_u, tex_v)
```

The local symbols `BuildFishEyeVertexBuf` and `ComputeHSVForFisheyeVideo`
indicate that the fisheye path has both geometry and color/passthrough special
handling. Fisheye passthrough uses HSV/RGB/range uniforms to produce alpha masks
from video content.

## Cube Video And Theater Geometry

The cube path is separate from equirect/fisheye:

- `BuildCubePosition` creates cube face positions;
- `BuildCubeUVCor` assigns UV corners per face;
- `BuildLocalCubeVertexBuf` builds mono cube vertices;
- `BuildLocal3DCubeVertexBuf` builds stereo cube vertices;
- `RenderCubeMovie` and `FillVideoCubePos` place the cube layer.

The model/theater path is also separate:

- `LoadNewTheatres`, `RecoveryTheatres`, `BuildTheatreCubeTexture`;
- `GetTheatresComLayer`, `GetTheatresModelComLayer`;
- `CreateTheatresSwapchain`, `DestoryTheatresSwapchain`.

That means the movie can be either a normal video layer in front of the user, a
panorama layer around the user, or part of a rendered theater environment.

## OpenXR Layer Flow

The renderer builds a layer list each frame:

```text
video swapchain
-> BuildVideoLayer
-> fill pose/extent/quaternion for chosen projection
-> optional subtitles quad
-> optional UI panels/curved UI
-> optional theater/model layers
-> submit XrCompositionLayerBaseHeader list
```

Flat video and subtitles can be copied as `XrCompositionLayerQuad`. Panorama and
cube video use mesh/projection rendering into the swapchain, then submit the
resulting composition layer. The local code keeps helper functions for copying a
layer with a specific `XrEyeVisibility`, which is how stereo layers avoid
showing the wrong eye to the wrong view.

## Passthrough And Alpha

The passthrough path has two modes:

- alpha passthrough: detect a color/range and turn it into an alpha mask;
- filter passthrough: detect a color/range and filter or replace it.

The shader strings contain:

```glsl
uniform vec4 vPassthroughHsv;
uniform vec4 vPassthroughRgb;
uniform vec4 vPassthroughRange;
```

The CPU side computes ranges with `GetPassthroughRange` and, for fisheye video,
`ComputeHSVForFisheyeVideo`. The render path then samples the normal video
texture, applies the color matrix, computes alpha, and blends with the
passthrough/composition target.

## Porting Order

Implement the mechanics in this order:

1. Parse filename/container tags into `StereoPacking` and `Projection`.
2. Decode MVC/MV-HEVC into two-eye buffers before UV mapping.
3. Create per-eye UV rects and apply `reverseEye` / `force2D`.
4. Compute the presentation aspect from half/full packing.
5. Build the mesh for flat, curved, equirect, fisheye, or cube projection.
6. Pick the HDR/color path from `docs/color-pipeline.md`.
7. Bind video textures, optional Dolby LUTs, optional alpha textures.
8. Submit OpenXR video, subtitle, UI, and theater layers.

Reference code:

- `android/VideoLayoutDecision.kt`;
- `cpp/vr_geometry_reference.h`;
- `tools/probe_layout.py`.

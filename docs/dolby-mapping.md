# Dolby Mapping Notes

This is the concrete Dolby path observed in the local player build.

## CPU Side

`libvr4p-movieplayer-lib.so` exposes the Dolby/RPU parser and side-data bridge:

- `FFmpegMedia::CheckDolby`
- `FFmpegMedia::CheckDolbyPak`
- `FFmpegMedia::GetDolbyProfile`
- `FFmpegMedia::GetDoviFrameSide`
- `FFmpegMedia::UsingDolbyFrameSideTime`
- `v4BitRead::get_ue_coef`
- `v4BitRead::get_se_coef`
- `BuildMMR`

The strings around the parser confirm the expected RPU validation fields:

- `coef_data_type`
- `coef_log2_denom`
- `bl_bit_depth_minus8`
- `el_bit_depth_minus8`
- `vdr_bit_depth_minus8`
- `ext_mapping_idc`
- `mapping_idc`
- `poly_order_minus1`
- `mmr_order_minus1`
- `affected_dm_id`
- `current_dm_id`
- `signal_bit_depth`

This lines up with FFmpeg's public `AVDOVIReshapingCurve` and
`AVDOVIDataMapping` structures: each component has piecewise pivots, a mapping
method, polynomial coefficients, and MMR coefficients.

## Texture Builder

`libvr4p-oculus.so` exposes the renderer-side builder:

- `g_fMappingTexBuffer`, 6 MiB data buffer;
- `GetImageDataPolynomial`;
- `GetImageDataMMR`;
- `bsDolbyMappingTex::UpdateTexFromMapping`;
- `glTexImage3D`;
- `glTexSubImage3D`.

That means the Dolby "table" is not just one static lookup table. The player
builds mapping textures from the current Dolby RPU mapping metadata, then binds
them as shader samplers.

## Shader Side

The shader has two mapping forms:

```glsl
uniform sampler2D sTextureDolbyI2D;
uniform sampler2D sTextureDolbyCt2D;
uniform sampler2D sTextureDolbyCp2D;
uniform sampler3D sTextureDolbyI3D;
uniform sampler3D sTextureDolbyCt3D;
uniform sampler3D sTextureDolbyCp3D;
```

2D mapping samples each component independently:

```glsl
float fDolbyI  = texture(sTextureDolbyI2D,  vec2(tempResult0.r, 0.0)).r;
float fDolbyCt = texture(sTextureDolbyCt2D, vec2(tempResult0.g, 0.0)).r;
float fDolbyCp = texture(sTextureDolbyCp2D, vec2(tempResult0.b, 0.0)).r;
```

3D mapping samples with the whole color vector:

```glsl
float fDolbyI  = texture(sTextureDolbyI3D,  tempResult0.rgb).r;
float fDolbyCt = texture(sTextureDolbyCt3D, tempResult0.rgb).r;
float fDolbyCp = texture(sTextureDolbyCp3D, tempResult0.rgb).r;
```

Then it writes the mapped channels back:

```glsl
tempResult0.r = fDolbyI;
tempResult0.g = fDolbyCt;
tempResult0.b = fDolbyCp;
```

The Dolby RGB/OES path first applies `vColorMatInverse`, performs Dolby mapping,
then returns through `vColorMat`:

```text
sample texture
-> vColorMatInverse
-> Dolby 2D/3D mapping
-> vColorMat
-> HDR/PQ look block
```

After that, the HDR block applies `vColorMatDolby`, ST2084 EOTF, Hable tone map,
`vPowerValue` gamma/smoothing, `rgb2sat`, and `vColorMatHDR`.

## Practical Port

For an Android/OpenXR player:

1. Parse DOVI RPU through FFmpeg side data.
2. Keep one `AVDOVIDataMapping` per frame or metadata timestamp.
3. Generate 2D textures for independent component mapping.
4. Generate 3D textures when MMR/cross-component mapping is required.
5. Bind the I/Ct/Cp textures and choose the matching shader branch.
6. Apply the same HDR look block after mapping.

Reference for the public FFmpeg metadata structure:
`https://github.com/FFmpeg/FFmpeg/blob/master/libavutil/dovi_meta.h`.

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

## Exact Formula

The extracted `BuildMMR` function has this signature shape:

```cpp
double BuildMMR(
    double x,
    double y,
    double z,
    int order,
    double constant,
    double coeff[3][7]
);
```

It builds these seven basis values:

```text
basis = [x, y, z, x*y, x*z, y*z, x*y*z]
```

Then it sums the same basis in powers 1, 2, and 3:

```text
result = constant

for row in 0..order-1:
    power = row + 1
    for i in 0..6:
        result += coeff[row][i] * pow(basis[i], power)
```

Expanded:

```text
constant
+ c00*x      + c01*y      + c02*z      + c03*xy      + c04*xz      + c05*yz      + c06*xyz
+ c10*x^2    + c11*y^2    + c12*z^2    + c13*(xy)^2  + c14*(xz)^2  + c15*(yz)^2  + c16*(xyz)^2
+ c20*x^3    + c21*y^3    + c22*z^3    + c23*(xy)^3  + c24*(xz)^3  + c25*(yz)^3  + c26*(xyz)^3
```

The local `GetImageDataMMR` path emits a `16 x 16 x 16` float texture. The
coordinates are normalized as:

```text
x = xi / 15
y = yi / 15
z = zi / 15
```

The output order is `z -> y -> x`, so index calculation is:

```text
index = (z * 16 + y) * 16 + x
```

The coefficients are normalized by:

```text
coef = raw_coef / (1 << coef_log2_denom)
constant = raw_constant / (1 << coef_log2_denom)
```

The local generator reads `mmr_constant[0]` and `mmr_coef[0][0..2][0..6]`.
Unused coefficient rows are expected to be zeroed upstream.

## Polynomial Formula

The polynomial path emits a `1024 x 1` float texture.

The identity baseline is:

```text
out[i] = i / 1023
```

For each piece:

```text
lower_pivot = pivots[piece] / ((1 << bit_depth) - 1)
c0 = poly_coef[piece][0] / (1 << coef_log2_denom)
c1 = poly_coef[piece][1] / (1 << coef_log2_denom)
c2 = poly_order[piece] == 2
   ? poly_coef[piece][2] / (1 << coef_log2_denom)
   : 0
```

For sample `x = i / 1023`, the selected piece is the last piece whose
`lower_pivot <= x`. If no piece matches, the identity value is kept.

The value written to the LUT is:

```text
out[i] = c0 + x*c1 + x*x*c2
```

Important: the polynomial is evaluated in global normalized `x`, not in local
piece coordinates.

## Texture Selection

For each Dolby component (`I`, `Ct`, `Cp`) the local `UpdateTexFromMapping`
counts curve pieces:

```text
if num_pivots <= 1:
    use identity 2D ramp
else if exactly one piece has mapping_idc == MMR:
    use 16x16x16 MMR 3D texture
else if at least one piece has mapping_idc == POLYNOMIAL:
    use 1024x1 polynomial 2D texture
else:
    use identity 2D ramp
```

Texture upload details:

```text
2D: target GL_TEXTURE_2D, width 1024, height 1, format GL_RED, type GL_FLOAT
3D: target GL_TEXTURE_3D, size 16x16x16, format GL_RED, type GL_FLOAT
```

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

// HDR color-look pipeline matching the local Pico VR player build.
// The app selects HDR10/HLG/Dolby before choosing one of these mapping blocks.

const float ST2084_M1 = 1.0 / 0.1593017578125;
const float ST2084_M2 = 1.0 / 78.84375;
const float ST2084_C1 = 0.8359375;
const float ST2084_C2 = 18.8515625;
const float ST2084_C3 = 18.6875;

const vec4 V4_ZERO = vec4(0.0, 0.0, 0.0, 0.0);
const vec4 V4_ONE = vec4(1.0, 1.0, 1.0, 1.0);
const vec3 ST2084_CLAMP_MAX = vec3(1.2, 1.2, 1.2);

const float HABLE_A = 0.15;
const float HABLE_B = 0.50;
const float HABLE_C = 0.10;
const float HABLE_D = 0.20;
const float HABLE_E = 0.02;
const float HABLE_F = 0.30;

const vec3 V4_TONE_GAIN = vec3(100.0, 93.0, 100.0) * 0.70;

vec3 v4St2084Eotf(vec3 x) {
    x = clamp(x, vec3(0.0), ST2084_CLAMP_MAX);
    vec3 xpow = pow(x, vec3(ST2084_M2));
    vec3 numerator = max(xpow - ST2084_C1, vec3(0.0));
    vec3 denominator = ST2084_C2 - ST2084_C3 * xpow;
    return pow(numerator / denominator, vec3(ST2084_M1));
}

float v4Hable(float x) {
    return ((x * (HABLE_A * x + HABLE_C * HABLE_B) + HABLE_D * HABLE_E) /
            (x * (HABLE_A * x + HABLE_B) + HABLE_D * HABLE_F)) -
            HABLE_E / HABLE_F;
}

vec3 v4ToneMap(vec3 hdrLinear) {
    float lum = max(max(hdrLinear.r, hdrLinear.g), hdrLinear.b);
    float scale = v4Hable(lum) / max(lum, 1e-6);
    return hdrLinear * scale * V4_TONE_GAIN;
}

vec3 v4Rgb2Sat(vec3 c, float power) {
    float fmax = max(c.r, max(c.g, c.b));
    vec3 delta = max(c - fmax, vec3(-0.1, -0.1, -0.1));
    return max(c + delta * vec3(power), vec3(0.0));
}

vec3 v4Rgb2Sat2(vec3 c, float colorFix, float recovery) {
    vec3 fresult = c * vec3(1.036 * colorFix, 1.000 * colorFix, 1.036 * colorFix);
    float fmax = max(fresult.r, max(fresult.g, fresult.b));
    fresult += max(fresult - fmax, vec3(-0.1, -0.1, -0.1)) * vec3(recovery);
    return max(fresult, vec3(0.0));
}

vec4 v4SmoothColor(vec4 inColor, float smooth) {
    float t3 = -2.0 * smooth;
    float t2 = 3.0 * smooth;
    float t1 = 1.0 - smooth;
    vec4 d2 = inColor * inColor;
    vec4 d3 = d2 * inColor;
    return d3 * t3 + d2 * t2 + inColor * t1;
}

vec4 v4MapHdr10Pq(vec4 tempResult0, mat4 vColorMatHDR, vec4 vPowerValue) {
    vec3 linear = v4St2084Eotf(clamp(tempResult0, V4_ZERO, V4_ONE).rgb);
    vec3 tm = v4ToneMap(linear);
    vec4 rv = pow(vec4(tm, 1.0), vec4(vPowerValue.y, vPowerValue.y, vPowerValue.y, 1.0));
    vec4 inColor = clamp(vec4(v4Rgb2Sat(rv.rgb, vPowerValue.x), 1.0) * vColorMatHDR, V4_ZERO, V4_ONE);
    return v4SmoothColor(inColor, vPowerValue.z);
}

vec4 v4MapDolbyPq(vec4 tempResult0, mat3 vColorMatDolby, mat4 vColorMatHDR, vec4 vPowerValue, bool boost) {
    vec3 vEotf = v4St2084Eotf(clamp(tempResult0, V4_ZERO, V4_ONE).rgb);
    vEotf *= vColorMatDolby;

    vec3 tm = v4ToneMap(clamp(vEotf, V4_ZERO.rgb, V4_ONE.rgb));
    vec4 rv = pow(vec4(tm, 1.0), vec4(vPowerValue.y, vPowerValue.y, vPowerValue.y, 1.0));
    if (boost) {
        rv *= 4.0;
    }

    vec4 inColor = clamp(vec4(v4Rgb2Sat(rv.rgb, vPowerValue.x), 1.0) * vColorMatHDR, V4_ZERO, V4_ONE);
    return v4SmoothColor(inColor, vPowerValue.z);
}

vec4 v4MapHlgStrong(vec4 tempResult0, mat4 vColorMatHDR, float smooth) {
    vec4 inColorB = clamp(tempResult0, V4_ZERO, V4_ONE);
    vec4 colorD2 = inColorB * inColorB / 3.0;
    vec4 colorH = (0.28466892 + exp((inColorB - 0.55991073) / 0.17883277)) / 12.0;
    vec4 colorR = mix(colorD2, colorH, step(0.5, inColorB));
    vec4 rv = pow(colorR, vec4(0.85, 0.85, 0.85, 1.0));
    vec4 inColor = clamp(vec4(v4Rgb2Sat2(rv.rgb, 3.0, 0.6), 1.0) * vColorMatHDR, V4_ZERO, V4_ONE);
    return v4SmoothColor(inColor, smooth);
}

vec4 v4MapHlgSoft(vec4 tempResult0, mat4 vColorMatHDR, float smooth) {
    vec4 inColorB = clamp(tempResult0, V4_ZERO, V4_ONE);
    vec4 colorD2 = inColorB * inColorB / 3.0;
    vec4 colorH = (0.28466892 + exp((inColorB - 0.55991073) / 0.17883277)) / 12.0;
    vec4 colorR = mix(colorD2, colorH, step(0.5, inColorB));
    vec4 rv = pow(colorR, vec4(0.45, 0.45, 0.45, 1.0));
    vec4 inColor = clamp(vec4(v4Rgb2Sat2(rv.rgb, 1.11, 0.3), 1.0) * vColorMatHDR, V4_ZERO, V4_ONE);
    return v4SmoothColor(inColor, smooth);
}

vec4 v4ApplyDolby2D(
    vec4 tempResult0,
    sampler2D sTextureDolbyI2D,
    sampler2D sTextureDolbyCt2D,
    sampler2D sTextureDolbyCp2D
) {
    float fDolbyI = texture(sTextureDolbyI2D, vec2(tempResult0.r, 0.0)).r;
    float fDolbyCt = texture(sTextureDolbyCt2D, vec2(tempResult0.g, 0.0)).r;
    float fDolbyCp = texture(sTextureDolbyCp2D, vec2(tempResult0.b, 0.0)).r;
    return vec4(fDolbyI, fDolbyCt, fDolbyCp, tempResult0.a);
}

vec4 v4ApplyDolby3D(
    vec4 tempResult0,
    sampler3D sTextureDolbyI3D,
    sampler3D sTextureDolbyCt3D,
    sampler3D sTextureDolbyCp3D
) {
    float fDolbyI = texture(sTextureDolbyI3D, tempResult0.rgb).r;
    float fDolbyCt = texture(sTextureDolbyCt3D, tempResult0.rgb).r;
    float fDolbyCp = texture(sTextureDolbyCp3D, tempResult0.rgb).r;
    return vec4(fDolbyI, fDolbyCt, fDolbyCp, tempResult0.a);
}

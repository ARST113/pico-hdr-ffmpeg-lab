// ST2084/PQ EOTF and compact tone mapping for VR video.
// Inputs are expected to be normalized PQ RGB after YUV->RGB conversion.

const float ST2084_M1 = 1.0 / 0.1593017578125;
const float ST2084_M2 = 1.0 / 78.84375;
const float ST2084_C1 = 0.8359375;
const float ST2084_C2 = 18.8515625;
const float ST2084_C3 = 18.6875;
const vec3 VR_HDR_GAIN = vec3(100.0, 93.0, 100.0) * 0.70;

vec3 st2084Eotf(vec3 x) {
    x = clamp(x, vec3(0.0), vec3(1.2));
    vec3 xpow = pow(x, vec3(ST2084_M2));
    vec3 numerator = max(xpow - ST2084_C1, vec3(0.0));
    vec3 denominator = ST2084_C2 - ST2084_C3 * xpow;
    return pow(numerator / denominator, vec3(ST2084_M1));
}

float hable(float x) {
    const float A = 0.15;
    const float B = 0.50;
    const float C = 0.10;
    const float D = 0.20;
    const float E = 0.02;
    const float F = 0.30;
    return ((x * (A * x + C * B) + D * E) /
            (x * (A * x + B) + D * F)) - E / F;
}

vec3 simpleVrToneMap(vec3 hdrLinear, float brightness) {
    float lum = max(max(hdrLinear.r, hdrLinear.g), hdrLinear.b);
    float scale = hable(lum) / max(lum, 1e-6);
    return hdrLinear * scale * brightness * VR_HDR_GAIN;
}

vec3 mapHdrPqToDisplay(vec3 pqRgb, mat4 colorMatrixHdr, float brightness) {
    vec4 corrected = clamp(vec4(pqRgb, 1.0) * colorMatrixHdr, vec4(0.0), vec4(1.0));
    vec3 linear = st2084Eotf(corrected.rgb);
    return simpleVrToneMap(linear, brightness);
}

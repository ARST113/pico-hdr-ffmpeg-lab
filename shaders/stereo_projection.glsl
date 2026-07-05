// Reference helpers for packed stereo UVs.
// Keep the same UV policy on Android, C++, and GLSL sides.

const int STEREO_MONO_2D = 0;
const int STEREO_HALF_SBS = 1;
const int STEREO_HALF_OU = 2;
const int STEREO_FULL_SBS = 3;
const int STEREO_FULL_OU = 4;
const int STEREO_MVC = 5;
const int STEREO_MV_HEVC = 6;

vec2 mapStereoUv(vec2 uv, int stereoPacking, int eyeIndex, bool reverseEye, bool force2D) {
    int eye = force2D ? 0 : eyeIndex;
    if (reverseEye) {
        eye = 1 - eye;
    }

    if (stereoPacking == STEREO_HALF_SBS || stereoPacking == STEREO_FULL_SBS) {
        uv.x = uv.x * 0.5 + (eye == 0 ? 0.0 : 0.5);
    } else if (stereoPacking == STEREO_HALF_OU || stereoPacking == STEREO_FULL_OU) {
        uv.y = uv.y * 0.5 + (eye == 0 ? 0.0 : 0.5);
    }

    return uv;
}

float presentationAspect(float width, float height, int stereoPacking) {
    if (height <= 0.0) {
        return 16.0 / 9.0;
    }

    if (stereoPacking == STEREO_FULL_SBS) {
        return (width * 0.5) / height;
    }

    if (stereoPacking == STEREO_FULL_OU) {
        return width / (height * 0.5);
    }

    return width / height;
}

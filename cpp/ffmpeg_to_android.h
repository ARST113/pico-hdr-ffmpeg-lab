#pragma once

extern "C" {
#include <libavutil/pixfmt.h>
}

#include <cstdint>

namespace pico_hdr {

struct AndroidHdrKeys {
    int colorStandard = 0;
    int colorTransfer = 0;
    int colorRange = 0;
};

enum AndroidColorStandard {
    ANDROID_COLOR_STANDARD_UNSPECIFIED = 0,
    ANDROID_COLOR_STANDARD_BT709 = 1,
    ANDROID_COLOR_STANDARD_BT2020 = 6,
};

enum AndroidColorTransfer {
    ANDROID_COLOR_TRANSFER_UNSPECIFIED = 0,
    ANDROID_COLOR_TRANSFER_SDR_VIDEO = 3,
    ANDROID_COLOR_TRANSFER_ST2084 = 6,
    ANDROID_COLOR_TRANSFER_HLG = 7,
};

enum AndroidColorRange {
    ANDROID_COLOR_RANGE_UNSPECIFIED = 0,
    ANDROID_COLOR_RANGE_FULL = 1,
    ANDROID_COLOR_RANGE_LIMITED = 2,
};

inline int map_color_standard(AVColorPrimaries primaries) {
    switch (primaries) {
        case AVCOL_PRI_BT709:
            return ANDROID_COLOR_STANDARD_BT709;
        case AVCOL_PRI_BT2020:
            return ANDROID_COLOR_STANDARD_BT2020;
        default:
            return ANDROID_COLOR_STANDARD_UNSPECIFIED;
    }
}

inline int map_color_transfer(AVColorTransferCharacteristic transfer) {
    switch (transfer) {
        case AVCOL_TRC_SMPTE2084:
            return ANDROID_COLOR_TRANSFER_ST2084;
        case AVCOL_TRC_ARIB_STD_B67:
            return ANDROID_COLOR_TRANSFER_HLG;
        case AVCOL_TRC_BT709:
        case AVCOL_TRC_SMPTE170M:
            return ANDROID_COLOR_TRANSFER_SDR_VIDEO;
        default:
            return ANDROID_COLOR_TRANSFER_UNSPECIFIED;
    }
}

inline int map_color_range(AVColorRange range) {
    switch (range) {
        case AVCOL_RANGE_JPEG:
            return ANDROID_COLOR_RANGE_FULL;
        case AVCOL_RANGE_MPEG:
            return ANDROID_COLOR_RANGE_LIMITED;
        default:
            return ANDROID_COLOR_RANGE_UNSPECIFIED;
    }
}

inline AndroidHdrKeys map_to_android_hdr_keys(
    AVColorPrimaries primaries,
    AVColorTransferCharacteristic transfer,
    AVColorRange range
) {
    return AndroidHdrKeys{
        map_color_standard(primaries),
        map_color_transfer(transfer),
        map_color_range(range),
    };
}

} // namespace pico_hdr


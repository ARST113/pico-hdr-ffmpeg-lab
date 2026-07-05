// Reference-only FFmpeg HDR extraction.
// Link against libavformat, libavcodec and libavutil in a real app.

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/mastering_display_metadata.h>
#include <libavutil/pixdesc.h>
}

#include <cstdint>
#include <optional>

namespace pico_hdr {

struct HdrInfo {
    int colorPrimaries = AVCOL_PRI_UNSPECIFIED;
    int colorTransfer = AVCOL_TRC_UNSPECIFIED;
    int colorSpace = AVCOL_SPC_UNSPECIFIED;
    int colorRange = AVCOL_RANGE_UNSPECIFIED;
    int bitDepth = 8;
    bool hasMasteringDisplay = false;
    bool hasContentLight = false;
    bool hasDolbyVision = false;
};

static int bits_per_component(AVPixelFormat format) {
    const AVPixFmtDescriptor* desc = av_pix_fmt_desc_get(format);
    if (!desc || desc->nb_components <= 0) return 8;
    return desc->comp[0].depth;
}

static bool has_side_data(const AVStream* stream, AVPacketSideDataType type) {
    if (!stream || !stream->codecpar) return false;

    for (int i = 0; i < stream->codecpar->nb_coded_side_data; ++i) {
        if (stream->codecpar->coded_side_data[i].type == type) return true;
    }

    return false;
}

HdrInfo extract_hdr_info(const AVStream* stream, AVPixelFormat decodedFormat) {
    HdrInfo out;
    if (!stream || !stream->codecpar) return out;

    const AVCodecParameters* cp = stream->codecpar;
    out.colorPrimaries = cp->color_primaries;
    out.colorTransfer = cp->color_trc;
    out.colorSpace = cp->color_space;
    out.colorRange = cp->color_range;
    out.bitDepth = bits_per_component(decodedFormat);
    out.hasMasteringDisplay = has_side_data(stream, AV_PKT_DATA_MASTERING_DISPLAY_METADATA);
    out.hasContentLight = has_side_data(stream, AV_PKT_DATA_CONTENT_LIGHT_LEVEL);
    out.hasDolbyVision = has_side_data(stream, AV_PKT_DATA_DOVI_CONF);

    return out;
}

bool is_hdr10_pq(const HdrInfo& info) {
    return info.colorPrimaries == AVCOL_PRI_BT2020 &&
           info.colorTransfer == AVCOL_TRC_SMPTE2084;
}

bool is_hlg(const HdrInfo& info) {
    return info.colorPrimaries == AVCOL_PRI_BT2020 &&
           info.colorTransfer == AVCOL_TRC_ARIB_STD_B67;
}

bool is_hdr(const HdrInfo& info) {
    return is_hdr10_pq(info) || is_hlg(info) || info.hasDolbyVision;
}

} // namespace pico_hdr


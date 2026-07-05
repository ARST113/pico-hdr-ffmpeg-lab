package com.arst113.picohdr

import android.media.MediaCodecInfo
import android.media.MediaFormat
import java.nio.ByteBuffer

fun MediaFormat.applyHdrMetadata(metadata: HdrMetadata): MediaFormat {
    if (metadata.colorStandard != HdrMetadata.COLOR_STANDARD_UNSPECIFIED) {
        setInteger(MediaFormat.KEY_COLOR_STANDARD, metadata.colorStandard)
    }

    if (metadata.colorTransfer != HdrMetadata.COLOR_TRANSFER_UNSPECIFIED) {
        setInteger(MediaFormat.KEY_COLOR_TRANSFER, metadata.colorTransfer)
    }

    if (metadata.colorRange != HdrMetadata.COLOR_RANGE_UNSPECIFIED) {
        setInteger(MediaFormat.KEY_COLOR_RANGE, metadata.colorRange)
    }

    metadata.hdrStaticInfo?.let { staticInfo ->
        setByteBuffer("hdr-static-info", ByteBuffer.wrap(staticInfo))
    }

    return this
}

fun MediaCodecInfo.CodecCapabilities.supportsP010(): Boolean {
    return colorFormats.any { it == COLOR_FormatYUVP010 }
}

fun MediaCodecInfo.CodecCapabilities.bestHdrColorFormat(): Int? {
    return when {
        supportsP010() -> COLOR_FormatYUVP010
        colorFormats.any { it == MediaCodecInfo.CodecCapabilities.COLOR_FormatYUV420Flexible } ->
            MediaCodecInfo.CodecCapabilities.COLOR_FormatYUV420Flexible
        else -> null
    }
}

private const val COLOR_FormatYUVP010 = 0x36


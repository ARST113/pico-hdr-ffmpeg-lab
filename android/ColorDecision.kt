package com.arst113.picohdr

enum class ColorMode {
    SDR_BT709,
    HDR10_PQ_BT2020,
    HLG_BT2020,
    DOLBY_VISION,
    FORCED_HDR
}

enum class RenderPath {
    SDR,
    NATIVE_ANDROID_HDR,
    HARDWARE_P010_SHADER,
    FFMPEG_10BIT_SHADER
}

enum class ToneMapper {
    NONE,
    PQ_TO_DISPLAY,
    HLG_TO_DISPLAY,
    DOLBY_EXTENSION
}

enum class XrColorSpaceIntent {
    REC709,
    BT2020_PQ,
    BT2020_HLG,
    DISPLAY_P3
}

data class DeviceHdrCapabilities(
    val supportsP010: Boolean = false,
    val supportsNativeHdrPresentation: Boolean = false,
    val supportsXrColorSpaceSelection: Boolean = false,
    val preferNativeHdr: Boolean = true
)

data class ColorDecision(
    val mode: ColorMode,
    val renderPath: RenderPath,
    val toneMapper: ToneMapper,
    val xrColorSpaceIntent: XrColorSpaceIntent,
    val preserve10Bit: Boolean,
    val useDolbyMetadataHook: Boolean,
    val colorMatrixName: String
)

fun chooseColorDecision(
    metadata: HdrMetadata,
    capabilities: DeviceHdrCapabilities
): ColorDecision {
    val mode = when {
        metadata.forceHdr -> ColorMode.FORCED_HDR
        metadata.dolbyVisionProfile != null -> ColorMode.DOLBY_VISION
        metadata.isHdr10 -> ColorMode.HDR10_PQ_BT2020
        metadata.isBt2020 && metadata.isHlg -> ColorMode.HLG_BT2020
        else -> ColorMode.SDR_BT709
    }

    val isHdrMode = mode != ColorMode.SDR_BT709
    val canUseNativeHdr = isHdrMode &&
        capabilities.preferNativeHdr &&
        capabilities.supportsNativeHdrPresentation

    val renderPath = when {
        !isHdrMode -> RenderPath.SDR
        canUseNativeHdr -> RenderPath.NATIVE_ANDROID_HDR
        capabilities.supportsP010 -> RenderPath.HARDWARE_P010_SHADER
        else -> RenderPath.FFMPEG_10BIT_SHADER
    }

    val toneMapper = when {
        renderPath == RenderPath.NATIVE_ANDROID_HDR -> ToneMapper.NONE
        mode == ColorMode.DOLBY_VISION -> ToneMapper.DOLBY_EXTENSION
        mode == ColorMode.HLG_BT2020 -> ToneMapper.HLG_TO_DISPLAY
        mode == ColorMode.HDR10_PQ_BT2020 || mode == ColorMode.FORCED_HDR -> ToneMapper.PQ_TO_DISPLAY
        else -> ToneMapper.NONE
    }

    val xrColorSpaceIntent = when (mode) {
        ColorMode.SDR_BT709 -> XrColorSpaceIntent.REC709
        ColorMode.HLG_BT2020 -> XrColorSpaceIntent.BT2020_HLG
        ColorMode.HDR10_PQ_BT2020,
        ColorMode.DOLBY_VISION,
        ColorMode.FORCED_HDR -> XrColorSpaceIntent.BT2020_PQ
    }

    val matrixName = when (mode) {
        ColorMode.SDR_BT709 -> "BT709_TO_WORKING"
        ColorMode.DOLBY_VISION -> "DOVI_RESHAPE_PLUS_BT2020_TO_WORKING"
        else -> "BT2020_TO_WORKING"
    }

    return ColorDecision(
        mode = mode,
        renderPath = renderPath,
        toneMapper = toneMapper,
        xrColorSpaceIntent = xrColorSpaceIntent,
        preserve10Bit = isHdrMode && (metadata.bitDepth >= 10 || capabilities.supportsP010),
        useDolbyMetadataHook = mode == ColorMode.DOLBY_VISION,
        colorMatrixName = matrixName
    )
}

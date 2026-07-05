package com.arst113.picohdr

data class HdrMetadata(
    val colorStandard: Int = COLOR_STANDARD_UNSPECIFIED,
    val colorTransfer: Int = COLOR_TRANSFER_UNSPECIFIED,
    val colorRange: Int = COLOR_RANGE_UNSPECIFIED,
    val hdrStaticInfo: ByteArray? = null,
    val dolbyVisionProfile: Int? = null,
    val bitDepth: Int = 8,
    val forceHdr: Boolean = false
) {
    val isBt2020: Boolean
        get() = colorStandard == COLOR_STANDARD_BT2020

    val isPq: Boolean
        get() = colorTransfer == COLOR_TRANSFER_ST2084

    val isHlg: Boolean
        get() = colorTransfer == COLOR_TRANSFER_HLG

    val isHdr10: Boolean
        get() = isBt2020 && isPq

    val isHdr: Boolean
        get() = forceHdr || isHdr10 || (isBt2020 && isHlg) || dolbyVisionProfile != null

    companion object {
        const val COLOR_STANDARD_UNSPECIFIED = 0
        const val COLOR_STANDARD_BT709 = 1
        const val COLOR_STANDARD_BT2020 = 6

        const val COLOR_TRANSFER_UNSPECIFIED = 0
        const val COLOR_TRANSFER_SDR_VIDEO = 3
        const val COLOR_TRANSFER_ST2084 = 6
        const val COLOR_TRANSFER_HLG = 7

        const val COLOR_RANGE_UNSPECIFIED = 0
        const val COLOR_RANGE_FULL = 1
        const val COLOR_RANGE_LIMITED = 2
    }
}


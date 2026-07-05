package com.arst113.picohdr

enum class StereoPacking {
    MONO_2D,
    HALF_SBS,
    HALF_OU,
    FULL_SBS,
    FULL_OU,
    MVC,
    MV_HEVC,
    UNKNOWN_STEREO
}

enum class Projection {
    FLAT,
    EQUIRECT_180,
    EQUIRECT_360,
    YOUTUBE_360,
    FISHEYE_180,
    FISHEYE_190,
    FISHEYE_200,
    FISHEYE_220,
    CUBE
}

enum class RenderGeometry {
    FLAT_QUAD,
    CURVED_SCREEN,
    EQUIRECT_HALF_DOME,
    EQUIRECT_SPHERE,
    FISHEYE_DOME,
    CUBE
}

data class VideoLayoutInput(
    val fileName: String = "",
    val width: Int = 0,
    val height: Int = 0,
    val mediaStereoMode: String? = null,
    val isMvc: Boolean = false,
    val isSsif: Boolean = false,
    val isMvHevc: Boolean = false,
    val reverseEye: Boolean = false,
    val force3DVideoShow2D: Boolean = false,
    val curvedScreen: Boolean = false
)

data class UvRect(
    val u0: Float,
    val v0: Float,
    val u1: Float,
    val v1: Float
)

data class EyeUv(
    val left: UvRect,
    val right: UvRect
)

data class VideoLayoutDecision(
    val stereoPacking: StereoPacking,
    val projection: Projection,
    val renderGeometry: RenderGeometry,
    val eyeUv: EyeUv,
    val presentationAspect: Float,
    val requiresTwoEyeDecode: Boolean,
    val useSingleEyeForBothEyes: Boolean,
    val fisheyeFovDegrees: Float? = null
)

fun chooseVideoLayout(input: VideoLayoutInput): VideoLayoutDecision {
    val lowerName = input.fileName.lowercase()
    val stereoMode = input.mediaStereoMode?.lowercase().orEmpty()

    var reverseEye = input.reverseEye
    val projection = detectProjection(lowerName)
    val packing = detectStereoPacking(input, lowerName, stereoMode).also {
        if (stereoMode.contains("block_rl")) {
            reverseEye = !reverseEye
        }
    }

    val useSingleEye = input.force3DVideoShow2D && packing != StereoPacking.MONO_2D
    val eyeUv = chooseEyeUv(packing, reverseEye, useSingleEye)
    val geometry = chooseRenderGeometry(projection, input.curvedScreen)
    val aspect = computePresentationAspect(input.width, input.height, packing)
    val twoEyeDecode = packing == StereoPacking.MVC || packing == StereoPacking.MV_HEVC

    return VideoLayoutDecision(
        stereoPacking = packing,
        projection = projection,
        renderGeometry = geometry,
        eyeUv = eyeUv,
        presentationAspect = aspect,
        requiresTwoEyeDecode = twoEyeDecode,
        useSingleEyeForBothEyes = useSingleEye,
        fisheyeFovDegrees = fisheyeFov(projection)
    )
}

private fun detectStereoPacking(
    input: VideoLayoutInput,
    lowerName: String,
    stereoMode: String
): StereoPacking {
    if (has2dToken(lowerName)) return StereoPacking.MONO_2D
    if (input.isMvHevc || lowerName.contains("mv-hevc") || lowerName.contains("mvhevc")) {
        return StereoPacking.MV_HEVC
    }
    if (input.isMvc || input.isSsif || lowerName.endsWith(".ssif") || lowerName.contains(".ssif.")) {
        return StereoPacking.MVC
    }

    if (hasAny(lowerName, ".fsbs.", "full.sbs", "fullsbs", "full sbs")) return StereoPacking.FULL_SBS
    if (hasAny(lowerName, ".hsbs.", "half.sbs", "halfsbs", "half sbs")) return StereoPacking.HALF_SBS
    if (hasAny(lowerName, ".fou.", "full.ou", "fullou", "full ou")) return StereoPacking.FULL_OU
    if (hasAny(lowerName, ".hou.", "half.ou", "halfou", "half ou", ".tb.", "topbottom")) {
        return StereoPacking.HALF_OU
    }
    if (hasAny(lowerName, "mvc 3d", "mvc3d")) return StereoPacking.MVC
    if (hasAny(lowerName, ".sbs.", " sbs ", "_sbs_", "-sbs-", "sbs3d")) return StereoPacking.HALF_SBS
    if (hasAny(lowerName, ".ou.", " ou ", "_ou_", "-ou-")) return StereoPacking.HALF_OU

    if (stereoMode.contains("top_bottom") || stereoMode.contains("top-bottom")) {
        return StereoPacking.HALF_OU
    }
    if (stereoMode.contains("block_lr") || stereoMode.contains("block_rl")) {
        return StereoPacking.HALF_SBS
    }

    if (hasAny(lowerName, ".3d.", "3d180", "3d360", "3dfisheye", ".full3d.")) {
        return StereoPacking.UNKNOWN_STEREO
    }

    return StereoPacking.MONO_2D
}

private fun detectProjection(lowerName: String): Projection {
    return when {
        lowerName.contains("cube") -> Projection.CUBE
        hasAny(lowerName, "fisheye220", "fisheye 220") -> Projection.FISHEYE_220
        hasAny(lowerName, "fisheye200", "fisheye 200") -> Projection.FISHEYE_200
        hasAny(lowerName, "fisheye190", "fisheye 190") -> Projection.FISHEYE_190
        lowerName.contains("fisheye") || lowerName.contains(".180x180.") -> Projection.FISHEYE_180
        hasAny(lowerName, "youtube360", "youtube 360") -> Projection.YOUTUBE_360
        hasAny(lowerName, ".360.", "360vr", "vr360", "2d 360", "3d 360") -> Projection.EQUIRECT_360
        hasAny(lowerName, ".180.", "180vr", "vr180", "2d 180", "3d 180") -> Projection.EQUIRECT_180
        lowerName.contains(".vr.") -> Projection.EQUIRECT_360
        else -> Projection.FLAT
    }
}

private fun chooseRenderGeometry(
    projection: Projection,
    curvedScreen: Boolean
): RenderGeometry {
    return when (projection) {
        Projection.FLAT -> if (curvedScreen) RenderGeometry.CURVED_SCREEN else RenderGeometry.FLAT_QUAD
        Projection.EQUIRECT_180 -> RenderGeometry.EQUIRECT_HALF_DOME
        Projection.EQUIRECT_360,
        Projection.YOUTUBE_360 -> RenderGeometry.EQUIRECT_SPHERE
        Projection.FISHEYE_180,
        Projection.FISHEYE_190,
        Projection.FISHEYE_200,
        Projection.FISHEYE_220 -> RenderGeometry.FISHEYE_DOME
        Projection.CUBE -> RenderGeometry.CUBE
    }
}

private fun chooseEyeUv(
    packing: StereoPacking,
    reverseEye: Boolean,
    force2D: Boolean
): EyeUv {
    val whole = UvRect(0.0f, 0.0f, 1.0f, 1.0f)
    val sbsLeft = UvRect(0.0f, 0.0f, 0.5f, 1.0f)
    val sbsRight = UvRect(0.5f, 0.0f, 1.0f, 1.0f)
    val ouLeft = UvRect(0.0f, 0.0f, 1.0f, 0.5f)
    val ouRight = UvRect(0.0f, 0.5f, 1.0f, 1.0f)

    val base = when (packing) {
        StereoPacking.HALF_SBS,
        StereoPacking.FULL_SBS -> EyeUv(sbsLeft, sbsRight)
        StereoPacking.HALF_OU,
        StereoPacking.FULL_OU -> EyeUv(ouLeft, ouRight)
        StereoPacking.MVC,
        StereoPacking.MV_HEVC,
        StereoPacking.MONO_2D,
        StereoPacking.UNKNOWN_STEREO -> EyeUv(whole, whole)
    }

    val ordered = if (reverseEye) EyeUv(base.right, base.left) else base
    return if (force2D) EyeUv(ordered.left, ordered.left) else ordered
}

private fun computePresentationAspect(
    width: Int,
    height: Int,
    packing: StereoPacking
): Float {
    if (width <= 0 || height <= 0) return 16.0f / 9.0f

    return when (packing) {
        StereoPacking.FULL_SBS -> (width.toFloat() * 0.5f) / height.toFloat()
        StereoPacking.FULL_OU -> width.toFloat() / (height.toFloat() * 0.5f)
        else -> width.toFloat() / height.toFloat()
    }
}

private fun fisheyeFov(projection: Projection): Float? {
    return when (projection) {
        Projection.FISHEYE_180 -> 180.0f
        Projection.FISHEYE_190 -> 190.0f
        Projection.FISHEYE_200 -> 200.0f
        Projection.FISHEYE_220 -> 220.0f
        else -> null
    }
}

private fun hasAny(value: String, vararg needles: String): Boolean {
    return needles.any { value.contains(it) }
}

private fun has2dToken(value: String): Boolean {
    return hasAny(value, ".2d.", "_2d_", "-2d-", " 2d ")
}

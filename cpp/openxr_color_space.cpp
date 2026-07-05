// Reference-only XR_FB_color_space helper.
// The exact enum preference order should be tuned per runtime/headset.

#include <openxr/openxr.h>

#include <vector>

namespace pico_hdr {

using PFN_xrEnumerateColorSpacesFB =
    XrResult(XRAPI_PTR*)(XrSession, uint32_t, uint32_t*, XrColorSpaceFB*);
using PFN_xrSetColorSpaceFB =
    XrResult(XRAPI_PTR*)(XrSession, const XrColorSpaceFB);

static bool contains(const std::vector<XrColorSpaceFB>& values, XrColorSpaceFB value) {
    for (XrColorSpaceFB item : values) {
        if (item == value) return true;
    }
    return false;
}

XrResult choose_and_set_color_space(
    XrInstance instance,
    XrSession session,
    const std::vector<XrColorSpaceFB>& preferenceOrder
) {
    PFN_xrEnumerateColorSpacesFB xrEnumerateColorSpacesFB = nullptr;
    PFN_xrSetColorSpaceFB xrSetColorSpaceFB = nullptr;

    XrResult result = xrGetInstanceProcAddr(
        instance,
        "xrEnumerateColorSpacesFB",
        reinterpret_cast<PFN_xrVoidFunction*>(&xrEnumerateColorSpacesFB)
    );
    if (XR_FAILED(result)) return result;

    result = xrGetInstanceProcAddr(
        instance,
        "xrSetColorSpaceFB",
        reinterpret_cast<PFN_xrVoidFunction*>(&xrSetColorSpaceFB)
    );
    if (XR_FAILED(result)) return result;

    uint32_t count = 0;
    result = xrEnumerateColorSpacesFB(session, 0, &count, nullptr);
    if (XR_FAILED(result) || count == 0) return result;

    std::vector<XrColorSpaceFB> supported(count);
    result = xrEnumerateColorSpacesFB(session, count, &count, supported.data());
    if (XR_FAILED(result)) return result;

    for (XrColorSpaceFB candidate : preferenceOrder) {
        if (contains(supported, candidate)) {
            return xrSetColorSpaceFB(session, candidate);
        }
    }

    return xrSetColorSpaceFB(session, supported.front());
}

} // namespace pico_hdr


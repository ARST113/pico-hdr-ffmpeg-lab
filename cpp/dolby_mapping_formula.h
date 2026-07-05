#pragma once

#include <algorithm>
#include <cstdint>
#include <vector>

extern "C" {
#include <libavutil/dovi_meta.h>
}

namespace pico_hdr {

enum class DolbyCurveTextureKind {
    Identity2D,
    Polynomial2D,
    Mmr3D,
};

inline double pow2i(int value) {
    return static_cast<double>(std::uint64_t{1} << value);
}

inline double codeMaxForBitDepth(int bitDepth) {
    return static_cast<double>((std::uint64_t{1} << bitDepth) - 1);
}

inline double buildMmr(
    double x,
    double y,
    double z,
    int order,
    double constant,
    const double coeff[3][7]
) {
    const double xy = x * y;
    const double xz = x * z;
    const double yz = y * z;
    const double xyz = xy * z;

    const double basis[7] = {x, y, z, xy, xz, yz, xyz};

    double result = constant;
    int clampedOrder = std::clamp(order, 0, 3);

    for (int row = 0; row < clampedOrder; ++row) {
        for (int i = 0; i < 7; ++i) {
            double term = basis[i];
            if (row == 1) {
                term = term * term;
            } else if (row == 2) {
                term = term * term * term;
            }

            result += term * coeff[row][i];
        }
    }

    return result;
}

inline DolbyCurveTextureKind chooseCurveTextureKind(const AVDOVIReshapingCurve& curve) {
    if (curve.num_pivots <= 1) {
        return DolbyCurveTextureKind::Identity2D;
    }

    int mmrPieces = 0;
    int polynomialPieces = 0;
    const int pieces = curve.num_pivots - 1;

    for (int i = 0; i < pieces; ++i) {
        if (curve.mapping_idc[i] == AV_DOVI_MAPPING_MMR) {
            ++mmrPieces;
        } else if (curve.mapping_idc[i] == AV_DOVI_MAPPING_POLYNOMIAL) {
            ++polynomialPieces;
        }
    }

    if (mmrPieces == 1) {
        return DolbyCurveTextureKind::Mmr3D;
    }

    if (polynomialPieces >= 1) {
        return DolbyCurveTextureKind::Polynomial2D;
    }

    return DolbyCurveTextureKind::Identity2D;
}

inline void buildIdentity2D(std::vector<float>& out, int size = 1024) {
    out.resize(size);
    const double denom = static_cast<double>(size - 1);

    for (int i = 0; i < size; ++i) {
        out[i] = static_cast<float>(static_cast<double>(i) / denom);
    }
}

inline void buildPolynomial2D(
    std::vector<float>& out,
    const AVDOVIReshapingCurve& curve,
    int bitDepth,
    int coefLog2Denom
) {
    constexpr int kLutSize = 1024;
    buildIdentity2D(out, kLutSize);

    if (curve.num_pivots <= 1) {
        return;
    }

    struct Piece {
        double lowerPivot;
        double c0;
        double c1;
        double c2;
    };

    std::vector<Piece> pieces;
    pieces.reserve(curve.num_pivots - 1);

    const double codeMax = codeMaxForBitDepth(bitDepth);
    const double coefScale = pow2i(coefLog2Denom);

    for (int i = 0; i < curve.num_pivots - 1; ++i) {
        Piece piece{};
        piece.lowerPivot = static_cast<double>(curve.pivots[i]) / codeMax;
        piece.c0 = static_cast<double>(curve.poly_coef[i][0]) / coefScale;
        piece.c1 = static_cast<double>(curve.poly_coef[i][1]) / coefScale;
        piece.c2 = curve.poly_order[i] == 2
            ? static_cast<double>(curve.poly_coef[i][2]) / coefScale
            : 0.0;
        pieces.push_back(piece);
    }

    for (int i = 0; i < kLutSize; ++i) {
        const double x = static_cast<double>(i) / static_cast<double>(kLutSize - 1);

        int pieceIndex = -1;
        for (int p = static_cast<int>(pieces.size()) - 1; p >= 0; --p) {
            if (x >= pieces[p].lowerPivot) {
                pieceIndex = p;
                break;
            }
        }

        if (pieceIndex < 0) {
            continue;
        }

        const Piece& piece = pieces[pieceIndex];
        out[i] = static_cast<float>(piece.c0 + x * piece.c1 + x * x * piece.c2);
    }
}

inline void buildMmr3D(
    std::vector<float>& out,
    const AVDOVIReshapingCurve& curve,
    int coefLog2Denom
) {
    constexpr int kEdge = 16;
    constexpr int kCoeffCount = 7;
    out.clear();
    out.reserve(kEdge * kEdge * kEdge);

    const double coefScale = pow2i(coefLog2Denom);
    const double constant = static_cast<double>(curve.mmr_constant[0]) / coefScale;

    double coeff[3][kCoeffCount] = {};
    for (int row = 0; row < 3; ++row) {
        for (int i = 0; i < kCoeffCount; ++i) {
            coeff[row][i] = static_cast<double>(curve.mmr_coef[0][row][i]) / coefScale;
        }
    }

    for (int zi = 0; zi < kEdge; ++zi) {
        const double z = static_cast<double>(zi) / static_cast<double>(kEdge - 1);
        for (int yi = 0; yi < kEdge; ++yi) {
            const double y = static_cast<double>(yi) / static_cast<double>(kEdge - 1);
            for (int xi = 0; xi < kEdge; ++xi) {
                const double x = static_cast<double>(xi) / static_cast<double>(kEdge - 1);
                out.push_back(static_cast<float>(buildMmr(x, y, z, 3, constant, coeff)));
            }
        }
    }
}

} // namespace pico_hdr

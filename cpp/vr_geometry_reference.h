#pragma once

#include <algorithm>
#include <cstddef>
#include <cmath>
#include <cstdint>
#include <vector>

namespace pico_hdr {

constexpr float kPi = 3.14159265358979323846f;

struct Vec2 {
    float x = 0.0f;
    float y = 0.0f;
};

struct Vec3 {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
};

struct UvRect {
    float u0 = 0.0f;
    float v0 = 0.0f;
    float u1 = 1.0f;
    float v1 = 1.0f;
};

struct Vertex {
    Vec3 position;
    Vec2 uv;
};

struct Mesh {
    std::vector<Vertex> vertices;
    std::vector<std::uint32_t> indices;
};

enum class Eye {
    Left,
    Right,
};

enum class StereoPacking {
    Mono2D,
    HalfSbs,
    HalfOu,
    FullSbs,
    FullOu,
    Mvc,
    MvHevc,
    UnknownStereo,
};

inline float radians(float degrees) {
    return degrees * kPi / 180.0f;
}

inline float lerp(float a, float b, float t) {
    return a + (b - a) * t;
}

inline Vec2 mapUv(const UvRect& rect, float u, float v) {
    return Vec2{
        lerp(rect.u0, rect.u1, u),
        lerp(rect.v0, rect.v1, v),
    };
}

inline UvRect eyeUvRect(
    StereoPacking packing,
    Eye eye,
    bool reverseEye = false,
    bool force2D = false
) {
    if (force2D) {
        eye = Eye::Left;
    }
    if (reverseEye) {
        eye = eye == Eye::Left ? Eye::Right : Eye::Left;
    }

    switch (packing) {
        case StereoPacking::HalfSbs:
        case StereoPacking::FullSbs:
            return eye == Eye::Left
                ? UvRect{0.0f, 0.0f, 0.5f, 1.0f}
                : UvRect{0.5f, 0.0f, 1.0f, 1.0f};

        case StereoPacking::HalfOu:
        case StereoPacking::FullOu:
            return eye == Eye::Left
                ? UvRect{0.0f, 0.0f, 1.0f, 0.5f}
                : UvRect{0.0f, 0.5f, 1.0f, 1.0f};

        case StereoPacking::Mvc:
        case StereoPacking::MvHevc:
        case StereoPacking::Mono2D:
        case StereoPacking::UnknownStereo:
        default:
            return UvRect{0.0f, 0.0f, 1.0f, 1.0f};
    }
}

inline float presentationAspect(int width, int height, StereoPacking packing) {
    if (width <= 0 || height <= 0) {
        return 16.0f / 9.0f;
    }

    switch (packing) {
        case StereoPacking::FullSbs:
            return (static_cast<float>(width) * 0.5f) / static_cast<float>(height);
        case StereoPacking::FullOu:
            return static_cast<float>(width) / (static_cast<float>(height) * 0.5f);
        default:
            return static_cast<float>(width) / static_cast<float>(height);
    }
}

inline void appendGridIndices(Mesh& mesh, int columns, int rows, std::uint32_t base = 0) {
    for (int y = 0; y < rows; ++y) {
        for (int x = 0; x < columns; ++x) {
            const std::uint32_t i0 = base + static_cast<std::uint32_t>(y * (columns + 1) + x);
            const std::uint32_t i1 = i0 + 1;
            const std::uint32_t i2 = i0 + static_cast<std::uint32_t>(columns + 1);
            const std::uint32_t i3 = i2 + 1;

            mesh.indices.push_back(i0);
            mesh.indices.push_back(i2);
            mesh.indices.push_back(i1);
            mesh.indices.push_back(i1);
            mesh.indices.push_back(i2);
            mesh.indices.push_back(i3);
        }
    }
}

inline Mesh buildFlatGrid(
    int columns,
    int rows,
    float width,
    float height,
    float distance,
    const UvRect& uvRect
) {
    columns = std::max(columns, 1);
    rows = std::max(rows, 1);

    Mesh mesh;
    mesh.vertices.reserve(static_cast<std::size_t>((columns + 1) * (rows + 1)));
    mesh.indices.reserve(static_cast<std::size_t>(columns * rows * 6));

    for (int y = 0; y <= rows; ++y) {
        const float v = static_cast<float>(y) / static_cast<float>(rows);
        for (int x = 0; x <= columns; ++x) {
            const float u = static_cast<float>(x) / static_cast<float>(columns);
            mesh.vertices.push_back(Vertex{
                Vec3{(u - 0.5f) * width, (0.5f - v) * height, -std::abs(distance)},
                mapUv(uvRect, u, v),
            });
        }
    }

    appendGridIndices(mesh, columns, rows);
    return mesh;
}

inline Mesh buildCurvedScreenGrid(
    int columns,
    int rows,
    float horizontalDegrees,
    float radius,
    float height,
    const UvRect& uvRect
) {
    columns = std::max(columns, 1);
    rows = std::max(rows, 1);
    radius = std::max(radius, 0.01f);

    const float halfAngle = radians(horizontalDegrees) * 0.5f;
    Mesh mesh;
    mesh.vertices.reserve(static_cast<std::size_t>((columns + 1) * (rows + 1)));
    mesh.indices.reserve(static_cast<std::size_t>(columns * rows * 6));

    for (int y = 0; y <= rows; ++y) {
        const float v = static_cast<float>(y) / static_cast<float>(rows);
        for (int x = 0; x <= columns; ++x) {
            const float u = static_cast<float>(x) / static_cast<float>(columns);
            const float theta = lerp(-halfAngle, halfAngle, u);
            mesh.vertices.push_back(Vertex{
                Vec3{radius * std::sin(theta), (0.5f - v) * height, -radius * std::cos(theta)},
                mapUv(uvRect, u, v),
            });
        }
    }

    appendGridIndices(mesh, columns, rows);
    return mesh;
}

inline Mesh buildEquirectDome(
    int columns,
    int rows,
    float horizontalDegrees,
    float verticalDegrees,
    float radius,
    const UvRect& uvRect
) {
    columns = std::max(columns, 3);
    rows = std::max(rows, 2);
    radius = std::max(radius, 0.01f);

    const float halfHorizontal = radians(horizontalDegrees) * 0.5f;
    const float halfVertical = radians(verticalDegrees) * 0.5f;
    Mesh mesh;
    mesh.vertices.reserve(static_cast<std::size_t>((columns + 1) * (rows + 1)));
    mesh.indices.reserve(static_cast<std::size_t>(columns * rows * 6));

    for (int y = 0; y <= rows; ++y) {
        const float v = static_cast<float>(y) / static_cast<float>(rows);
        const float pitch = lerp(halfVertical, -halfVertical, v);
        const float cp = std::cos(pitch);
        const float sp = std::sin(pitch);

        for (int x = 0; x <= columns; ++x) {
            const float u = static_cast<float>(x) / static_cast<float>(columns);
            const float yaw = lerp(-halfHorizontal, halfHorizontal, u);
            mesh.vertices.push_back(Vertex{
                Vec3{
                    radius * cp * std::sin(yaw),
                    radius * sp,
                    -radius * cp * std::cos(yaw),
                },
                mapUv(uvRect, u, v),
            });
        }
    }

    appendGridIndices(mesh, columns, rows);
    return mesh;
}

inline Mesh buildFisheyeDome(
    int segments,
    int rings,
    float fisheyeDegrees,
    float radius,
    const UvRect& uvRect
) {
    segments = std::max(segments, 8);
    rings = std::max(rings, 1);
    radius = std::max(radius, 0.01f);

    const float halfFov = radians(fisheyeDegrees) * 0.5f;
    Mesh mesh;
    mesh.vertices.reserve(static_cast<std::size_t>(1 + segments * rings));
    mesh.indices.reserve(static_cast<std::size_t>(segments * (3 + (rings - 1) * 6)));

    const float cu = (uvRect.u0 + uvRect.u1) * 0.5f;
    const float cv = (uvRect.v0 + uvRect.v1) * 0.5f;
    const float ru = (uvRect.u1 - uvRect.u0) * 0.5f;
    const float rv = (uvRect.v1 - uvRect.v0) * 0.5f;

    mesh.vertices.push_back(Vertex{Vec3{0.0f, 0.0f, -radius}, Vec2{cu, cv}});

    for (int ring = 1; ring <= rings; ++ring) {
        const float fr = static_cast<float>(ring) / static_cast<float>(rings);
        const float theta = fr * halfFov;
        const float st = std::sin(theta);
        const float ct = std::cos(theta);

        for (int segment = 0; segment < segments; ++segment) {
            const float a = (static_cast<float>(segment) / static_cast<float>(segments)) * 2.0f * kPi;
            const float ca = std::cos(a);
            const float sa = std::sin(a);
            mesh.vertices.push_back(Vertex{
                Vec3{radius * st * ca, radius * st * sa, -radius * ct},
                Vec2{cu + ru * fr * ca, cv + rv * fr * sa},
            });
        }
    }

    for (int segment = 0; segment < segments; ++segment) {
        const std::uint32_t next = static_cast<std::uint32_t>((segment + 1) % segments);
        mesh.indices.push_back(0);
        mesh.indices.push_back(1 + next);
        mesh.indices.push_back(1 + static_cast<std::uint32_t>(segment));
    }

    for (int ring = 2; ring <= rings; ++ring) {
        const std::uint32_t prevBase = 1 + static_cast<std::uint32_t>((ring - 2) * segments);
        const std::uint32_t curBase = 1 + static_cast<std::uint32_t>((ring - 1) * segments);

        for (int segment = 0; segment < segments; ++segment) {
            const std::uint32_t next = static_cast<std::uint32_t>((segment + 1) % segments);
            const std::uint32_t p0 = prevBase + static_cast<std::uint32_t>(segment);
            const std::uint32_t p1 = prevBase + next;
            const std::uint32_t c0 = curBase + static_cast<std::uint32_t>(segment);
            const std::uint32_t c1 = curBase + next;

            mesh.indices.push_back(p0);
            mesh.indices.push_back(c0);
            mesh.indices.push_back(p1);
            mesh.indices.push_back(p1);
            mesh.indices.push_back(c0);
            mesh.indices.push_back(c1);
        }
    }

    return mesh;
}

inline Mesh buildCubeFace(
    const Vec3& p00,
    const Vec3& p10,
    const Vec3& p01,
    const Vec3& p11,
    const UvRect& uvRect
) {
    Mesh mesh;
    mesh.vertices = {
        Vertex{p00, mapUv(uvRect, 0.0f, 0.0f)},
        Vertex{p10, mapUv(uvRect, 1.0f, 0.0f)},
        Vertex{p01, mapUv(uvRect, 0.0f, 1.0f)},
        Vertex{p11, mapUv(uvRect, 1.0f, 1.0f)},
    };
    mesh.indices = {0, 2, 1, 1, 2, 3};
    return mesh;
}

} // namespace pico_hdr

#pragma once

#include <string>
#include <vector>

namespace ImageOutline
{
    struct Point
    {
        int x = 0;
        int y = 0;

        bool operator==(const Point& other) const = default;
    };

    bool BuildOutlineFromAlpha(
        const std::string& path,
        int alphaThreshold,
        int vertexStride,
        std::vector<Point>& outPoints,
        int& outWidth,
        int& outHeight);
}

#include "image_outline.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <unordered_map>
#include <vector>

#include "DxLib.h"

namespace ImageOutline
{
    namespace
    {
        struct Edge
        {
            Point start;
            Point end;
            bool used = false;
        };

        struct PointHash
        {
            size_t operator()(const Point& point) const noexcept
            {
                const uint64_t x = static_cast<uint32_t>(point.x);
                const uint64_t y = static_cast<uint32_t>(point.y);
                return static_cast<size_t>((x << 32) ^ y);
            }
        };

        bool IsFilled(const std::vector<uint8_t>& mask, int width, int x, int y)
        {
            if (x < 0 || y < 0 || x >= width)
            {
                return false;
            }

            const size_t index = static_cast<size_t>(y) * static_cast<size_t>(width) + static_cast<size_t>(x);
            return index < mask.size() && mask[index] != 0;
        }

        void AppendEdge(std::vector<Edge>& edges, Point start, Point end)
        {
            edges.push_back({ start, end, false });
        }

        float ComputeSignedArea(const std::vector<Point>& points)
        {
            if (points.size() < 3)
            {
                return 0.0f;
            }

            double area = 0.0;
            for (size_t index = 0; index < points.size(); ++index)
            {
                const Point& a = points[index];
                const Point& b = points[(index + 1) % points.size()];
                area += static_cast<double>(a.x) * static_cast<double>(b.y) -
                    static_cast<double>(b.x) * static_cast<double>(a.y);
            }

            return static_cast<float>(area * 0.5);
        }

        void RemoveCollinearPoints(std::vector<Point>& points)
        {
            if (points.size() < 3)
            {
                return;
            }

            bool changed = true;
            while (changed && points.size() >= 3)
            {
                changed = false;
                for (size_t index = 0; index < points.size(); ++index)
                {
                    const Point& prev = points[(index + points.size() - 1) % points.size()];
                    const Point& curr = points[index];
                    const Point& next = points[(index + 1) % points.size()];
                    const int dx1 = curr.x - prev.x;
                    const int dy1 = curr.y - prev.y;
                    const int dx2 = next.x - curr.x;
                    const int dy2 = next.y - curr.y;
                    if (dx1 * dy2 - dy1 * dx2 == 0)
                    {
                        points.erase(points.begin() + static_cast<std::ptrdiff_t>(index));
                        changed = true;
                        break;
                    }
                }
            }
        }

        void ApplyStride(std::vector<Point>& points, int vertexStride)
        {
            if (vertexStride <= 1 || points.size() <= 8)
            {
                return;
            }

            std::vector<Point> reduced;
            reduced.reserve(points.size() / static_cast<size_t>(vertexStride) + 2);
            for (size_t index = 0; index < points.size(); index += static_cast<size_t>(vertexStride))
            {
                reduced.push_back(points[index]);
            }

            if (reduced.size() >= 3)
            {
                points = std::move(reduced);
                RemoveCollinearPoints(points);
            }
        }
    }

    bool BuildOutlineFromAlpha(
        const std::string& path,
        int alphaThreshold,
        int vertexStride,
        std::vector<Point>& outPoints,
        int& outWidth,
        int& outHeight)
    {
        outPoints.clear();
        outWidth = 0;
        outHeight = 0;

        const int softImage = LoadSoftImage(path.c_str());
        if (softImage < 0)
        {
            return false;
        }

        int width = 0;
        int height = 0;
        if (GetSoftImageSize(softImage, &width, &height) != 0 || width <= 0 || height <= 0)
        {
            DeleteSoftImage(softImage);
            return false;
        }

        std::vector<uint8_t> mask(static_cast<size_t>(width) * static_cast<size_t>(height), 0);
        const int threshold = (std::clamp)(alphaThreshold, 0, 255);
        for (int y = 0; y < height; ++y)
        {
            for (int x = 0; x < width; ++x)
            {
                int r = 0;
                int g = 0;
                int b = 0;
                int a = 0;
                if (GetPixelSoftImage(softImage, x, y, &r, &g, &b, &a) == 0 && a >= threshold)
                {
                    mask[static_cast<size_t>(y) * static_cast<size_t>(width) + static_cast<size_t>(x)] = 1;
                }
            }
        }

        DeleteSoftImage(softImage);

        std::vector<Edge> edges;
        for (int y = 0; y < height; ++y)
        {
            for (int x = 0; x < width; ++x)
            {
                if (!IsFilled(mask, width, x, y))
                {
                    continue;
                }

                if (!IsFilled(mask, width, x, y - 1))
                {
                    AppendEdge(edges, { x, y }, { x + 1, y });
                }
                if (!IsFilled(mask, width, x + 1, y))
                {
                    AppendEdge(edges, { x + 1, y }, { x + 1, y + 1 });
                }
                if (!IsFilled(mask, width, x, y + 1))
                {
                    AppendEdge(edges, { x + 1, y + 1 }, { x, y + 1 });
                }
                if (!IsFilled(mask, width, x - 1, y))
                {
                    AppendEdge(edges, { x, y + 1 }, { x, y });
                }
            }
        }

        if (edges.empty())
        {
            return false;
        }

        std::unordered_map<Point, std::vector<size_t>, PointHash> outgoing;
        outgoing.reserve(edges.size());
        for (size_t index = 0; index < edges.size(); ++index)
        {
            outgoing[edges[index].start].push_back(index);
        }

        std::vector<Point> bestLoop;
        float bestArea = 0.0f;

        for (size_t edgeIndex = 0; edgeIndex < edges.size(); ++edgeIndex)
        {
            if (edges[edgeIndex].used)
            {
                continue;
            }

            std::vector<Point> loop;
            loop.push_back(edges[edgeIndex].start);

            size_t currentEdgeIndex = edgeIndex;
            const Point start = edges[currentEdgeIndex].start;
            Point current = edges[currentEdgeIndex].end;
            edges[currentEdgeIndex].used = true;

            int guard = 0;
            while (!(current == start) && guard < static_cast<int>(edges.size()) + 1)
            {
                loop.push_back(current);
                auto it = outgoing.find(current);
                if (it == outgoing.end())
                {
                    break;
                }

                bool foundNext = false;
                for (size_t candidateIndex : it->second)
                {
                    if (edges[candidateIndex].used)
                    {
                        continue;
                    }

                    currentEdgeIndex = candidateIndex;
                    current = edges[currentEdgeIndex].end;
                    edges[currentEdgeIndex].used = true;
                    foundNext = true;
                    break;
                }

                if (!foundNext)
                {
                    break;
                }

                ++guard;
            }

            if (!(current == start) || loop.size() < 3)
            {
                continue;
            }

            RemoveCollinearPoints(loop);
            ApplyStride(loop, vertexStride);
            if (loop.size() < 3)
            {
                continue;
            }

            const float area = std::fabs(ComputeSignedArea(loop));
            if (area > bestArea)
            {
                bestArea = area;
                bestLoop = std::move(loop);
            }
        }

        if (bestLoop.size() < 3)
        {
            return false;
        }

        outPoints = std::move(bestLoop);
        outWidth = width;
        outHeight = height;
        return true;
    }
}

// SPDX-FileCopyrightText: 2026 SemkiShow
//
// SPDX-License-Identifier: GPL-3.0-only

#include <cmath>
#include <cstring>
#include <fstream>
#include <iostream>
#include <stb_image.h>
#include <unordered_set>
#include <vector>

struct Point
{
    int x = 0, y = 0;

    bool operator<(const Point& other) const { return x < other.x || y < other.y; }
    bool operator==(const Point& other) const { return x == other.x && y == other.y; }
    bool operator!=(const Point& other) const { return !(*this == other); }
};

static int HexToDec(const std::string& hex)
{
    int out = 0;
    for (auto& c: hex)
    {
        out *= 16;
        if (c >= '0' && c <= '9') out += c - '0';
        if (c >= 'A' && c <= 'Z') out += c - 'A' + 10;
        if (c >= 'a' && c <= 'z') out += c - 'a' + 10;
    }
    return out;
}

struct Color
{
    unsigned char r = 255, g = 255, b = 255;

    bool operator==(const Color& other) const
    {
        return r == other.r && g == other.g && b == other.b;
    }

    static Color FromHex(const std::string& hex)
    {
        Color color;

        if (hex.size() < 7)
        {
            std::cerr << "Invalid hex: " << hex << '\n';
            return color;
        }

        color.r = HexToDec(hex.substr(1, 2));
        color.g = HexToDec(hex.substr(3, 2));
        color.b = HexToDec(hex.substr(5, 2));
        return color;
    }
};

template <> struct std::hash<Point>
{
    std::size_t operator()(const Point& p) const
    {
        auto h1 = std::hash<int>{}(p.x);
        auto h2 = std::hash<int>{}(p.y);
        // NOLINTNEXTLINE (readability-magic-numbers)
        return h1 ^ (h2 + 0x9e3779b9 + (h1 << 6) + (h1 >> 2));
    }
};

static std::vector<Point> GetBorderPoints(const std::vector<Point>& points, int stepSize,
                                          int approxStepSize)
{
    if (points.empty()) return {};

    std::unordered_set<Point> pointSet(points.begin(), points.end());

    // Find the start point
    Point startPoint = points[0];
    for (const auto& p: points)
    {
        startPoint = std::min(startPoint, p);
    }

    const int DIRS_SIZE = 8;
    Point dirs[DIRS_SIZE] = {{0, -1}, {1, -1}, {1, 0}, {1, 1}, {0, 1}, {-1, 1}, {-1, 0}, {-1, -1}};

    // Extract border points using a wall follower algorithm
    std::vector<Point> borderPoints;
    Point currentPoint = startPoint;
    int backtrackDir = 7;
    do
    {
        borderPoints.push_back({currentPoint.x * stepSize, currentPoint.y * stepSize});

        bool foundNext = false;
        for (int i = 0; i < DIRS_SIZE; i++)
        {
            int checkIdx = (backtrackDir + i) % DIRS_SIZE;
            Point neighbor = {currentPoint.x + dirs[checkIdx].x, currentPoint.y + dirs[checkIdx].y};

            if (pointSet.count(neighbor) > 0)
            {
                currentPoint = neighbor;
                backtrackDir = (checkIdx + 5) % DIRS_SIZE;
                foundNext = true;
                break;
            }
        }

        if (!foundNext) break;
    } while (currentPoint != startPoint);

    // Return early if there are not enough points anyway
    if (borderPoints.size() < 3) return borderPoints;

    // Approximate border points as an optimisation
    std::vector<Point> approxBorderPoints;
    approxBorderPoints.push_back(borderPoints[0]);
    const int SQR_APPROX = approxStepSize * approxStepSize;
    for (size_t i = 1; i < borderPoints.size(); i++)
    {
        Point a = borderPoints[i];
        Point b = approxBorderPoints.back();
        if (pow(a.x - b.x, 2) + pow(a.y - b.y, 2) >= SQR_APPROX)
        {
            approxBorderPoints.push_back(borderPoints[i]);
        }
    }

    // Return the original border points if the approximation removed too many points
    return (approxBorderPoints.size() < 3 ? borderPoints : approxBorderPoints);
}

static void PrintHelp(char* programName)
{
    printf("Usage: %s [OPTION...] <-i image>\n", programName);
    printf("Convert a pixel mask into a polygon\n");
    printf("\n");
    printf("-h, --help    Print help\n");
    printf("-i            Provide an input image\n");
    printf("-o            Provide the output path (.ppm will be appended to the input)\n");
    printf("-c            Provide the mask color as #rrggbb\n");
    printf("-s            Provide the step size\n");
}

int main(int argc, char* argv[])
{
    const char* input = NULL;
    const char* output = NULL;
    Color maskColor = {255, 255, 255};
    int stepSize = 5;
    for (int i = 1; i < argc; i++)
    {
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0)
        {
            PrintHelp(argv[0]);
            return 0;
        }
        if (strcmp(argv[i], "-i") == 0 && i + 1 < argc)
        {
            input = argv[i + 1];
            i++;
        }
        if (strcmp(argv[i], "-o") == 0 && i + 1 < argc)
        {
            output = argv[i + 1];
            i++;
        }
        if (strcmp(argv[i], "-c") == 0 && i + 1 < argc)
        {
            maskColor = Color::FromHex(argv[i + 1]);
            i++;
        }
        if (strcmp(argv[i], "-s") == 0 && i + 1 < argc)
        {
            stepSize = atoi(argv[i + 1]);
            i++;
        }
    }
    if (!input)
    {
        PrintHelp(argv[0]);
        return 1;
    }

    fprintf(stderr, "Mask color: %d %d %d\n", maskColor.r, maskColor.g, maskColor.b);

    int w, h, n;
    unsigned char* data = stbi_load(input, &w, &h, &n, 0);
    fprintf(stderr, "There are %d channels in the file\n", n);

    std::vector<Point> points;
    for (int y = 0; y < h; y++)
    {
        for (int x = 0; x < w; x++)
        {
            unsigned char* pixel = data + (y * w + x) * n;
            if (Color{pixel[0], pixel[1], pixel[2]} == maskColor)
            {
                points.push_back({x, y});

                if (output)
                {
                    pixel[0] = 255;
                    pixel[1] = 0;
                    pixel[2] = 0;
                }
            }
        }
    }
    auto borderPoints = GetBorderPoints(points, 1, stepSize);
    fprintf(stderr, "Found %zu border points:\n", borderPoints.size());
    for (auto& point: borderPoints)
    {
        if (output)
        {
            unsigned char* pixel = data + (point.y * w + point.x) * n;
            pixel[0] = 0;
            pixel[1] = 0;
            pixel[2] = 255;
        }

        printf("%d,%d,", point.x, point.y);
    }

    if (output)
    {
        std::ofstream file(std::string(output) + ".ppm");
        file << "P3\n" << w << ' ' << h << "\n255\n";
        for (int y = 0; y < h; y++)
        {
            for (int x = 0; x < w; x++)
            {
                file << (unsigned int)data[(y * w + x) * n + 0] << ' '
                     << (unsigned int)data[(y * w + x) * n + 1] << ' '
                     << (unsigned int)data[(y * w + x) * n + 2] << '\n';
            }
        }
    }

    stbi_image_free(data);

    return 0;
}

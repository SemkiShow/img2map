// SPDX-FileCopyrightText: 2026 SemkiShow
//
// SPDX-License-Identifier: GPL-3.0-only

#include <cmath>
#include <cstddef>
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
    constexpr int HEX = 16;
    constexpr int LETTER_OFFSET = 10;
    int out = 0;
    for (const auto& c: hex)
    {
        out *= HEX;
        if (c >= '0' && c <= '9') out += c - '0';
        if (c >= 'A' && c <= 'Z') out += c - 'A' + LETTER_OFFSET;
        if (c >= 'a' && c <= 'z') out += c - 'a' + LETTER_OFFSET;
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

        constexpr int HEX_LEN = 7;
        if (hex.size() < HEX_LEN)
        {
            std::cerr << "Invalid hex: " << hex << '\n';
            return color;
        }

        constexpr int R_OFFSET = 1;
        constexpr int G_OFFSET = 3;
        constexpr int B_OFFSET = 5;
        constexpr int COLOR_LEN = 2;

        color.r = HexToDec(hex.substr(R_OFFSET, COLOR_LEN));
        color.g = HexToDec(hex.substr(G_OFFSET, COLOR_LEN));
        color.b = HexToDec(hex.substr(B_OFFSET, COLOR_LEN));
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

struct BorderPointsOptions
{
    int stepSize;
    int approxStepSize;
};

static std::vector<Point> GetBorderPoints(const std::vector<Point>& points,
                                          BorderPointsOptions options)
{
    if (points.empty()) return {};

    std::unordered_set<Point> pointSet(points.begin(), points.end());

    // Find the start point
    Point startPoint = points[0];
    for (const auto& p: points)
    {
        startPoint = std::min(startPoint, p);
    }

    constexpr int DIRS_LEN = 8;
    Point dirs[] = {{0, -1}, {1, -1}, {1, 0}, {1, 1}, {0, 1}, {-1, 1}, {-1, 0}, {-1, -1}};

    // Extract border points using a wall follower algorithm
    std::vector<Point> borderPoints;
    Point currentPoint = startPoint;
    constexpr int START_BACKTRACK = 7;
    constexpr int BACKTRACK_OFFSET = 5;
    int backtrackDir = START_BACKTRACK;
    do
    {
        borderPoints.push_back(
            {currentPoint.x * options.stepSize, currentPoint.y * options.stepSize});

        bool foundNext = false;
        for (int i = 0; i < DIRS_LEN; i++)
        {
            int checkIdx = (backtrackDir + i) % DIRS_LEN;
            Point neighbor = {currentPoint.x + dirs[checkIdx].x, currentPoint.y + dirs[checkIdx].y};

            if (pointSet.count(neighbor) > 0)
            {
                currentPoint = neighbor;
                backtrackDir = (checkIdx + BACKTRACK_OFFSET) % DIRS_LEN;
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
    const int SQR_APPROX = options.approxStepSize * options.approxStepSize;
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
    printf("-i            Provide an input image (required)\n");
    printf("-o            Provide the output path (.ppm will be appended)\n");
    printf("-c            Provide the mask color as #rrggbb (the default value is #ffffff)\n");
    printf("-s            Provide the step size (the default value is 5)\n");
}

constexpr int MAX_COLOR = 255;
constexpr Color DEFAULT_MASK_COLOR = {MAX_COLOR, MAX_COLOR, MAX_COLOR};
constexpr int DEFAULT_STEP_SIZE = 5;

static char* g_input = nullptr;
static char* g_output = nullptr;
static Color g_maskColor = DEFAULT_MASK_COLOR;
static int g_stepSize = DEFAULT_STEP_SIZE;

static void ProcessArgs(int argc, char* argv[])
{
    for (int i = 1; i < argc; i++)
    {
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0)
        {
            PrintHelp(argv[0]);
            exit(0);
        }
        if (strcmp(argv[i], "-i") == 0 && i + 1 < argc)
        {
            g_input = argv[i + 1];
            i++;
        }
        if (strcmp(argv[i], "-o") == 0 && i + 1 < argc)
        {
            g_output = argv[i + 1];
            i++;
        }
        if (strcmp(argv[i], "-c") == 0 && i + 1 < argc)
        {
            g_maskColor = Color::FromHex(argv[i + 1]);
            i++;
        }
        if (strcmp(argv[i], "-s") == 0 && i + 1 < argc)
        {
            g_stepSize = atoi(argv[i + 1]);
            i++;
        }
    }
}

int main(int argc, char* argv[])
{
    ProcessArgs(argc, argv);
    if (g_input == nullptr)
    {
        PrintHelp(argv[0]);
        return 1;
    }

    fprintf(stderr, "Mask color: %d %d %d\n", g_maskColor.r, g_maskColor.g, g_maskColor.b);

    int w;
    int h;
    int n;
    unsigned char* data = stbi_load(g_input, &w, &h, &n, 0);
    if (data == nullptr)
    {
        fprintf(stderr, "Error: failed to open file %s!\n", g_input);
        return 1;
    }
    fprintf(stderr, "There are %d channels in the file\n", n);

    std::vector<Point> points;
    for (int y = 0; y < h; y++)
    {
        for (int x = 0; x < w; x++)
        {
            unsigned char* pixel = data + static_cast<ptrdiff_t>((y * w + x) * n);
            if (Color{pixel[0], pixel[1], pixel[2]} == g_maskColor)
            {
                points.push_back({x, y});

                if (g_output != nullptr)
                {
                    pixel[0] = MAX_COLOR;
                    pixel[1] = 0;
                    pixel[2] = 0;
                }
            }
        }
    }

    auto borderPoints = GetBorderPoints(points, {1, g_stepSize});
    fprintf(stderr, "Found %zu border points\n", borderPoints.size());
    for (auto& point: borderPoints)
    {
        if (g_output != nullptr)
        {
            unsigned char* pixel = data + static_cast<ptrdiff_t>((point.y * w + point.x) * n);
            pixel[0] = 0;
            pixel[1] = 0;
            pixel[2] = MAX_COLOR;
        }

        printf("%d,%d,", point.x, point.y);
    }

    if (g_output != nullptr)
    {
        std::ofstream file(std::string(g_output) + ".ppm");
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

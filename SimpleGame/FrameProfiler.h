#pragma once
#include "Dependencies/glew.h"
#include <chrono>
#include <cstdint>
#include <cstdio>

// Single render-thread counter shared across translation units.
// Count submitted drawing commands, not buffer-selection/state changes.
class FrameProfiler
{
  public:
    static void BeginFrame()
    {
        auto& stats = Data();
        if (!stats.started)
        {
            stats.windowStart = Clock::now();
            stats.started = true;
        }
        stats.arrays = 0;
        stats.pixels = 0;
        stats.inFrame = true;
    }

    static void DrawArrays(GLenum mode, GLint first, GLsizei count)
    {
        glDrawArrays(mode, first, count);
        if (Data().inFrame)
            ++Data().arrays;
    }

    static void DrawPixels(
        GLsizei width, GLsizei height, GLenum format, GLenum type, const void* pixels)
    {
        glDrawPixels(width, height, format, type, pixels);
        if (Data().inFrame)
            ++Data().pixels;
    }

    static void EndFrame()
    {
        auto& stats = Data();
        stats.inFrame = false;
        const auto calls = stats.arrays + stats.pixels;
        if (stats.frames == 0 || calls < stats.minimum)
            stats.minimum = calls;
        if (calls > stats.maximum)
            stats.maximum = calls;
        stats.total += calls;
        ++stats.frames;
        const auto now = Clock::now();
        const double seconds = std::chrono::duration<double>(now - stats.windowStart).count();
        if (seconds < 1.0)
            return;
        std::printf(
            "[성능] FPS %.1f | DrawCall/프레임 최근 %llu · 평균 %.1f · 최소 %llu · 최대 %llu"
            " | 최근 glDrawArrays %llu · glDrawPixels %llu\n",
            double(stats.frames) / seconds,
            static_cast<unsigned long long>(calls),
            double(stats.total) / double(stats.frames),
            static_cast<unsigned long long>(stats.minimum),
            static_cast<unsigned long long>(stats.maximum),
            static_cast<unsigned long long>(stats.arrays),
            static_cast<unsigned long long>(stats.pixels));
        stats.windowStart = now;
        stats.frames = stats.total = stats.minimum = stats.maximum = 0;
    }

  private:
    using Clock = std::chrono::steady_clock;

    struct Statistics
    {
        Clock::time_point windowStart;
        std::uint64_t arrays = 0, pixels = 0, frames = 0, total = 0, minimum = 0, maximum = 0;
        bool started = false, inFrame = false;
    };

    static Statistics& Data()
    {
        static Statistics stats;
        return stats;
    }
};

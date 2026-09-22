#pragma once
#include "Dependencies/glew.h"
#include "RenderStorage.h"
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <array>
#include <sstream>

class FrameProfiler
{
  public:
    using Clock = std::chrono::steady_clock;

    static double Milliseconds(Clock::time_point start)
    {
        return std::chrono::duration<double, std::milli>(Clock::now() - start).count();
    }

    static void Event(const char* type, const std::string& key, size_t bytes, double ms = 0)
    {
        auto& s = Data();
        if (!s.started)
        {
            s.startup << s.sequence << ',' << type << ',' << bytes << ',' << ms << ',' << key
                      << '\n';
        }
        else if (s.events)
            s.events << s.sequence << ',' << type << ',' << bytes << ',' << ms << ',' << key
                     << '\n';
    }

    static void BeginFrame()
    {
        auto& s = Data();
        if (!s.started)
        {
            s.windowStart = Clock::now();
            s.previous = s.windowStart;
            s.started = true;
            const auto folder = RenderStorage::Folder(L"Performance");
            SYSTEMTIME time = {};
            GetLocalTime(&time);
            wchar_t name[100] = {};
            swprintf_s(name,
                       L"/%04u%02u%02u-%02u%02u%02u-%03u-%lu",
                       time.wYear,
                       time.wMonth,
                       time.wDay,
                       time.wHour,
                       time.wMinute,
                       time.wSecond,
                       time.wMilliseconds,
                       GetCurrentProcessId());
            if (!folder.empty())
            {
                auto stem = folder + name;
                s.csv.open((stem + L"-frames.csv").c_str());
                s.events.open((stem + L"-events.csv").c_str());
                s.csv << "frame,interval_ms,render_cpu_ms,swap_ms,update_cpu_ms,draw_arrays,draw_"
                         "instanced,draw_pixels,instances,useful_vertices,submitted_vertices,"
                         "upload_bytes\n";
                s.events << "frame,event,bytes,milliseconds,key\n";
                s.events << s.startup.str();
                s.startup.str("");
                std::ofstream metadata((stem + L"-session.txt").c_str());
                metadata << "schema=1\npatch=1.0.7v\nbatching="
                         << RenderStorage::Enabled(L"GSE_RENDER_BATCH")
                         << "\ndisk_cache=" << RenderStorage::Enabled(L"GSE_MESH_DISK_CACHE")
                         << "\nGL_VENDOR=" << glGetString(GL_VENDOR)
                         << "\nGL_RENDERER=" << glGetString(GL_RENDERER)
                         << "\nGL_VERSION=" << glGetString(GL_VERSION)
                         << "\nTimer delay=16ms; CPU submission is not GPU execution time.\n";
                std::printf("[성능] 분석 로그: %ls\n", stem.c_str());
                if (!s.csv || !s.events || !metadata)
                    std::printf(
                        "[성능] 일부 분석 파일을 열 수 없습니다. 콘솔 기록은 계속합니다.\n");
            }
        }
        s.frameStart = Clock::now();
        s.interval = std::chrono::duration<double, std::milli>(s.frameStart - s.previous).count();
        s.previous = s.frameStart;
        ++s.sequence;
        s.arrays = s.instanced = s.pixels = s.instances = s.useful = s.submitted = s.upload = 0;
        s.activeQuery = -1;
        for (size_t i = 0; i < s.queries.size(); ++i)
        {
            auto& query = s.queries[i];
            if (query.pending)
            {
                GLint ready = GL_FALSE;
                glGetQueryObjectiv(query.id, GL_QUERY_RESULT_AVAILABLE, &ready);
                if (ready)
                {
                    GLuint64 ns = 0;
                    glGetQueryObjectui64v(query.id, GL_QUERY_RESULT, &ns);
                    Event("gpu_ms", std::to_string(query.frame), 0, double(ns) / 1000000.0);
                    query.pending = false;
                }
            }
            if (!query.pending && s.activeQuery < 0)
                s.activeQuery = static_cast<int>(i);
        }
        if (s.activeQuery >= 0)
        {
            auto& query = s.queries[s.activeQuery];
            if (!query.id)
                glGenQueries(1, &query.id);
            query.frame = s.sequence;
            glBeginQuery(GL_TIME_ELAPSED, query.id);
        }
        else
            Event("gpu_query_skipped", "ring_busy", 0);
    }

    static void UpdateTime(double ms)
    {
        Data().update = ms;
    }

    static void CacheHit()
    {
        ++Data().cacheHits;
    }

    static void BeforeSwap()
    {
        auto& s = Data();
        if (s.activeQuery >= 0)
        {
            glEndQuery(GL_TIME_ELAPSED);
            s.queries[s.activeQuery].pending = true;
        }
        Data().render = Milliseconds(Data().frameStart);
        Data().swapStart = Clock::now();
    }

    static void Close()
    {
        auto& s = Data();
        for (auto& query : s.queries)
        {
            if (query.pending)
                Event("gpu_query_uncollected", std::to_string(query.frame), 0);
            if (query.id)
                glDeleteQueries(1, &query.id);
            query = {};
        }
        s.csv.flush();
        s.events.flush();
    }

    static void Upload(size_t bytes)
    {
        Data().upload += bytes;
    }

    static void DrawArrays(GLenum mode, GLint first, GLsizei count)
    {
        glDrawArrays(mode, first, count);
        ++Data().arrays;
    }

    static void DrawInstanced(GLsizei vertices, GLsizei instances, size_t useful)
    {
        glDrawArraysInstanced(GL_TRIANGLES, 0, vertices, instances);
        ++Data().instanced;
        Data().instances += instances;
        Data().useful += useful;
        Data().submitted += static_cast<std::uint64_t>(vertices) * instances;
    }

    static void DrawPixels(GLsizei w, GLsizei h, GLenum format, GLenum type, const void* pixels)
    {
        glDrawPixels(w, h, format, type, pixels);
        ++Data().pixels;
    }

    static void EndFrame()
    {
        auto& s = Data();
        const double swap = Milliseconds(s.swapStart);
        const auto calls = s.arrays + s.instanced + s.pixels;
        if (!s.frames || calls < s.minimum)
            s.minimum = calls;
        if (calls > s.maximum)
            s.maximum = calls;
        s.total += calls;
        ++s.frames;
        if (s.csv)
            s.csv << s.sequence << ',' << s.interval << ',' << s.render << ',' << swap << ','
                  << s.update << ',' << s.arrays << ',' << s.instanced << ',' << s.pixels << ','
                  << s.instances << ',' << s.useful << ',' << s.submitted << ',' << s.upload
                  << '\n';
        const double seconds = std::chrono::duration<double>(Clock::now() - s.windowStart).count();
        if (seconds < 1)
            return;
        Event("mesh_ram_hits_interval", "", s.cacheHits);
        s.cacheHits = 0;
        std::printf("[성능] FPS %.1f | DrawCall 최근 %llu · 평균 %.1f · 최소 %llu · 최대 %llu"
                    " | 일반 %llu · 인스턴싱 %llu · 픽셀 %llu | 인스턴스 %llu | CPU 렌더 %.2fms · "
                    "화면교환 %.2fms · 갱신 %.2fms\n",
                    s.frames / seconds,
                    calls,
                    double(s.total) / s.frames,
                    s.minimum,
                    s.maximum,
                    s.arrays,
                    s.instanced,
                    s.pixels,
                    s.instances,
                    s.render,
                    swap,
                    s.update);
        s.csv.flush();
        s.events.flush();
        s.frames = s.total = s.minimum = s.maximum = 0;
        s.windowStart = Clock::now();
        // Periodic marker; retain the complete session for later analysis.
        if (s.sequence % 36000 == 0)
            Event("checkpoint", "performance-history-retained", 0);
    }

  private:
    struct Statistics
    {
        struct Query
        {
            GLuint id = 0;
            unsigned long long frame = 0;
            bool pending = false;
        };

        std::array<Query, 8> queries;
        int activeQuery = -1;
        Clock::time_point windowStart, previous, frameStart, swapStart;
        unsigned long long sequence = 0, arrays = 0, instanced = 0, pixels = 0;
        unsigned long long frames = 0, total = 0, minimum = 0, maximum = 0;
        unsigned long long instances = 0, useful = 0, submitted = 0, upload = 0;
        double interval = 0, render = 0, update = 0;
        bool started = false;
        std::ofstream csv, events;
        std::ostringstream startup;
        unsigned long long cacheHits = 0;
    };

    static Statistics& Data()
    {
        static Statistics s;
        return s;
    }
};

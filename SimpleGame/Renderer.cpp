#include "stdafx.h"
#include "Renderer.h"
#include "FrameProfiler.h"
#include "RenderStorage.h"
#include "ShaderFiles.h"
#include <cmath>
#include <algorithm>
#include <windows.h>
#pragma comment(lib, "gdi32.lib")

Renderer::Renderer(int width, int height) : m_Width(width), m_Height(height)
{
    m_Program = ShaderFiles::Program(L"Shaders/Batch.vs", L"Shaders/Batch.fs");
    if (!m_Program)
        return;
    m_Viewport = glGetUniformLocation(m_Program, "viewport");
    m_LinearScene = glGetUniformLocation(m_Program, "linearScene");
    glGenVertexArrays(1, &m_Array);
    glGenBuffers(1, &m_Buffer);
    glGenBuffers(1, &m_PoolBuffer);
    glGenTextures(1, &m_PoolTexture);
    glBindVertexArray(m_Array);
    glBindBuffer(GL_ARRAY_BUFFER, m_Buffer);
    for (GLuint i = 0; i < 5; ++i)
    {
        glEnableVertexAttribArray(i);
        glVertexAttribPointer(i,
                              4,
                              GL_FLOAT,
                              GL_FALSE,
                              sizeof(Instance),
                              reinterpret_cast<void*>(size_t(i) * 4 * sizeof(float)));
        glVertexAttribDivisor(i, 1);
    }
    glBindVertexArray(0);
    glUseProgram(m_Program);
    glUniform1i(glGetUniformLocation(m_Program, "meshPool"), 8);
    for (int i = 0; i < 8; ++i)
    {
        const auto name = "image" + std::to_string(i);
        glUniform1i(glGetUniformLocation(m_Program, name.c_str()), i);
    }
    glUseProgram(0);
    m_Batching = RenderStorage::Enabled(L"GSE_RENDER_BATCH");
    GLint maximumTexels = 0;
    glGetIntegerv(GL_MAX_TEXTURE_BUFFER_SIZE, &maximumTexels);
    m_PoolVertexLimit = (std::min)(size_t(maximumTexels / 2), size_t(524288));
    m_Instances.reserve(2048);
    m_Post.reset(new PostProcessing());
    m_Initialized = true;
    Templates();
    Resize(width, height);
}

Renderer::~Renderer()
{
    for (auto& entry : m_TextCache)
        glDeleteTextures(1, &entry.second.texture);
    m_Post.reset();
    glDeleteBuffers(1, &m_Buffer);
    glDeleteBuffers(1, &m_PoolBuffer);
    glDeleteTextures(1, &m_PoolTexture);
    glDeleteVertexArrays(1, &m_Array);
    if (m_Program)
        glDeleteProgram(m_Program);
}

void Renderer::Resize(int width, int height)
{
    Flush();
    m_Width = (std::max)(1, width);
    m_Height = (std::max)(1, height);
    glViewport(0, 0, m_Width, m_Height);
    FrameProfiler::Event("viewport", std::to_string(m_Width) + "x" + std::to_string(m_Height), 0);
    if (m_Post)
        m_Post->Resize(m_Width, m_Height);
}

void Renderer::Begin(Color c)
{
    ++m_Frame;
    m_InHDRScene = m_Post && m_Post->Begin(postProcess.enabled);
    if (m_InHDRScene)
    {
        c.r = std::pow(c.r, 2.2f);
        c.g = std::pow(c.g, 2.2f);
        c.b = std::pow(c.b, 2.2f);
    }
    glClearColor(c.r, c.g, c.b, c.a);
    glClear(GL_COLOR_BUFFER_BIT);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

void Renderer::Flush()
{
    if (m_Instances.empty() || !m_Initialized)
        return;
    glUseProgram(m_Program);
    glUniform2f(m_Viewport, float(m_Width), float(m_Height));
    glUniform1i(m_LinearScene, m_InHDRScene);
    glActiveTexture(GL_TEXTURE8);
    glBindTexture(GL_TEXTURE_BUFFER, m_PoolTexture);
    for (size_t i = 0; i < m_Textures.size(); ++i)
    {
        glActiveTexture(GL_TEXTURE0 + static_cast<GLenum>(i));
        glBindTexture(GL_TEXTURE_2D, m_Textures[i]);
    }
    glBindVertexArray(m_Array);
    glBindBuffer(GL_ARRAY_BUFFER, m_Buffer);
    glBufferData(
        GL_ARRAY_BUFFER, m_Instances.size() * sizeof(Instance), m_Instances.data(), GL_STREAM_DRAW);
    FrameProfiler::Upload(m_Instances.size() * sizeof(Instance));
    size_t useful = 0;
    for (const auto& instance : m_Instances)
        useful += size_t(instance.placement[3]);
    FrameProfiler::DrawInstanced(m_BatchVertices, static_cast<GLsizei>(m_Instances.size()), useful);
    glBindVertexArray(0);
    glUseProgram(0);
    glActiveTexture(GL_TEXTURE0);
    m_Instances.clear();
    m_Textures.clear();
    m_BatchVertices = 0;
}

void Renderer::FinishScene()
{
    Flush();
    if (m_InHDRScene)
        m_Post->Finish(postProcess);
    m_InHDRScene = false;
}

void Renderer::UploadPool()
{
    std::vector<float> data;
    for (auto& entry : m_MeshCache)
    {
        entry.second.first = static_cast<int>(data.size() / 8);
        for (const Vertex& v : entry.second.vertices)
            data.insert(data.end(), {v.x, v.y, 0, 0, v.r, v.g, v.b, v.a});
    }
    glBindBuffer(GL_TEXTURE_BUFFER, m_PoolBuffer);
    glBufferData(GL_TEXTURE_BUFFER, data.size() * sizeof(float), data.data(), GL_STATIC_DRAW);
    glActiveTexture(GL_TEXTURE8);
    glBindTexture(GL_TEXTURE_BUFFER, m_PoolTexture);
    glTexBuffer(GL_TEXTURE_BUFFER, GL_RGBA32F, m_PoolBuffer);
    glActiveTexture(GL_TEXTURE0);
    FrameProfiler::Upload(data.size() * sizeof(float));
    FrameProfiler::Event("mesh_pool_upload", "", data.size() * sizeof(float));
}

void Renderer::InstallMesh(const std::string& key, std::vector<Vertex> vertices)
{
    Flush();
    size_t totalVertices = vertices.size();
    for (const auto& entry : m_MeshCache)
        totalVertices += entry.second.vertices.size();
    while (m_MeshCache.size() >= 256 || totalVertices > m_PoolVertexLimit)
    {
        auto oldest = m_MeshCache.end();
        for (auto it = m_MeshCache.begin(); it != m_MeshCache.end(); ++it)
            if (it->first.find("unit:") != 0 &&
                (oldest == m_MeshCache.end() || it->second.lastUsed < oldest->second.lastUsed))
                oldest = it;
        if (oldest != m_MeshCache.end())
        {
            totalVertices -= oldest->second.vertices.size();
            FrameProfiler::Event(
                "mesh_evict", oldest->first, oldest->second.vertices.size() * sizeof(Vertex));
            m_MeshCache.erase(oldest);
        }
        else
        {
            FrameProfiler::Event("mesh_pool_rejected", key, vertices.size() * sizeof(Vertex));
            return;
        }
    }
    CachedMesh mesh;
    mesh.vertices = std::move(vertices);
    mesh.lastUsed = m_Frame;
    m_MeshCache.emplace(key, std::move(mesh));
    UploadPool();
}

bool Renderer::BeginCachedMesh(const std::string& key, Point origin)
{
    if (!m_Initialized || m_Capturing)
        return false;
    if (m_MeshCache.count(key))
    {
        FrameProfiler::CacheHit();
        return false;
    }
    const auto start = FrameProfiler::Clock::now();
    std::vector<Vertex> loaded;
    if (RenderStorage::Read(key, loaded))
    {
        FrameProfiler::Event("mesh_disk_hit",
                             key,
                             loaded.size() * sizeof(Vertex),
                             FrameProfiler::Milliseconds(start));
        InstallMesh(key, std::move(loaded));
        return false;
    }
    FrameProfiler::Event("mesh_disk_miss", key, 0, FrameProfiler::Milliseconds(start));
    m_Capturing = true;
    m_CaptureStart = FrameProfiler::Clock::now();
    m_CaptureKey = key;
    m_CaptureOrigin = origin;
    m_Vertices.clear();
    return true;
}

void Renderer::EndCachedMesh()
{
    if (!m_Capturing)
        return;
    m_Capturing = false;
    for (auto& v : m_Vertices)
    {
        v.x -= m_CaptureOrigin.x;
        v.y -= m_CaptureOrigin.y;
    }
    FrameProfiler::Event("mesh_build_cpu",
                         m_CaptureKey,
                         m_Vertices.size() * sizeof(Vertex),
                         FrameProfiler::Milliseconds(m_CaptureStart));
    const auto start = FrameProfiler::Clock::now();
    const bool written = RenderStorage::Write(m_CaptureKey, m_Vertices);
    FrameProfiler::Event(written ? "mesh_built_saved" : "mesh_built_unsaved",
                         m_CaptureKey,
                         m_Vertices.size() * sizeof(Vertex),
                         FrameProfiler::Milliseconds(start));
    InstallMesh(m_CaptureKey, std::move(m_Vertices));
    m_Vertices.clear();
}

void Renderer::Templates()
{
    if (BeginCachedMesh("unit:triangle", {0, 0}))
    {
        Triangle({0, 0}, {1, 0}, {0, 1}, Color(1, 1, 1));
        EndCachedMesh();
    }
    if (BeginCachedMesh("unit:quad", {0, 0}))
    {
        Rect(0, 0, 1, 1, Color(1, 1, 1));
        EndCachedMesh();
    }
    if (BeginCachedMesh("unit:circle24", {0, 0}))
    {
        Ellipse(0, 0, 1, 1, Color(1, 1, 1));
        EndCachedMesh();
    }
}

void Renderer::Submit(const std::string& key, Instance instance, GLuint texture)
{
    auto found = m_MeshCache.find(key);
    if (found == m_MeshCache.end())
        return;
    auto& mesh = found->second;
    mesh.lastUsed = m_Frame;
    const int count = static_cast<int>(mesh.vertices.size());
    // Bound padding overhead: large terrain meshes cannot inflate tiny sprite batches.
    if (!m_Instances.empty() &&
        (m_Instances.size() >= 2048 || count > (std::max)(72, m_BatchVertices) * 4 ||
         m_BatchVertices > (std::max)(72, count) * 4))
    {
        FrameProfiler::Event("batch_flush", "capacity_or_vertex_ratio", m_Instances.size());
        Flush();
    }
    int slot = -1;
    if (texture)
    {
        auto it = std::find(m_Textures.begin(), m_Textures.end(), texture);
        if (it == m_Textures.end())
        {
            if (m_Textures.size() == 8)
            {
                FrameProfiler::Event("batch_flush", "texture_limit", m_Instances.size());
                Flush();
            }
            slot = static_cast<int>(m_Textures.size());
            m_Textures.push_back(texture);
        }
        else
            slot = static_cast<int>(it - m_Textures.begin());
    }
    instance.placement[2] = float(mesh.first);
    instance.placement[3] = float(count);
    instance.material[0] = float(slot);
    m_BatchVertices = (std::max)(m_BatchVertices, count);
    m_Instances.push_back(instance);
    if (!m_Batching)
        Flush();
}

void Renderer::DrawCachedMesh(const std::string& key, Point origin)
{
    Submit(key,
           {{1, 0, 0, 1}, {origin.x, origin.y, 0, 0}, {1, 1, 1, 1}, {0, 0, 1, 1}, {-1, 0, 0, 0}});
}

void Renderer::Triangle(Point a, Point b, Point c, Color col)
{
    if (m_Capturing)
    {
        m_Vertices.push_back({a.x, a.y, col.r, col.g, col.b, col.a});
        m_Vertices.push_back({b.x, b.y, col.r, col.g, col.b, col.a});
        m_Vertices.push_back({c.x, c.y, col.r, col.g, col.b, col.a});
        return;
    }
    Submit("unit:triangle",
           {{b.x - a.x, b.y - a.y, c.x - a.x, c.y - a.y},
            {a.x, a.y, 0, 0},
            {col.r, col.g, col.b, col.a},
            {0, 0, 1, 1},
            {-1, 0, 0, 0}});
}

void Renderer::Quad(Point a, Point b, Point c, Point d, Color col)
{
    Triangle(a, b, c, col);
    Triangle(a, c, d, col);
}

void Renderer::Rect(float x, float y, float w, float h, Color c)
{
    if (m_Capturing)
    {
        Quad({x, y}, {x + w, y}, {x + w, y + h}, {x, y + h}, c);
        return;
    }
    Submit("unit:quad",
           {{w, 0, 0, h}, {x, y, 0, 0}, {c.r, c.g, c.b, c.a}, {0, 0, 1, 1}, {-1, 0, 0, 0}});
}

void Renderer::Ellipse(float x, float y, float rx, float ry, Color c)
{
    if (!m_Capturing)
    {
        Submit("unit:circle24",
               {{rx, 0, 0, ry}, {x, y, 0, 0}, {c.r, c.g, c.b, c.a}, {0, 0, 1, 1}, {-1, 0, 0, 0}});
        return;
    }
    for (int i = 0; i < 24; ++i)
    {
        float a = i * 6.2831853f / 24, b = (i + 1) * 6.2831853f / 24;
        Triangle({x, y},
                 {x + std::cos(a) * rx, y + std::sin(a) * ry},
                 {x + std::cos(b) * rx, y + std::sin(b) * ry},
                 c);
    }
}

void Renderer::Line(Point a, Point b, float w, Color c)
{
    float dx = b.x - a.x, dy = b.y - a.y, length = std::sqrt(dx * dx + dy * dy);
    if (length < .001f)
        return;
    float nx = -dy / length * w * .5f, ny = dx / length * w * .5f;
    if (m_Capturing)
        Quad({a.x + nx, a.y + ny},
             {b.x + nx, b.y + ny},
             {b.x - nx, b.y - ny},
             {a.x - nx, a.y - ny},
             c);
    else
        Submit("unit:quad",
               {{dx, dy, -2 * nx, -2 * ny},
                {a.x + nx, a.y + ny, 0, 0},
                {c.r, c.g, c.b, c.a},
                {0, 0, 1, 1},
                {-1, 0, 0, 0}});
}

void Renderer::Sprite(GLuint texture,
                      float x,
                      float y,
                      float w,
                      float h,
                      float u0,
                      float v0,
                      float u1,
                      float v1,
                      Color c,
                      float emission)
{
    Submit(
        "unit:quad",
        {{w, 0, 0, h}, {x, y, 0, 0}, {c.r, c.g, c.b, c.a}, {u0, v0, u1, v1}, {0, emission, 0, 0}},
        texture);
}

void Renderer::Text(float x, float y, const std::string& text, Color c, bool large)
{
    if (text.empty())
        return;
    const std::string key = (large ? "large:" : "normal:") + text;
    auto found = m_TextCache.find(key);
    if (found == m_TextCache.end())
    {
        // Decode UTF-8 explicitly, independent of the Windows system code page.
        int count = MultiByteToWideChar(
            CP_UTF8, MB_ERR_INVALID_CHARS, text.c_str(), static_cast<int>(text.size()), nullptr, 0);
        if (count <= 0)
            return;
        std::wstring wide(count, L'\0');
        MultiByteToWideChar(CP_UTF8,
                            MB_ERR_INVALID_CHARS,
                            text.c_str(),
                            static_cast<int>(text.size()),
                            &wide[0],
                            count);
        HDC dc = CreateCompatibleDC(nullptr);
        if (!dc)
            return;
        HFONT font = CreateFontW(large ? -20 : -15,
                                 0,
                                 0,
                                 0,
                                 large ? FW_BOLD : FW_NORMAL,
                                 FALSE,
                                 FALSE,
                                 FALSE,
                                 DEFAULT_CHARSET,
                                 OUT_DEFAULT_PRECIS,
                                 CLIP_DEFAULT_PRECIS,
                                 ANTIALIASED_QUALITY,
                                 DEFAULT_PITCH,
                                 L"Malgun Gothic");
        if (!font)
        {
            DeleteDC(dc);
            return;
        }
        HGDIOBJ oldFont = SelectObject(dc, font);
        SIZE size = {};
        TEXTMETRICW metrics = {};
        if (!GetTextExtentPoint32W(dc, wide.c_str(), count, &size) ||
            !GetTextMetricsW(dc, &metrics))
        {
            SelectObject(dc, oldFont);
            DeleteObject(font);
            DeleteDC(dc);
            return;
        }
        TextBitmap bitmap;
        bitmap.width = size.cx + 2;
        bitmap.height = metrics.tmHeight;
        bitmap.descent = metrics.tmDescent;
        BITMAPINFO info = {};
        info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        info.bmiHeader.biWidth = bitmap.width;
        // Positive height gives bottom-up pixels, matching OpenGL's raster order.
        info.bmiHeader.biHeight = bitmap.height;
        info.bmiHeader.biPlanes = 1;
        info.bmiHeader.biBitCount = 32;
        info.bmiHeader.biCompression = BI_RGB;
        void* bits = nullptr;
        HBITMAP dib = CreateDIBSection(dc, &info, DIB_RGB_COLORS, &bits, nullptr, 0);
        if (!dib)
        {
            SelectObject(dc, oldFont);
            DeleteObject(font);
            DeleteDC(dc);
            return;
        }
        HGDIOBJ oldBitmap = SelectObject(dc, dib);
        PatBlt(dc, 0, 0, bitmap.width, bitmap.height, BLACKNESS);
        SetBkMode(dc, TRANSPARENT);
        SetTextColor(dc, RGB(255, 255, 255));
        BOOL drawn = TextOutW(dc, 0, 0, wide.c_str(), count);
        GdiFlush();
        const unsigned char* source = static_cast<const unsigned char*>(bits);
        bitmap.pixels.resize(static_cast<size_t>(bitmap.width) * bitmap.height * 4);
        for (size_t i = 0; i < bitmap.pixels.size(); i += 4)
        {
            bitmap.pixels[i] = bitmap.pixels[i + 1] = bitmap.pixels[i + 2] = 255;
            bitmap.pixels[i + 3] = source[i];
        }
        SelectObject(dc, oldBitmap);
        SelectObject(dc, oldFont);
        DeleteObject(dib);
        DeleteObject(font);
        DeleteDC(dc);
        if (!drawn)
            return;
        // Bound memory use when future dialogue introduces many unique lines.
        if (m_TextCache.size() >= 128)
        {
            Flush();
            for (auto& entry : m_TextCache)
                glDeleteTextures(1, &entry.second.texture);
            m_TextCache.clear();
        }
        glGenTextures(1, &bitmap.texture);
        glBindTexture(GL_TEXTURE_2D, bitmap.texture);
        glTexImage2D(GL_TEXTURE_2D,
                     0,
                     GL_RGBA8,
                     bitmap.width,
                     bitmap.height,
                     0,
                     GL_RGBA,
                     GL_UNSIGNED_BYTE,
                     bitmap.pixels.data());
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        FrameProfiler::Upload(bitmap.pixels.size());
        bitmap.pixels.clear();
        found = m_TextCache.emplace(key, std::move(bitmap)).first;
    }
    const TextBitmap& bitmap = found->second;
    Sprite(bitmap.texture,
           float(int(x)),
           float(int(y) + bitmap.descent - bitmap.height),
           float(bitmap.width),
           float(bitmap.height),
           0,
           1,
           1,
           0,
           c);
}

void Renderer::DrawSolidRect(
    float x, float y, float, float size, float r, float g, float b, float a)
{
    Rect(m_Width * .5f + x - size * .5f,
         m_Height * .5f - y - size * .5f,
         size,
         size,
         Color(r, g, b, a));
}

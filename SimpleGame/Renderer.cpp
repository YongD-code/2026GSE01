#include "stdafx.h"
#include "Renderer.h"
#include "Dependencies/freeglut.h"
#include <cmath>
#include <cstddef>
#include <iostream>
#include <utility>
#include <windows.h>

#pragma comment(lib, "gdi32.lib")

namespace {
GLuint Shader(GLenum kind, const char* source) {
    GLuint shader = glCreateShader(kind);
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);
    GLint ok = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[2048] = {};
        glGetShaderInfoLog(shader, sizeof(log), nullptr, log);
        std::cerr << log << std::endl;
        glDeleteShader(shader);
        return 0;
    }
    return shader;
}
}
Renderer::Renderer(int width, int height) : m_Width(width), m_Height(height) {
    // Embedded shaders make startup independent of the working directory.
    const char* vs = "#version 330\nlayout(location=0) in vec2 position;\n"
        "layout(location=1) in vec4 color; out vec4 tint; uniform vec2 viewport;\n"
        "void main(){ gl_Position=vec4(position.x/viewport.x*2.-1.,"
        "1.-position.y/viewport.y*2.,0.,1.); tint=color; }";
    const char* fs = "#version 330\nin vec4 tint; out vec4 outputColor;\n"
        "uniform bool linearScene; void main(){ outputColor=vec4(linearScene ? pow(max(tint.rgb,vec3(0)),vec3(2.2)) : tint.rgb,tint.a); }";
    GLuint vertex = Shader(GL_VERTEX_SHADER, vs), fragment = Shader(GL_FRAGMENT_SHADER, fs);
    if (!vertex || !fragment) {
        if (vertex) glDeleteShader(vertex);
        if (fragment) glDeleteShader(fragment);
        return;
    }
    m_Program = glCreateProgram();
    glAttachShader(m_Program, vertex); glAttachShader(m_Program, fragment);
    glLinkProgram(m_Program);
    glDeleteShader(vertex); glDeleteShader(fragment);
    GLint ok = 0; glGetProgramiv(m_Program, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[2048] = {};
        glGetProgramInfoLog(m_Program, sizeof(log), nullptr, log);
        std::cerr << log << std::endl;
        return;
    }
    m_Viewport = glGetUniformLocation(m_Program, "viewport");
    glGenVertexArrays(1, &m_Array); glBindVertexArray(m_Array);
    glGenBuffers(1, &m_Buffer); glBindBuffer(GL_ARRAY_BUFFER, m_Buffer);
    glEnableVertexAttribArray(0); glEnableVertexAttribArray(1);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), nullptr);
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex),
        reinterpret_cast<void*>(offsetof(Vertex, r)));
    glBindVertexArray(0);
    m_LinearScene = glGetUniformLocation(m_Program, "linearScene");
    m_Post.reset(new PostProcessing());
    m_Vertices.reserve(40000);
    m_Initialized = true;
    Resize(width, height);
}
Renderer::~Renderer() {
    m_Post.reset();
    if (m_Buffer) glDeleteBuffers(1, &m_Buffer);
    if (m_Array) glDeleteVertexArrays(1, &m_Array);
    if (m_Program) glDeleteProgram(m_Program);
}
void Renderer::Resize(int width, int height) {
    m_Width = width > 0 ? width : 1; m_Height = height > 0 ? height : 1;
    glViewport(0, 0, m_Width, m_Height);
    if (m_Post) m_Post->Resize(m_Width, m_Height);
}
void Renderer::Begin(Color c) {
    m_Vertices.clear();
    m_InHDRScene = m_Post && m_Post->Begin(postProcess.enabled);
    if (m_InHDRScene) { c.r=std::pow(c.r,2.2f); c.g=std::pow(c.g,2.2f); c.b=std::pow(c.b,2.2f); }
    glClearColor(c.r, c.g, c.b, c.a);
    glClear(GL_COLOR_BUFFER_BIT);
    glDisable(GL_DEPTH_TEST); glDisable(GL_CULL_FACE);
    glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}
void Renderer::Flush() {
    if (m_Vertices.empty() || !m_Initialized) return;
    glUseProgram(m_Program); glUniform2f(m_Viewport, float(m_Width), float(m_Height));
    glUniform1i(m_LinearScene, m_InHDRScene ? 1 : 0);
    glBindVertexArray(m_Array); glBindBuffer(GL_ARRAY_BUFFER, m_Buffer);
    glBufferData(GL_ARRAY_BUFFER, m_Vertices.size() * sizeof(Vertex), m_Vertices.data(), GL_STREAM_DRAW);
    glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(m_Vertices.size()));
    glBindVertexArray(0); glUseProgram(0); m_Vertices.clear();
}
void Renderer::FinishScene() {
    Flush();
    if (m_InHDRScene) m_Post->Finish(postProcess);
    m_InHDRScene = false;
}
void Renderer::Triangle(Point a, Point b, Point c, Color col) {
    m_Vertices.push_back({a.x,a.y,col.r,col.g,col.b,col.a});
    m_Vertices.push_back({b.x,b.y,col.r,col.g,col.b,col.a});
    m_Vertices.push_back({c.x,c.y,col.r,col.g,col.b,col.a});
}
void Renderer::Quad(Point a, Point b, Point c, Point d, Color col) {
    Triangle(a,b,c,col); Triangle(a,c,d,col);
}
void Renderer::Rect(float x, float y, float w, float h, Color c) {
    Quad({x,y},{x+w,y},{x+w,y+h},{x,y+h},c);
}
void Renderer::Ellipse(float x, float y, float rx, float ry, Color c) {
    const int segments = 24;
    for (int i=0; i<segments; ++i) {
        float a = i*6.2831853f/segments, b = (i+1)*6.2831853f/segments;
        Triangle({x,y},{x+std::cos(a)*rx,y+std::sin(a)*ry},
            {x+std::cos(b)*rx,y+std::sin(b)*ry},c);
    }
}
void Renderer::Line(Point a, Point b, float w, Color c) {
    float dx=b.x-a.x, dy=b.y-a.y, length=std::sqrt(dx*dx+dy*dy);
    if (length < .001f) return;
    float nx=-dy/length*w*.5f, ny=dx/length*w*.5f;
    Quad({a.x+nx,a.y+ny},{b.x+nx,b.y+ny},{b.x-nx,b.y-ny},{a.x-nx,a.y-ny},c);
}
void Renderer::Text(float x, float y, const std::string& text, Color c, bool large) {
    if (text.empty()) return;
    const std::string key = (large ? "large:" : "normal:") + text;
    auto found = m_TextCache.find(key);
    if (found == m_TextCache.end()) {
        // Decode UTF-8 explicitly, independent of the Windows system code page.
        int count = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.c_str(),
            static_cast<int>(text.size()), nullptr, 0);
        if (count <= 0) return;
        std::wstring wide(count, L'\0');
        MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.c_str(),
            static_cast<int>(text.size()), &wide[0], count);
        HDC dc = CreateCompatibleDC(nullptr);
        if (!dc) return;
        HFONT font = CreateFontW(large ? -20 : -15, 0, 0, 0, large ? FW_BOLD : FW_NORMAL,
            FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            ANTIALIASED_QUALITY, DEFAULT_PITCH, L"Malgun Gothic");
        if (!font) { DeleteDC(dc); return; }
        HGDIOBJ oldFont = SelectObject(dc, font);
        SIZE size = {}; TEXTMETRICW metrics = {};
        if (!GetTextExtentPoint32W(dc, wide.c_str(), count, &size) || !GetTextMetricsW(dc, &metrics)) {
            SelectObject(dc, oldFont); DeleteObject(font); DeleteDC(dc); return;
        }
        TextBitmap bitmap;
        bitmap.width = size.cx + 2; bitmap.height = metrics.tmHeight;
        bitmap.descent = metrics.tmDescent;
        BITMAPINFO info = {};
        info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        info.bmiHeader.biWidth = bitmap.width;
        // Positive height gives bottom-up pixels, matching OpenGL's raster order.
        info.bmiHeader.biHeight = bitmap.height;
        info.bmiHeader.biPlanes = 1; info.bmiHeader.biBitCount = 32;
        info.bmiHeader.biCompression = BI_RGB;
        void* bits = nullptr;
        HBITMAP dib = CreateDIBSection(dc, &info, DIB_RGB_COLORS, &bits, nullptr, 0);
        if (!dib) { SelectObject(dc, oldFont); DeleteObject(font); DeleteDC(dc); return; }
        HGDIOBJ oldBitmap = SelectObject(dc, dib);
        PatBlt(dc, 0, 0, bitmap.width, bitmap.height, BLACKNESS);
        SetBkMode(dc, TRANSPARENT); SetTextColor(dc, RGB(255,255,255));
        BOOL drawn = TextOutW(dc, 0, 0, wide.c_str(), count);
        GdiFlush();
        const unsigned char* source = static_cast<const unsigned char*>(bits);
        bitmap.pixels.resize(static_cast<size_t>(bitmap.width) * bitmap.height * 4);
        for (size_t i = 0; i < bitmap.pixels.size(); i += 4) {
            bitmap.pixels[i] = bitmap.pixels[i+1] = bitmap.pixels[i+2] = 255;
            bitmap.pixels[i+3] = source[i];
        }
        SelectObject(dc, oldBitmap); SelectObject(dc, oldFont);
        DeleteObject(dib); DeleteObject(font); DeleteDC(dc);
        if (!drawn) return;
        // Bound memory use when future dialogue introduces many unique lines.
        if (m_TextCache.size() >= 128) m_TextCache.clear();
        found = m_TextCache.emplace(key, std::move(bitmap)).first;
    }
    Flush();
    const TextBitmap& bitmap = found->second;
    glUseProgram(0);
    glWindowPos2i(static_cast<int>(x), m_Height-static_cast<int>(y)-bitmap.descent);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
    glPixelTransferf(GL_RED_SCALE, c.r); glPixelTransferf(GL_GREEN_SCALE, c.g);
    glPixelTransferf(GL_BLUE_SCALE, c.b); glPixelTransferf(GL_ALPHA_SCALE, c.a);
    glDrawPixels(bitmap.width, bitmap.height, GL_RGBA, GL_UNSIGNED_BYTE, bitmap.pixels.data());
    glPixelTransferf(GL_RED_SCALE, 1); glPixelTransferf(GL_GREEN_SCALE, 1);
    glPixelTransferf(GL_BLUE_SCALE, 1); glPixelTransferf(GL_ALPHA_SCALE, 1);
}
void Renderer::DrawSolidRect(float x, float y, float, float size, float r, float g, float b, float a) {
    Rect(m_Width*.5f+x-size*.5f,m_Height*.5f-y-size*.5f,size,size,Color(r,g,b,a));
}


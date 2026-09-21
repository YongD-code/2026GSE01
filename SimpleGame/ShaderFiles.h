#pragma once
#include "Dependencies/glew.h"
#include <windows.h>
#include <iostream>
#include <string>

namespace ShaderFiles
{
inline std::string Read(const std::wstring& relative)
{
    wchar_t executable[32768] = {};
    GetModuleFileNameW(nullptr, executable, 32768);
    const std::wstring path = executable;
    const size_t slash = path.find_last_of(L"\\/");
    const std::wstring folder = slash == std::wstring::npos ? L"" : path.substr(0, slash + 1);
    const std::wstring candidates[] = {folder + relative, relative, L"SimpleGame/" + relative};
    for (const std::wstring& candidate : candidates)
    {
        HANDLE file = CreateFileW(candidate.c_str(),
                                  GENERIC_READ,
                                  FILE_SHARE_READ,
                                  nullptr,
                                  OPEN_EXISTING,
                                  FILE_ATTRIBUTE_NORMAL,
                                  nullptr);
        if (file == INVALID_HANDLE_VALUE)
            continue;
        const DWORD size = GetFileSize(file, nullptr);
        if (size == INVALID_FILE_SIZE || size == 0 || size > 1024 * 1024)
        {
            CloseHandle(file);
            continue;
        }
        std::string source(size, '\0');
        DWORD read = 0;
        const bool ok = ReadFile(file, &source[0], size, &read, nullptr) && read == size;
        CloseHandle(file);
        if (!ok)
            continue;
        if (source.compare(0, 3, "\xEF\xBB\xBF") == 0)
            source.erase(0, 3);
        return source;
    }
    std::wcerr << L"셰이더 파일을 읽을 수 없습니다: " << relative << std::endl;
    return {};
}

inline GLuint Compile(GLenum type, const std::wstring& path)
{
    const std::string source = Read(path);
    if (source.empty())
        return 0;
    const char* text = source.c_str();
    const GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &text, nullptr);
    glCompileShader(shader);
    GLint ok = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (!ok)
    {
        char log[4096] = {};
        glGetShaderInfoLog(shader, sizeof(log), nullptr, log);
        std::wcerr << L"셰이더 컴파일 실패: " << path << std::endl;
        std::cerr << log << std::endl;
        glDeleteShader(shader);
        return 0;
    }
    return shader;
}

inline GLuint Program(const std::wstring& vertexPath, const std::wstring& fragmentPath)
{
    const GLuint vertex = Compile(GL_VERTEX_SHADER, vertexPath);
    const GLuint fragment = Compile(GL_FRAGMENT_SHADER, fragmentPath);
    if (!vertex || !fragment)
    {
        if (vertex)
            glDeleteShader(vertex);
        if (fragment)
            glDeleteShader(fragment);
        return 0;
    }
    const GLuint program = glCreateProgram();
    glAttachShader(program, vertex);
    glAttachShader(program, fragment);
    glLinkProgram(program);
    glDeleteShader(vertex);
    glDeleteShader(fragment);
    GLint ok = 0;
    glGetProgramiv(program, GL_LINK_STATUS, &ok);
    if (!ok)
    {
        char log[4096] = {};
        glGetProgramInfoLog(program, sizeof(log), nullptr, log);
        std::wcerr << L"셰이더 연결 실패: " << vertexPath << L" / " << fragmentPath << std::endl;
        std::cerr << log << std::endl;
        glDeleteProgram(program);
        return 0;
    }
    return program;
}
} // namespace ShaderFiles

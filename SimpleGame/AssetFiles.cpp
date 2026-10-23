#include "stdafx.h"
#include "AssetFiles.h"
#include <windows.h>
#include <cwctype>

namespace AssetFiles
{
std::wstring Wide(const std::string& text)
{
    if (text.empty())
    {
        return {};
    }
    const int count = MultiByteToWideChar(
        CP_UTF8, MB_ERR_INVALID_CHARS, text.data(), int(text.size()), nullptr, 0);
    if (!count)
    {
        return {};
    }
    std::wstring result(count, L'\0');
    MultiByteToWideChar(
        CP_UTF8, MB_ERR_INVALID_CHARS, text.data(), int(text.size()), &result[0], count);
    return result;
}

std::string Utf8(const std::wstring& text)
{
    if (text.empty())
    {
        return {};
    }
    const int count = WideCharToMultiByte(
        CP_UTF8, WC_ERR_INVALID_CHARS, text.data(), int(text.size()), nullptr, 0, nullptr, nullptr);
    if (!count)
    {
        return {};
    }
    std::string result(count, '\0');
    WideCharToMultiByte(CP_UTF8,
                        WC_ERR_INVALID_CHARS,
                        text.data(),
                        int(text.size()),
                        &result[0],
                        count,
                        nullptr,
                        nullptr);
    return result;
}

static std::wstring Absolute(const std::wstring& path)
{
    wchar_t buffer[32768] = {};
    const DWORD count = GetFullPathNameW(path.c_str(), 32768, buffer, nullptr);
    return count && count < 32768 ? std::wstring(buffer, count) : std::wstring();
}

std::wstring Directory(const std::wstring& path)
{
    const auto at = path.find_last_of(L"/\\");
    return at == std::wstring::npos ? L"" : path.substr(0, at + 1);
}

std::wstring Local(const std::wstring& folder, const std::string& relative)
{
    const auto name = Wide(relative);
    if (name.empty() || name.front() == L'/' || name.front() == L'\\' ||
        name.find(L':') != std::wstring::npos || name.find(L'\0') != std::wstring::npos)
    {
        return {};
    }
    auto base = Absolute(folder);
    if (base.empty())
    {
        return {};
    }
    if (base.back() != L'\\')
    {
        base += L'\\';
    }
    const auto result = Absolute(base + name);
    if (result.size() <= base.size())
    {
        return {};
    }
    for (size_t i = 0; i < base.size(); ++i)
    {
        if (std::towlower(base[i]) != std::towlower(result[i]))
        {
            return {};
        }
    }
    return result;
}

std::wstring Resolve(const std::wstring& relative)
{
    wchar_t executable[32768] = {};
    GetModuleFileNameW(nullptr, executable, 32768);
    const std::wstring roots[] = {Directory(executable), Absolute(L".") + L"\\"};
    for (auto root : roots)
    {
        for (int parent = 0; parent < 7 && !root.empty(); ++parent)
        {
            const auto candidate = Local(root, Utf8(relative));
            const DWORD attributes =
                candidate.empty() ? INVALID_FILE_ATTRIBUTES : GetFileAttributesW(candidate.c_str());
            if (attributes != INVALID_FILE_ATTRIBUTES && !(attributes & FILE_ATTRIBUTE_DIRECTORY))
            {
                return candidate;
            }
            const auto trimmed = root.substr(0, root.size() - 1);
            const auto next = Directory(trimmed);
            if (next == root)
            {
                break;
            }
            root = next;
        }
    }
    return {};
}

bool Read(const std::wstring& path, std::vector<unsigned char>& bytes, size_t limit)
{
    HANDLE file = CreateFileW(path.c_str(),
                              GENERIC_READ,
                              FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                              nullptr,
                              OPEN_EXISTING,
                              FILE_ATTRIBUTE_NORMAL,
                              nullptr);
    if (file == INVALID_HANDLE_VALUE)
    {
        return false;
    }
    LARGE_INTEGER size = {};
    bool success = GetFileSizeEx(file, &size) && size.QuadPart > 0 &&
                   static_cast<unsigned long long>(size.QuadPart) <= limit &&
                   size.QuadPart <= MAXDWORD;
    if (success)
    {
        bytes.resize(static_cast<size_t>(size.QuadPart));
        DWORD read = 0;
        success = ReadFile(file, bytes.data(), DWORD(bytes.size()), &read, nullptr) &&
                  read == bytes.size();
    }
    CloseHandle(file);
    if (!success)
    {
        bytes.clear();
    }
    return success;
}
} // namespace AssetFiles

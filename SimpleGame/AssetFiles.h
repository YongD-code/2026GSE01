#pragma once
#include <string>
#include <vector>
#include <cstddef>

// UTF-8 asset names on disk must also work below Korean Windows project paths.
namespace AssetFiles
{
std::wstring Wide(const std::string& text);
std::string Utf8(const std::wstring& text);
std::wstring Resolve(const std::wstring& relative);
std::wstring Directory(const std::wstring& path);
std::wstring Local(const std::wstring& folder, const std::string& relative);
bool Read(const std::wstring& path, std::vector<unsigned char>& bytes, size_t limit);
} // namespace AssetFiles

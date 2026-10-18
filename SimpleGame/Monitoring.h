#pragma once
#include <windows.h>
#include <winhttp.h>
#pragma comment(lib, "winhttp.lib")
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <locale>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>

namespace Monitoring
{
inline std::string Quote(const std::string& value)
{
    static const char hex[] = "0123456789abcdef";
    std::string result = "\"";
    for (unsigned char c : value)
    {
        if (c == '"' || c == '\\')
        {
            result += '\\';
            result += char(c);
        }
        else if (c < 32)
        {
            result += "\\u00";
            result += hex[c >> 4];
            result += hex[c & 15];
        }
        else
        {
            result += char(c);
        }
    }
    return result + '"';
}

inline long long Timestamp()
{
    return std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::system_clock::now().time_since_epoch())
        .count();
}

inline std::string Utf8(const std::wstring& text)
{
    const int size = WideCharToMultiByte(
        CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0, nullptr, nullptr);
    std::string result(size, '\0');
    if (size)
    {
        WideCharToMultiByte(CP_UTF8,
                            0,
                            text.data(),
                            static_cast<int>(text.size()),
                            &result[0],
                            size,
                            nullptr,
                            nullptr);
    }
    return result;
}

struct Config
{
    std::wstring host = L"127.0.0.1", token;
    std::string gameId = "last-ember", gameName = "주흔 — 잊힌 이름", sourceId;
    INTERNET_PORT port = 18765;
    bool enabled = true, https = false;

    Config()
    {
        wchar_t executable[32768] = {};
        GetModuleFileNameW(nullptr, executable, 32768);
        const std::wstring path = executable;
        const std::wstring ini =
            path.substr(0, path.find_last_of(L"\\/") + 1) + L"Data/Monitoring.ini";
        auto read = [&](const wchar_t* key, const wchar_t* fallback)
        {
            wchar_t buffer[512] = {};
            GetPrivateProfileStringW(L"monitoring", key, fallback, buffer, 512, ini.c_str());
            return std::wstring(buffer);
        };
        host = read(L"host", L"127.0.0.1");
        token = read(L"token", L"");
        const UINT configuredPort =
            GetPrivateProfileIntW(L"monitoring", L"port", 18765, ini.c_str());
        if (configuredPort > 0 && configuredPort <= 65535)
        {
            port = static_cast<INTERNET_PORT>(configuredPort);
        }
        enabled = GetPrivateProfileIntW(L"monitoring", L"enabled", 1, ini.c_str()) != 0;
        https = GetPrivateProfileIntW(L"monitoring", L"https", 0, ini.c_str()) != 0;
        gameId = Utf8(read(L"game_id", L"last-ember"));
        wchar_t computer[256] = {};
        DWORD size = 256;
        GetComputerNameW(computer, &size);
        sourceId = Utf8(read(L"source_id", computer)) + "-" + std::to_string(GetCurrentProcessId());
    }
};

// One latest snapshot, no growing send queue. Network calls never run on the game thread.
class Publisher
{
    const Config config;
    std::mutex mutex;
    std::condition_variable ready;
    std::string pending;
    bool stopping = false;
    std::thread worker;

    void Run()
    {
        if (!config.enabled)
        {
            return;
        }
        HINTERNET session = WinHttpOpen(L"GameMonitor/1.0",
                                        WINHTTP_ACCESS_TYPE_NO_PROXY,
                                        WINHTTP_NO_PROXY_NAME,
                                        WINHTTP_NO_PROXY_BYPASS,
                                        0);
        if (!session)
        {
            return;
        }
        WinHttpSetTimeouts(session, 1000, 1000, 1000, 1000);
        HINTERNET connection = WinHttpConnect(session, config.host.c_str(), config.port, 0);
        bool lastFailed = false;
        for (;;)
        {
            std::string message;
            {
                std::unique_lock<std::mutex> lock(mutex);
                ready.wait(lock,
                           [this]
                           {
                               return stopping || !pending.empty();
                           });
                if (pending.empty() && stopping)
                {
                    break;
                }
                message.swap(pending);
            }
            HINTERNET request = connection
                                    ? WinHttpOpenRequest(connection,
                                                         L"POST",
                                                         L"/api/snapshots",
                                                         nullptr,
                                                         WINHTTP_NO_REFERER,
                                                         WINHTTP_DEFAULT_ACCEPT_TYPES,
                                                         config.https ? WINHTTP_FLAG_SECURE : 0)
                                    : nullptr;
            DWORD status = 0, size = sizeof(status);
            if (request)
            {
                const DWORD redirects = WINHTTP_OPTION_REDIRECT_POLICY_NEVER;
                WinHttpSetOption(request,
                                 WINHTTP_OPTION_REDIRECT_POLICY,
                                 const_cast<DWORD*>(&redirects),
                                 sizeof(redirects));
                std::wstring headers = L"Content-Type: application/json; charset=utf-8\r\n";
                if (!config.token.empty())
                {
                    headers += L"Authorization: Bearer " + config.token + L"\r\n";
                }
                if (WinHttpSendRequest(request,
                                       headers.c_str(),
                                       static_cast<DWORD>(-1),
                                       &message[0],
                                       static_cast<DWORD>(message.size()),
                                       static_cast<DWORD>(message.size()),
                                       0) &&
                    WinHttpReceiveResponse(request, nullptr))
                {
                    WinHttpQueryHeaders(request,
                                        WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                                        WINHTTP_HEADER_NAME_BY_INDEX,
                                        &status,
                                        &size,
                                        WINHTTP_NO_HEADER_INDEX);
                }
                WinHttpCloseHandle(request);
            }
            const bool failed = status != 202;
            if (failed && !lastFailed)
            {
                OutputDebugStringW(
                    L"Monitoring: collector unavailable; retrying with newest snapshot.\n");
            }
            lastFailed = failed;
        }
        if (connection)
        {
            WinHttpCloseHandle(connection);
        }
        WinHttpCloseHandle(session);
    }

  public:
    Publisher()
        : worker(
              [this]
              {
                  Run();
              })
    {
    }

    const Config& Settings() const
    {
        return config;
    }

    ~Publisher()
    {
        {
            std::lock_guard<std::mutex> lock(mutex);
            stopping = true;
        }
        ready.notify_one();
        worker.join();
    }

    void Submit(std::string message)
    {
        if (!config.enabled)
        {
            return;
        }
        {
            std::lock_guard<std::mutex> lock(mutex);
            pending = std::move(message);
        }
        ready.notify_one();
    }
};
} // namespace Monitoring

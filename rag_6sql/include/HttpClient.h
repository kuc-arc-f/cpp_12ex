#pragma once
// ============================================================
//  HttpClient.h  -  Windows WinHTTP ラッパークラス
//  Visual Studio 2019/2022  (C++17)
//  リンク設定: プロジェクト → リンカー → 追加の依存ファイル → winhttp.lib
// ============================================================

#include <windows.h>
#include <winhttp.h>
#include <string>
#include <vector>
#include <map>
#include <stdexcept>
#include <sstream>

#pragma comment(lib, "winhttp.lib")

// -------------------------------------------------------
//  HttpResponse  ─ レスポンスデータ保持構造体
// -------------------------------------------------------
struct HttpResponse
{
    int                                statusCode = 0;
    std::wstring                       statusText;
    std::map<std::wstring,std::wstring> headers;
    std::string                        body;       // UTF-8 / バイナリ
};

// -------------------------------------------------------
//  HttpClient  ─ シンプル HTTP/HTTPS クライアント
// -------------------------------------------------------
class HttpClient
{
public:
    // コンストラクタ
    // userAgent : User-Agent 文字列
    // timeoutMs : 送受信タイムアウト (ミリ秒)
    explicit HttpClient(const std::wstring& userAgent = L"WinHTTP-Sample/1.0",
                        DWORD timeoutMs = 60000)
        : m_hSession(nullptr), m_timeoutMs(timeoutMs)
    {
        m_hSession = ::WinHttpOpen(
            userAgent.c_str(),
            WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
            WINHTTP_NO_PROXY_NAME,
            WINHTTP_NO_PROXY_BYPASS,
            0);

        if (!m_hSession)
            throw std::runtime_error("WinHttpOpen failed: " + GetLastErrorStr());
    }

    ~HttpClient()
    {
        if (m_hSession) ::WinHttpCloseHandle(m_hSession);
    }

    // コピー禁止
    HttpClient(const HttpClient&)            = delete;
    HttpClient& operator=(const HttpClient&) = delete;

    // -------------------------------------------------------
    //  GET リクエスト
    // -------------------------------------------------------
    HttpResponse Get(const std::wstring& url,
                     const std::map<std::wstring,std::wstring>& extraHeaders = {})
    {
        return Request(L"GET", url, {}, extraHeaders);
    }

    // -------------------------------------------------------
    //  POST リクエスト
    // -------------------------------------------------------
    HttpResponse Post(const std::wstring& url,
                      const std::string&  body,
                      const std::wstring& contentType = L"application/json",
                      const std::map<std::wstring,std::wstring>& extraHeaders = {})
    {
        auto headers = extraHeaders;
        headers[L"Content-Type"] = contentType;
        return Request(L"POST", url, body, headers);
    }

    // -------------------------------------------------------
    //  PUT リクエスト
    // -------------------------------------------------------
    HttpResponse Put(const std::wstring& url,
                     const std::string&  body,
                     const std::wstring& contentType = L"application/json",
                     const std::map<std::wstring,std::wstring>& extraHeaders = {})
    {
        auto headers = extraHeaders;
        headers[L"Content-Type"] = contentType;
        return Request(L"PUT", url, body, headers);
    }

    // -------------------------------------------------------
    //  DELETE リクエスト
    // -------------------------------------------------------
    HttpResponse Delete(const std::wstring& url,
                        const std::map<std::wstring,std::wstring>& extraHeaders = {})
    {
        return Request(L"DELETE", url, {}, extraHeaders);
    }

    // -------------------------------------------------------
    //  汎用リクエスト
    // -------------------------------------------------------
    HttpResponse Request(const std::wstring& method,
                         const std::wstring& url,
                         const std::string&  body    = {},
                         const std::map<std::wstring,std::wstring>& extraHeaders = {})
    {
        // URL を解析
        URL_COMPONENTS uc = {};
        uc.dwStructSize   = sizeof(uc);
        wchar_t scheme[16]   = {};
        wchar_t host[256]    = {};
        wchar_t path[2048]   = {};
        uc.lpszScheme        = scheme;  uc.dwSchemeLength    = _countof(scheme);
        uc.lpszHostName      = host;    uc.dwHostNameLength  = _countof(host);
        uc.lpszUrlPath       = path;    uc.dwUrlPathLength   = _countof(path);

        if (!::WinHttpCrackUrl(url.c_str(), 0, 0, &uc))
            throw std::runtime_error("WinHttpCrackUrl failed: " + GetLastErrorStr());

        bool isHttps = (uc.nScheme == INTERNET_SCHEME_HTTPS);

        // 接続ハンドル
        HINTERNET hConnect = ::WinHttpConnect(m_hSession, host, uc.nPort, 0);
        if (!hConnect)
            throw std::runtime_error("WinHttpConnect failed: " + GetLastErrorStr());

        HandleGuard hcGuard(hConnect);

        // リクエストハンドル
        DWORD flags = isHttps ? WINHTTP_FLAG_SECURE : 0;
        HINTERNET hRequest = ::WinHttpOpenRequest(
            hConnect, method.c_str(), path,
            nullptr, WINHTTP_NO_REFERER,
            WINHTTP_DEFAULT_ACCEPT_TYPES, flags);
        if (!hRequest)
            throw std::runtime_error("WinHttpOpenRequest failed: " + GetLastErrorStr());

        HandleGuard hrGuard(hRequest);

        // タイムアウト設定
        ::WinHttpSetTimeouts(hRequest, m_timeoutMs, m_timeoutMs, m_timeoutMs, m_timeoutMs);

        // ヘッダー追加
        for (auto& [k, v] : extraHeaders)
        {
            std::wstring hdrLine = k + L": " + v + L"\r\n";
            ::WinHttpAddRequestHeaders(hRequest, hdrLine.c_str(), (DWORD)-1L,
                                       WINHTTP_ADDREQ_FLAG_ADD | WINHTTP_ADDREQ_FLAG_REPLACE);
        }

        // リクエスト送信
        BOOL sent = ::WinHttpSendRequest(
            hRequest,
            WINHTTP_NO_ADDITIONAL_HEADERS, 0,
            body.empty() ? nullptr : (LPVOID)body.data(),
            (DWORD)body.size(),
            (DWORD)body.size(),
            0);
        if (!sent)
            throw std::runtime_error("WinHttpSendRequest failed: " + GetLastErrorStr());

        if (!::WinHttpReceiveResponse(hRequest, nullptr))
            throw std::runtime_error("WinHttpReceiveResponse failed: " + GetLastErrorStr());

        // レスポンス構築
        HttpResponse resp;
        resp.statusCode = QueryStatusCode(hRequest);
        resp.statusText = QueryStatusText(hRequest);
        resp.headers    = QueryResponseHeaders(hRequest);
        resp.body       = ReadBody(hRequest);
        return resp;
    }

private:
    HINTERNET m_hSession;
    DWORD     m_timeoutMs;

    // HINTERNET RAII ガード
    struct HandleGuard
    {
        HINTERNET h;
        explicit HandleGuard(HINTERNET h_) : h(h_) {}
        ~HandleGuard() { if (h) ::WinHttpCloseHandle(h); }
    };

    static int QueryStatusCode(HINTERNET hReq)
    {
        DWORD code = 0, size = sizeof(code);
        ::WinHttpQueryHeaders(hReq,
            WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
            WINHTTP_HEADER_NAME_BY_INDEX, &code, &size, WINHTTP_NO_HEADER_INDEX);
        return static_cast<int>(code);
    }

    static std::wstring QueryStatusText(HINTERNET hReq)
    {
        DWORD size = 0;
        ::WinHttpQueryHeaders(hReq, WINHTTP_QUERY_STATUS_TEXT,
            WINHTTP_HEADER_NAME_BY_INDEX, nullptr, &size, WINHTTP_NO_HEADER_INDEX);
        std::wstring txt(size / sizeof(wchar_t), L'\0');
        ::WinHttpQueryHeaders(hReq, WINHTTP_QUERY_STATUS_TEXT,
            WINHTTP_HEADER_NAME_BY_INDEX, txt.data(), &size, WINHTTP_NO_HEADER_INDEX);
        // 末尾のヌル文字を除去
        while (!txt.empty() && txt.back() == L'\0') txt.pop_back();
        return txt;
    }

    static std::map<std::wstring,std::wstring> QueryResponseHeaders(HINTERNET hReq)
    {
        std::map<std::wstring,std::wstring> result;
        DWORD size = 0;
        ::WinHttpQueryHeaders(hReq, WINHTTP_QUERY_RAW_HEADERS_CRLF,
            WINHTTP_HEADER_NAME_BY_INDEX, nullptr, &size, WINHTTP_NO_HEADER_INDEX);

        std::wstring raw(size / sizeof(wchar_t), L'\0');
        ::WinHttpQueryHeaders(hReq, WINHTTP_QUERY_RAW_HEADERS_CRLF,
            WINHTTP_HEADER_NAME_BY_INDEX, raw.data(), &size, WINHTTP_NO_HEADER_INDEX);

        std::wistringstream ss(raw);
        std::wstring line;
        while (std::getline(ss, line))
        {
            auto pos = line.find(L':');
            if (pos == std::wstring::npos) continue;
            auto key = line.substr(0, pos);
            auto val = line.substr(pos + 1);
            // 先頭の空白・末尾の \r を除去
            if (!val.empty() && val.front() == L' ') val.erase(0, 1);
            if (!val.empty() && val.back()  == L'\r') val.pop_back();
            result[key] = val;
        }
        return result;
    }

    static std::string ReadBody(HINTERNET hReq)
    {
        std::string body;
        DWORD available = 0;
        while (::WinHttpQueryDataAvailable(hReq, &available) && available > 0)
        {
            std::vector<char> buf(available);
            DWORD read = 0;
            if (::WinHttpReadData(hReq, buf.data(), available, &read))
                body.append(buf.data(), read);
        }
        return body;
    }

    static std::string GetLastErrorStr()
    {
        return "error code " + std::to_string(::GetLastError());
    }
};

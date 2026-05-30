#pragma once
#include <windows.h>
#include <winhttp.h>
#include <iostream>
#include <string>
#include <vector>
#include <cstdlib>
#include <nlohmann/json.hpp> // nlohmann/jsonが必要です

#pragma comment(lib, "winhttp.lib")

using json = nlohmann::json;


// UTF-8文字列をワイド文字列(LPCWSTR)に変換するヘルパー
std::wstring Utf8ToWide(const std::string& utf8) {
    if (utf8.empty()) return L"";
    int len = MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, nullptr, 0);
    std::wstring wide(len, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, &wide[0], len);
    if (!wide.empty() && wide.back() == L'\0') wide.pop_back();
    return wide;
}

class MyRag {
private:
    std::string m_name;

public:
    explicit MyRag(std::string str){}

    ~MyRag() {}

    // Gemini API からテキスト生成結果を取得する関数
    std::string GenerateContent(const std::string& apiKey, const std::string& prompt) {
        // 1. リクエストボディのJSON構築
        json reqBody;
        reqBody["contents"] = json::array({
            {{"parts", json::array({{{"text", prompt}}})}}
        });
        std::string bodyStr = reqBody.dump();

        // 2. WinHTTP セッション開始
        HINTERNET hSession = WinHttpOpen(L"GeminiCppClient/1.0",
                                        WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                                        WINHTTP_NO_PROXY_NAME,
                                        WINHTTP_NO_PROXY_BYPASS, 0);
        if (!hSession) { std::cerr << "WinHttpOpen failed\n"; return ""; }

        // 3. 接続先ホストへ接続
        HINTERNET hConnect = WinHttpConnect(hSession,
                                            L"generativelanguage.googleapis.com",
                                            INTERNET_DEFAULT_HTTPS_PORT, 0);
        if (!hConnect) { WinHttpCloseHandle(hSession); return ""; }

        // 4. リクエスト作成 (モデル名は適宜変更してください)
        const wchar_t* path = L"/v1beta/models/gemma-4-31b-it:generateContent";
        HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"POST", path,
                                                nullptr, WINHTTP_NO_REFERER,
                                                WINHTTP_DEFAULT_ACCEPT_TYPES,
                                                WINHTTP_FLAG_SECURE);
        if (!hRequest) {
            WinHttpCloseHandle(hConnect);
            WinHttpCloseHandle(hSession);
            return "";
        }

        // 5. ヘッダー設定
        std::wstring authHeader = L"x-goog-api-key: " + Utf8ToWide(apiKey);
        WinHttpAddRequestHeaders(hRequest, authHeader.c_str(), -1L, WINHTTP_ADDREQ_FLAG_ADD);
        WinHttpAddRequestHeaders(hRequest, L"Content-Type: application/json", -1L, WINHTTP_ADDREQ_FLAG_ADD);

        // 6. リクエスト送信
        BOOL bResult = WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                                        (LPVOID)bodyStr.c_str(), (DWORD)bodyStr.size(),
                                        (DWORD)bodyStr.size(), 0);
        if (!bResult || !WinHttpReceiveResponse(hRequest, nullptr)) {
            std::cerr << "Request failed. Error: " << GetLastError() << "\n";
            WinHttpCloseHandle(hRequest);
            WinHttpCloseHandle(hConnect);
            WinHttpCloseHandle(hSession);
            return "";
        }

        // 7. レスポンス読み取り
        std::string response;
        DWORD dwSize = 0;
        do {
            dwSize = 0;
            if (!WinHttpQueryDataAvailable(hRequest, &dwSize)) break;
            if (dwSize == 0) break;

            std::vector<char> buffer(dwSize);
            DWORD dwRead = 0;
            if (WinHttpReadData(hRequest, buffer.data(), dwSize, &dwRead)) {
                response.append(buffer.data(), dwRead);
            }
        } while (dwSize > 0);

        // ハンドルクローズ
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);

        // 8. JSONパースして生成テキストを抽出
        try {
            auto resJson = json::parse(response);
            // Gemini APIのレスポンス構造: candidates[0].content.parts[0].text
            return resJson["candidates"][0]["content"]["parts"][0]["text"].get<std::string>();
        } catch (const std::exception& e) {
            std::cerr << "JSON Parse Error: " << e.what() << "\n";
            std::cerr << "Raw Response: " << response << "\n";
            return "";
        }
    }

};

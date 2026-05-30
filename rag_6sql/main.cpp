// ============================================================
//  MyWebViewApp - C++ / LLVM Clang / WebView2 Desktop App
//  ローカルHTMLファイルを表示するデスクトップアプリ
// ============================================================

#ifndef UNICODE
#define UNICODE
#endif

#include <windows.h>
#include <wrl.h>
#include <wil/com.h>
#include <WebView2.h>

#include <cpr/cpr.h>
#include <iostream>
#include <fstream>
#include <filesystem>
#include <mutex>
#include <vector>
#include <sstream>
#include <string>
#include <stdexcept>
#include <shlwapi.h>
#include <thread>

#include "nlohmann/json.hpp"
#include "dotenv.h"
#include "include/HttpClient.h"
#include "db_search.hpp"

// JSON用エイリアス
using json = nlohmann::json;
#pragma comment(lib, "shlwapi.lib")
using namespace Microsoft::WRL;

// ── グローバル変数 ─────────────────────────────────────────
static std::mutex        g_mutex;
static HWND                                    g_hWnd       = nullptr;
static wil::com_ptr<ICoreWebView2Controller>   g_controller;
static wil::com_ptr<ICoreWebView2>             g_webview;

const std::string DB_PATH = "example.db";
const std::wstring API_URL_CHAT = L"http://localhost:8090/v1/chat/completions";

// ── ウィンドウタイトル / クラス名 ─────────────────────────
static constexpr wchar_t APP_TITLE[]     = L"MyWebViewApp";
static constexpr wchar_t WND_CLASS[]     = L"MyWebViewAppClass";

// ── HTMLファイルのパスを取得 ──────────────────────────────
static std::wstring GetHtmlPath()
{
    wchar_t exePath[MAX_PATH] = {};
    GetModuleFileNameW(nullptr, exePath, MAX_PATH);
    PathRemoveFileSpecW(exePath);               // 実行ファイルのディレクトリ

    std::wstring path = exePath;
    path += L"\\html\\index.html";
    return path;
}

std::wstring StringToWString(const std::string& str)
{
    if (str.empty()) return L"";

    int size_needed = MultiByteToWideChar(
        CP_UTF8, 0,
        str.c_str(), (int)str.size(),
        NULL, 0
    );

    std::wstring wstr(size_needed, 0);

    MultiByteToWideChar(
        CP_UTF8, 0,
        str.c_str(), (int)str.size(),
        &wstr[0], size_needed
    );

    return wstr;
}
std::string to_utf8(const std::wstring& wstr) {
    if (wstr.empty()) return std::string();
    int size_needed = WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), NULL, 0, NULL, NULL);
    std::string strTo(size_needed, 0);
    WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), &strTo[0], size_needed, NULL, NULL);
    return strTo;
}

struct ActionRequest {
    std::string action;
    std::string data;
};
struct ActionResponse {
    int ret;
    std::string data;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(ActionResponse, ret, data)

struct QueryReq {
    std::string input;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(QueryReq, input)

struct ChatQuery {
    std::string role;
    std::string content;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(ChatQuery, role, content)

struct ChatRequest {
    std::string model;
    std::vector<ChatQuery> messages;
    double temperature;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(ChatRequest, model, messages, temperature)

std::string extractContent(const std::string& jsonStr)
{
    try {
        auto j = nlohmann::json::parse(jsonStr);
        return j["choices"][0]["message"]["content"].get<std::string>();
    } catch (const std::exception& e) {
        std::cerr << "[ERROR] JSON parse: " << e.what() << "\n";
        return "";
    }
}

std::string send_chat(std::string query) {
    std::string ret = "";
    ChatQuery req2;
    req2.role = "user";
    req2.content = query;
    json j2 = req2;
    std::string json_str2 = j2.dump();
    std::wstring w_str2 = StringToWString(json_str2);
    //std::wcout << L"json_str2:" << w_str2 << std::endl;
    std::vector<ChatQuery> chat_messages;
    chat_messages.push_back(req2);

    std::string target_msg = "[";
    target_msg.append(json_str2);
    target_msg.append("]");
    ChatRequest req3;
    req3.model = "local-model";
    req3.messages = chat_messages;
    req3.temperature = 0.7;
    json j3 = req3; // 構造体を代入するだけ！
    std::string json_str3 = j3.dump();
    std::wstring w_str3 = StringToWString(json_str3);
    //std::wcout << L"json_str3:" << w_str3 << std::endl;
    HttpClient client;

    auto resp2 = client.Post(
        API_URL_CHAT,
        json_str3,
        L"application/json");

    if (resp2.statusCode == 200) {
        std::wcout << L"resp.statusCode=200 \n\n";
        std::string reply = extractContent(resp2.body);
        return reply;
    }
    return ret;
}

// Gemini Embedding API から埋め込みベクトルを取得する関数
std::vector<float> getGeminiEmbedding(const std::string& apiKey, const std::string& text) {
    std::vector<float> ret;
    const std::string url = "https://generativelanguage.googleapis.com/v1beta/models/gemini-embedding-001:embedContent";
    
    // リクエストボディの構築
    json requestBody = {
        {"model", "models/gemini-embedding-001"},
        {"content", {
            {"parts", {{{"text", text}}}}
        }}
    };

    cpr::Response r = cpr::Post(
        cpr::Url{url},
        cpr::Header{{"Content-Type", "application/json"}, 
                    {"x-goog-api-key", apiKey}},
        cpr::Body{requestBody.dump()}
    );

    // レスポンスのパースと embedding 配列の抽出
    try {
        if (r.status_code != 200) {
            std::wcout << "error , status_code:" << r.status_code << std::endl;
            throw std::runtime_error("API request failed with status ");
        }

        json response = json::parse(r.text);
        std::vector<float> embedding = response["embedding"]["values"]
                                               .get<std::vector<float>>();
        return embedding;
    } catch (const json::exception& e) {
        //throw std::runtime_error(std::string("JSON parse error: ") + e.what() + 
        //                         "\nRaw response: " + r.text);
        std::wcout << e.what() << std::endl;
        return ret;
    }
}
/**
*
* @param
*
* @return
*/
std::string rag_search(std::string query, std::string api_key) {
    std::string ret = "";
    try {
        auto embedding = getGeminiEmbedding(api_key, query);
        std::wcout << L"Embedding dimensions: " << embedding.size() << std::endl;
        DbSearch db_helper(DB_PATH);
        if(db_helper.search_embed_size(embedding) == false){
            return "error , Embedding size mismatch";
        }        
        std::string resp_str = db_helper.rag_search(embedding);            
        std::string out_str = "日本語で、回答して欲しい。 \n要約して欲しい。\n\n";
        if(resp_str.empty()){
            out_str.append("user query: ");
            out_str.append(query);
            out_str.append(" \n");
        }else{
            out_str.append("context:");
            out_str.append(resp_str);
            out_str.append("\n user query: ");
            out_str.append(query);
            out_str.append(" \n");
        }
        ret = send_chat(out_str);
        return ret;
    }
    catch (const std::exception& e) {
        //std::wcerr << L"\n[ERROR] " << e.what() << L"\n";
    }
    return ret;
}

std::wstring action_handler(const std::wstring& data) {
    ActionResponse resp;
    try {    
        resp.ret = 500;
        dotenv env(".env");
        std::string api_key = env.get("GEMINI_API_KEY", "key123");
        //std::cout << "api_key: " << api_key << std::endl;        
        std::string data_u8 = to_utf8(data);
        json j1 = json::parse(data_u8);
        std::string action = j1.at("action").get<std::string>();
        if (action == "search") {
            std::string data_str = j1.at("data").get<std::string>();
            json j3 = json::parse(data_str);
            std::string query = j3.at("query").get<std::string>();

            std::string search_result = rag_search(query, api_key);
            std::string body = search_result;
            resp.data = body;
            resp.ret = 200;
            json j2 = resp;
            std::string json_str = j2.dump();
            std::wstring resp_wstr = StringToWString(json_str);
            return resp_wstr;
        }
        return L"";
    } catch (const std::exception& ex) {
        //std::wcerr << ex.what() << std::endl;
        return L"";
    }
}

// ── WebView2 の初期化 ─────────────────────────────────────
static void InitWebView2(HWND hWnd)
{
    // ユーザーデータフォルダ（実行ファイルと同じ場所に作成）
    wchar_t exePath[MAX_PATH] = {};
    GetModuleFileNameW(nullptr, exePath, MAX_PATH);
    PathRemoveFileSpecW(exePath);
    std::wstring userDataFolder = std::wstring(exePath) + L"\\WebView2Data";

    HRESULT hr = CreateCoreWebView2EnvironmentWithOptions(
        nullptr,                    // ブラウザ実行可能フォルダ (nullptr = 自動検索)
        userDataFolder.c_str(),     // ユーザーデータフォルダ
        nullptr,                    // 追加オプション
        Callback<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler>(
            [hWnd](HRESULT result, ICoreWebView2Environment* env) -> HRESULT
            {
                if (FAILED(result) || !env)
                {
                    MessageBoxW(hWnd,
                        L"WebView2 ランタイムが見つかりません。\n"
                        L"Microsoft Edge WebView2 Runtime をインストールしてください。\n"
                        L"https://developer.microsoft.com/microsoft-edge/webview2/",
                        L"エラー", MB_ICONERROR | MB_OK);
                    PostQuitMessage(1);
                    return result;
                }

                env->CreateCoreWebView2Controller(
                    hWnd,
                    Callback<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler>(
                        [hWnd](HRESULT result, ICoreWebView2Controller* controller) -> HRESULT
                        {
                            if (FAILED(result) || !controller)
                            {
                                MessageBoxW(hWnd, L"WebView2 コントローラの作成に失敗しました。",
                                    L"エラー", MB_ICONERROR | MB_OK);
                                PostQuitMessage(1);
                                return result;
                            }

                            g_controller = controller;
                            g_controller->get_CoreWebView2(&g_webview);

                            // ── WebView2 設定 ──────────────────────
                            wil::com_ptr<ICoreWebView2Settings> settings;
                            g_webview->get_Settings(&settings);
                            if (settings)
                            {
                                settings->put_IsScriptEnabled(TRUE);
                                settings->put_AreDefaultContextMenusEnabled(TRUE);
                                settings->put_IsZoomControlEnabled(TRUE);
                                settings->put_AreDevToolsEnabled(TRUE);   // 開発中はTRUE
                            }

                            // ── ウィンドウサイズに合わせてリサイズ ──
                            RECT bounds;
                            GetClientRect(hWnd, &bounds);
                            g_controller->put_Bounds(bounds);
                            g_controller->put_IsVisible(TRUE);

                            // ── ローカルHTMLファイルをロード ────────
                            std::wstring htmlPath = GetHtmlPath();
                            // file:/// URI に変換
                            std::wstring uri = L"file:///" + htmlPath;
                            // バックスラッシュをスラッシュに変換
                            for (auto& c : uri) if (c == L'\\') c = L'/';

                            g_webview->Navigate(uri.c_str());

                            // ── JS からメッセージを受信した時の処理 ───────
                            g_webview->add_WebMessageReceived(
                                Callback<ICoreWebView2WebMessageReceivedEventHandler>(
                                    [](ICoreWebView2* sender,
                                       ICoreWebView2WebMessageReceivedEventArgs* args) -> HRESULT
                                    {
                                        wil::unique_cotaskmem_string message;
                                        args->TryGetWebMessageAsString(&message);

                                        if (message)
                                        {
                                            std::wstring msgStr = message.get();
                                            
                                            // 受信した文字を確認（デバッグ用）
                                            std::string data_u8 = to_utf8(msgStr);
                                            json j1 = json::parse(data_u8);
                                            std::string data_str = j1.at("data").get<std::string>();
                                            std::wstring data_str_w = StringToWString(data_str);
                                            //MessageBoxW(g_hWnd, data_str_w.c_str(), L"C++ 受信", MB_OK);
                                            // 重い処理は別スレッド
                                            std::thread([msgStr]() {
                                                //std::this_thread::sleep_for(
                                                //    std::chrono::seconds(5));
                                                auto resp = action_handler(msgStr);

                                                std::wstring result = resp;

                                                // WebView2操作はUIスレッドへ戻す
                                                 PostMessage(
                                                     g_hWnd,
                                                     WM_APP + 1,
                                                     (WPARAM)new std::wstring(result),
                                                     0);
                                            }).detach();                                           
                                        }
                                        return S_OK;
                                    }
                                ).Get(),
                                nullptr
                            );

                            // ── ナビゲーション完了イベント ──────────
                            g_webview->add_NavigationCompleted(
                                Callback<ICoreWebView2NavigationCompletedEventHandler>(
                                    [](ICoreWebView2* sender,
                                       ICoreWebView2NavigationCompletedEventArgs* args) -> HRESULT
                                    {
                                        BOOL success = FALSE;
                                        args->get_IsSuccess(&success);
                                        if (!success)
                                        {
                                            COREWEBVIEW2_WEB_ERROR_STATUS status;
                                            args->get_WebErrorStatus(&status);
                                            // ファイルが見つからない場合はフォールバックHTML
                                            if (status == COREWEBVIEW2_WEB_ERROR_STATUS_CANNOT_CONNECT ||
                                                status == COREWEBVIEW2_WEB_ERROR_STATUS_UNKNOWN)
                                            {
                                                sender->NavigateToString(
                                                    L"<html><body style='font-family:sans-serif;"
                                                    L"display:flex;align-items:center;justify-content:center;"
                                                    L"height:100vh;margin:0;background:#1a1a2e;color:#eee'>"
                                                    L"<div><h2>&#x26A0; HTMLファイルが見つかりません</h2>"
                                                    L"<p>html/index.html を配置してください。</p></div></body></html>"
                                                );
                                            }
                                        }
                                        return S_OK;
                                    }
                                ).Get(),
                                nullptr
                            );

                            return S_OK;
                        }
                    ).Get()
                );
                return S_OK;
            }
        ).Get()
    );

    if (FAILED(hr))
    {
        MessageBoxW(hWnd, L"WebView2 環境の作成に失敗しました。", L"エラー", MB_ICONERROR | MB_OK);
        PostQuitMessage(1);
    }
}

// ── ウィンドウプロシージャ ────────────────────────────────
static LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case WM_SIZE:
        if (g_controller)
        {
            RECT bounds;
            GetClientRect(hWnd, &bounds);
            g_controller->put_Bounds(bounds);
        }
        return 0;

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;

    // ── キーボードショートカット ──────────────────────────
     case WM_KEYDOWN:
         if (wParam == VK_F5 && g_webview)
         {
             g_webview->Reload();               // F5: リロード
         }
         else if (wParam == VK_F12 && g_webview)
         {
             g_webview->OpenDevToolsWindow();   // F12: DevTools
         }
         return 0;

    case WM_APP + 1:
        {
            std::wstring* result = reinterpret_cast<std::wstring*>(wParam);
            if (result && g_webview)
            {
                g_webview->PostWebMessageAsString(result->c_str());
            }
            delete result;
            return 0;
        }

    default:
        return DefWindowProcW(hWnd, msg, wParam, lParam);
    }
}

// ── エントリーポイント ────────────────────────────────────
int WINAPI wWinMain(
    _In_     HINSTANCE hInstance,
    _In_opt_ HINSTANCE /*hPrevInstance*/,
    _In_     LPWSTR    /*lpCmdLine*/,
    _In_     int       nCmdShow)
{
    // DPI 対応
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

    // ── ウィンドウクラス登録 ──────────────────────────────
    WNDCLASSEXW wc = {};
    wc.cbSize        = sizeof(wc);
    wc.style         = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc   = WndProc;
    wc.hInstance     = hInstance;
    wc.hCursor       = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    wc.lpszClassName = WND_CLASS;
    wc.hIcon         = LoadIconW(nullptr, IDI_APPLICATION);

    if (!RegisterClassExW(&wc))
    {
        MessageBoxW(nullptr, L"ウィンドウクラスの登録に失敗しました。", L"エラー", MB_ICONERROR);
        return 1;
    }

    // ── ウィンドウ作成 ────────────────────────────────────
    const int W = 1200, H = 800;
    int screenW = GetSystemMetrics(SM_CXSCREEN);
    int screenH = GetSystemMetrics(SM_CYSCREEN);

    g_hWnd = CreateWindowExW(
        0, WND_CLASS, APP_TITLE,
        WS_OVERLAPPEDWINDOW,
        (screenW - W) / 2, (screenH - H) / 2,   // 画面中央
        W, H,
        nullptr, nullptr, hInstance, nullptr
    );

    if (!g_hWnd)
    {
        MessageBoxW(nullptr, L"ウィンドウの作成に失敗しました。", L"エラー", MB_ICONERROR);
        return 1;
    }

    ShowWindow(g_hWnd, nCmdShow);
    UpdateWindow(g_hWnd);

    // ── WebView2 初期化（非同期） ─────────────────────────
    InitWebView2(g_hWnd);

    // ── メッセージループ ──────────────────────────────────
    MSG msg = {};
    while (GetMessageW(&msg, nullptr, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    return static_cast<int>(msg.wParam);
}

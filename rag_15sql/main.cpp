#include <algorithm>
#include <cmath>
#include <cpr/cpr.h>
#include <chrono>
#include <fcntl.h>       // _O_U16TEXT
#include <fstream>
#include <filesystem>
#include <iostream>
#include <io.h>          // _setmode
#include <iomanip>
#include <random>
#include <string>
#include <sqlite3.h>
#include <sstream>
#include <stdexcept>
#include <windows.h>
#include <map>
#include <vector>
//#include <objbase.h>   // CoCreateGuid#include <random>
#include <shellapi.h>    // CommandLineToArgvW
#include <nlohmann/json.hpp>

#pragma comment(lib, "shell32.lib")

#include "dotenv.h"
#include "http_client.hpp"
#include "Database.hpp"

// JSON用エイリアス
using json = nlohmann::json;

const size_t      VECTOR_DIM = 3072;  // ベクトル次元数
const std::string DB_PATH = "example.db";


// 1ファイル分のデータを保持する構造体
struct TextFile {
    std::string filename;
    std::vector<std::string> lines;
};

struct QueryReq {
    std::string input;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(QueryReq, input)

struct SearchReq {
    std::wstring input;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(SearchReq, input)


std::string GuidToString(const GUID& guid)
{
    char buf[64];
    snprintf(buf, sizeof(buf),
        "%08lX-%04X-%04X-%04X-%012llX",
        guid.Data1,
        guid.Data2,
        guid.Data3,
        (guid.Data4[0] << 8) | guid.Data4[1],
        *((unsigned long long*)&guid.Data4[2])
    );
    return std::string(buf);
}
//
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
// wstring ↔ string 変換ヘルパー (Windows ANSI 限定)
static std::string WStrToStr(const std::wstring& ws)
{
    if (ws.empty()) return {};
    int n = ::WideCharToMultiByte(CP_UTF8, 0, ws.c_str(), -1, nullptr, 0, nullptr, nullptr);
    std::string s(n - 1, '\0');
    ::WideCharToMultiByte(CP_UTF8, 0, ws.c_str(), -1, s.data(), n, nullptr, nullptr);
    return s;
}

// std::wstring を UTF-8 の std::string に変換するヘルパー
std::string to_utf8(const std::wstring& wstr) {
    if (wstr.empty()) return std::string();
    int size_needed = WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), NULL, 0, NULL, NULL);
    std::string strTo(size_needed, 0);
    WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), &strTo[0], size_needed, NULL, NULL);
    return strTo;
}

// .txt ファイルを読み込んで行を返す
TextFile loadTextFile(const std::filesystem::path& filepath) {
    TextFile tf;
    tf.filename = filepath.filename().string();

    std::ifstream ifs(filepath);
    if (!ifs.is_open()) {
        std::cerr << "[警告] ファイルを開けません: " << filepath << "\n";
        return tf;
    }

    std::string line;
    while (std::getline(ifs, line)) {
        tf.lines.push_back(line);
    }
    return tf;
}


/**
*
* @param
*
* @return
*/
void vector_add(std::vector<float> embedding, std::string content) {
    try {
        auto now = std::chrono::system_clock::now();
    
        // ミリ秒精度に変換
        auto duration = now.time_since_epoch();
        auto millis = std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();
        std::string str_tmp = std::format("{}", millis);        
        std::wcout << L"str: " << StringToWString(str_tmp) << std::endl;
        Database app(DB_PATH);
        app.add_vec(embedding, content, str_tmp);
    }
    catch (const std::exception& ex) {
        std::wcerr << ex.what() << std::endl;
    }
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
int ebmed(std::string query, std::string api_key) {
    int ret = 0;
    try {
        auto embedding = getGeminiEmbedding(api_key, query);
        std::wcout << L"Embedding dimensions: " << embedding.size() << std::endl;
        vector_add(embedding, query);

    }
    catch (const std::exception& e) {
        std::wcout << L"Error , main \n";
        return 1;
    }
    return 0;
}

// 読み込んだデータを表示する
void printTextFiles(const std::vector<TextFile>& files , std::string api_key) {
    for (const auto& tf : files) {
        std::wcout << L"========================================\n";
        //std::wstring w_filename = StringToWString(tf.filename);
        //std::wcout << L"ファイル名: " << w_filename << L"\n";
        std::wcout << L"行数      : " << tf.lines.size() << L"\n";
        std::wcout << L"----------------------------------------\n";
        std::string target = "";
        for (size_t i = 0; i < tf.lines.size(); ++i) {
            //std::cout << "[" << i + 1 << "] " << tf.lines[i] << "\n";
            std::string tmp = tf.lines[i] + "\n";
            target.append(tmp);
        }
        std::wstring w_str = StringToWString(target);
        std::wcout <<  w_str << "\n";
        int resp = ebmed(target, api_key);
        std::wcout << L"resp=" << resp << L"\n";
    }
    std::wcout << L"========================================\n";
}


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
struct ChatQuery {
    std::string role;
    std::string content;
};
// これ一行で、QueryReq <=> json の変換が魔法のように可能になります
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(ChatQuery, role, content)
struct ChatRequest {
    std::string model;
    std::vector<ChatQuery> messages;
    double temperature;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(ChatRequest, model, messages, temperature)

struct SearchResponse {
    std::string ret;
    std::string text;
};   
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(SearchResponse, ret, text)

const std::string API_URL_CHAT = "http://localhost:8090/v1/chat/completions";

void send_chat(std::string query){
    try{
        HttpClient client;
        ChatQuery req2;
        req2.role = "user";
        req2.content = query;
        json j2 = req2;
        std::string json_str2 = j2.dump();
        //std::cout << "json_str2:" << json_str2 << std::endl;
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
        //std::cout << "json_str3:" << json_str3 << std::endl;
        std::string requestBody = json_str3;
        //std::cout << "requestBody:" << requestBody  << std::endl;
        auto resp = client.post(
            API_URL_CHAT,
            requestBody
            // Content-Type は省略時 "application/json" が使われる
        );      
        if(!resp.empty()) {
            std::string reply = extractContent(resp);
            std::wcout << "Assistant: " << StringToWString(reply)  << std::endl;
        }        
    }catch (const std::exception& e) {
        std::wcerr << L"\n[ERROR] " << e.what() << L"\n";
        std::wcerr << L"Error , rag_search" << std::endl;
    }    
}

/**
*
* @param
*
* @return
*/
void rag_search(std::string query, std::string api_key) {
    try {
        auto embedding = getGeminiEmbedding(api_key, query);
        std::wcout << L"Embedding dimensions: " << embedding.size() << std::endl;
        if (embedding.size() != VECTOR_DIM) {
            std::wcerr << L"error , Embedding size mismatch" << std::endl; 
            return;
        }
        Database app(DB_PATH);
        int count =  app.get_count();
        std::wcout << L"count=" << count << L"\n";
        if(count == 0){
            std::wcout << L"error, documet none"  << L"\n";
            return;
        }   
        std::string resp_str = app.rag_search(embedding);        
        std::string out_str = "日本語で、回答して欲しい。 \n要約して欲しい。\n\n";
        //std::string resp_str = out;
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
        std::wcout << StringToWString(out_str)  << std::endl;
        send_chat(out_str);
    }
    catch (const std::exception& e) {
        std::wcerr << L"\n[ERROR] " << e.what() << L"\n";
        std::wcerr << L"Error , rag_search" << std::endl;
    }
}
//
int main()
{
    dotenv env(".env");
    std::string api_key = env.get("GEMINI_API_KEY", "key123");
    std::cout << "api_key: " << api_key << std::endl;

    // ① stdout をワイド文字モードに切り替え
    _setmode(_fileno(stdout), _O_U16TEXT);

    // ② コンソールの出力コードページを UTF-8 に設定
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);

        // ③ Windows API でコマンドライン全体を UTF-16 で取得
    //    GetCommandLineW() はプロセス起動時のコマンドライン文字列を返す
    int    argc = 0;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);

    if (argv == nullptr)
    {
        std::wcerr << L"CommandLineToArgvW が失敗しました。\n";
        return 1;
    }

    // ④ 引数を表示
    std::wcout << L"引数の数: " << argc << L"\n\n";

    if (argc <= 1) {
        std::wcout << L"[ERROR] argment none \n";
        return 0;
    }

    std::wstring target_act = argv[1];
    if (target_act == L"embed") {
        std::wcout << L"#embed-start====\n";
        std::wstring dirPath = L"./data";
        try {
            if (!std::filesystem::exists(dirPath) || !std::filesystem::is_directory(dirPath)) {
                std::wcerr << L"[ERROR] folder none: " << dirPath << L"\n";
                return 1;
            }
            std::wcout << L"対象フォルダ: " << std::filesystem::absolute(dirPath) << L"\n\n";

            std::vector<TextFile> allFiles;

            // フォルダ内の .txt ファイルをすべて列挙
            for (const auto& entry : std::filesystem::directory_iterator(dirPath)) {
                if (entry.is_regular_file() &&
                    (entry.path().extension() == ".txt" || entry.path().extension() == ".md")) {
                    TextFile tf = loadTextFile(entry.path());
                    allFiles.push_back(std::move(tf));
                }
            }

            if (allFiles.empty()) {
                std::wcout << L".txt ファイルが見つかりませんでした。\n";
                return 0;
            }
            std::wcout << L"読み込んだファイル数: " << allFiles.size() << L"\n\n";
            printTextFiles(allFiles, api_key);
            return 0;
        }
        catch (const std::exception& ex) {
            std::wcerr << L"\n[ERROR] " << ex.what() << L"\n";
            return 1;
        }
    }
    if (target_act == L"search") {
        if (argc <= 2) {
            std::wcout << L"[ERROR] argment none \n";
            return 0;
        }
        std::wcout << L"search-start====\n";
        std::wstring input = argv[2];
        std::string query =  to_utf8(input);
        rag_search(query, api_key);        

    }

}

// プログラムの実行: Ctrl + F5 または [デバッグ] > [デバッグなしで開始] メニュー
// プログラムのデバッグ: F5 または [デバッグ] > [デバッグの開始] メニュー

// 作業を開始するためのヒント: 
//    1. ソリューション エクスプローラー ウィンドウを使用してファイルを追加/管理します 
//   2. チーム エクスプローラー ウィンドウを使用してソース管理に接続します
//   3. 出力ウィンドウを使用して、ビルド出力とその他のメッセージを表示します
//   4. エラー一覧ウィンドウを使用してエラーを表示します
//   5. [プロジェクト] > [新しい項目の追加] と移動して新しいコード ファイルを作成するか、[プロジェクト] > [既存の項目の追加] と移動して既存のコード ファイルをプロジェクトに追加します
//   6. 後ほどこのプロジェクトを再び開く場合、[ファイル] > [開く] > [プロジェクト] と移動して .sln ファイルを選択します

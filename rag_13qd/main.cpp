#include <cmath>
#include <cpr/cpr.h>
#include <fcntl.h>       // _O_U16TEXT
#include <fstream>
#include <filesystem>
#include <iostream>
#include <io.h>          // _setmode
#include <iomanip>
#include <string>
#include <windows.h>
#include <sstream>
#include <map>
#include <stdexcept>
#include <vector>
#include <objbase.h>   // CoCreateGuid#include <random>
#include <shellapi.h>    // CommandLineToArgvW
#include <nlohmann/json.hpp>


#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "ole32.lib")

#include "dotenv.h"
//#include "http_client.hpp"
#include "HttpClient.h"
#include "qdrant_client.hpp"
#include "my_rag.hpp"

//  JSON用エイリアス
using json = nlohmann::json;

const std::string COLLECTION = "sample_collection";
const size_t      VECTOR_DIM = 3072;  // ベクトル次元数
const std::wstring API_URL_CHAT = L"http://localhost:8090/v1/chat/completions";

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
        QdrantClient client("localhost", 6333);
        std::wcout << L"=== Qdrant C++ Client ===\n\n";
        std::wcout << L"--- ベクトル登録 ---\n";

        std::vector<QdrantClient::Point> points;
        GUID guid;
        if (CoCreateGuid(&guid) == S_OK)
        {
            std::string uuid = GuidToString(guid);
            std::wcout << "UUID: " << StringToWString(uuid) << std::endl;
            std::vector<QdrantClient::Point> points;
            QdrantClient::Point p;
            p.id = std::hash<std::string>{}(uuid);
            p.vector = embedding;
            p.payload = {
                {"content",    content},
                {"doc_id", uuid}, 
            };
            points.push_back(p);
            client.upsertPoints(COLLECTION, points);
        }
        else
        {
            std::wcout << "Failed to create GUID" << std::endl;
            return;
        }
    }
    catch (const std::exception& ex) {
        std::cerr << ex.what() << std::endl;
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

    std::wcout << L"Query: " << StringToWString(query) << std::endl;


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
void printTextFiles(const std::vector<TextFile>& files, std::string api_key) {
    for (const auto& tf : files) {
        std::wcout << L"========================================\n";
        //std::wstring w_filename = StringToWString(tf.filename);
        //std::wcout << L"ファイル名: " << w_filename << L"\n";
        std::wcout << L"行数      : " << tf.lines.size() << L"\n";
        std::wcout << L"----------------------------------------\n";
        std::string target = "";
        for (size_t i = 0; i < tf.lines.size(); ++i) {
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
void send_chat(std::string query) {
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
        std::wstring w_str4 = StringToWString(reply);
        std::wcout << L"Assistant: " << w_str4 << std::endl; 
        std::wcout << L"\n" << std::endl;
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

        QdrantClient qdrant_client("localhost", 6333);
        std::wcout << L"=== Qdrant C++ Client ===\n\n";

        auto results = qdrant_client.search(COLLECTION, embedding, 1);
        std::string matches = "";
        for (const auto& r : results) {
            std::wcout   << L"  スコア: " << r.score
            << L"\n";
            if (r.score > 0.5) {
                matches = r.payload["content"].get<std::string>();
            }
        }
        std::wcout << L"\n"; 
        //std::wcout << StringToWString(matches)  << std::endl;
        std::string out_str = "日本語で、回答して欲しい。 \n要約して欲しい。\n\n";
        std::string resp_str = matches;
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
        /*
        MyRag rag_helper("");
        std::string resp = rag_helper.GenerateContent(api_key, out_str);

        if (!resp.empty()) {
            std::wcout << L"=== AI ===\n" << StringToWString(resp) << std::endl;
        } else {
            std::wcerr << L"Failed to get response.\n" << std::endl;
        }        
        */
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
    if (target_act == L"init") {
        std::wcout << L"#init-start====\n";
        QdrantClient client("localhost", 6333);
        std::wcout << L"=== Qdrant C++ Client ===\n\n";
        // =========================================================
        // 2. コレクション管理
        // =========================================================
        std::wcout << L"--- Collections List ---\n";
        auto collections = client.listCollections();
        if (collections.empty()) {
            std::wcout << L"  (コレクションなし)\n";
        } else {
            for (std::string& c : collections) {
                std::wcout << L"  - " << StringToWString(c) << L"\n";
            };
        }

        // 既存コレクションを削除して再作成
        client.deleteCollection(COLLECTION);
        client.createCollection(COLLECTION, VECTOR_DIM, "Cosine");
        std::wcout << "\n";

    }    
    if (target_act == L"embed") {
        std::wcout << L"#embed-start====\n";
       //std::wstring dirPath = (argc >= 2) ? argv[2] : ".";
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
        std::wcout << L"search-start====\n";
        if (argc <= 2) {
            std::wcout << L"[ERROR] argment none \n";
            return 0;
        }
        std::wstring input = argv[2];
        std::string query =  to_utf8(input);
        rag_search(query, api_key);

        return 0;
    }

}

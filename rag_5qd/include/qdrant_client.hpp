#pragma once

#include <string>
#include <vector>
#include <map>
#include <optional>
#include <stdexcept>
#include <iostream>
#include <sstream>
#include <curl/curl.h>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

// ------------------------------------------------------------
// HTTP レスポンス受信用コールバック
// ------------------------------------------------------------
static size_t WriteCallback(void* contents, size_t size, size_t nmemb, std::string* out) {
    size_t total = size * nmemb;
    out->append(static_cast<char*>(contents), total);
    return total;
}

// ------------------------------------------------------------
// QdrantClient クラス
// ------------------------------------------------------------
class QdrantClient {
public:
    // コンストラクタ
    // host: Qdrant サーバーのホスト (例: "localhost")
    // port: Qdrant サーバーのポート (例: 6333)
    // api_key: API キー (なければ空文字列)
    QdrantClient(const std::string& host = "localhost",
                 int port = 6333,
                 const std::string& api_key = "")
        : base_url_("http://" + host + ":" + std::to_string(port)),
          api_key_(api_key)
    {
        curl_global_init(CURL_GLOBAL_DEFAULT);
    }

    ~QdrantClient() {
        curl_global_cleanup();
    }

    // --------------------------------------------------------
    // コレクション作成
    // collection_name : コレクション名
    // vector_size     : ベクトル次元数
    // distance        : "Cosine" | "Euclid" | "Dot"
    // --------------------------------------------------------
    bool createCollection(const std::string& collection_name,
                          size_t vector_size,
                          const std::string& distance = "Cosine")
    {
        json body = {
            {"vectors", {
                {"size", vector_size},
                {"distance", distance}
            }}
        };

        std::string url = base_url_ + "/collections/" + collection_name;
        auto [status, resp] = request("PUT", url, body.dump());

        if (status == 200 || status == 201) {
            //std::cout << "[OK] コレクション作成: " << collection_name << "\n";
            std::wcout << L"[OK] コレクション作成: \n";
            return true;
        }
        //std::cerr << "[ERROR] コレクション作成失敗 (HTTP " << status << "): " << resp << "\n";
        std::wcerr << L"[ERROR] コレクション作成失敗 (HTTP " << status << "): \n";
        return false;
    }

    // --------------------------------------------------------
    // コレクション削除
    // --------------------------------------------------------
    bool deleteCollection(const std::string& collection_name) {
        std::string url = base_url_ + "/collections/" + collection_name;
        auto [status, resp] = request("DELETE", url, "");

        if (status == 200) {
            //std::cout << "[OK] コレクション削除: " << collection_name << "\n";
            //std::wcout << L"[OK] コレクション削除: \n";
            return true;
        }
        //std::cerr << "[ERROR] コレクション削除失敗 (HTTP " << status << "): " << resp << "\n";
        std::wcerr << "[ERROR] コレクション削除失敗 (HTTP " << status << "): \n";
        return false;
    }

    // --------------------------------------------------------
    // コレクション一覧取得
    // --------------------------------------------------------
    std::vector<std::string> listCollections() {
        std::string url = base_url_ + "/collections";
        auto [status, resp] = request("GET", url, "");

        std::vector<std::string> names;
        if (status == 200) {
            auto j = json::parse(resp);
            for (auto& col : j["result"]["collections"]) {
                names.push_back(col["name"].get<std::string>());
            }
        } else {
            std::cerr << "[ERROR] コレクション一覧取得失敗 (HTTP " << status << ")\n";
        }
        return names;
    }

    // --------------------------------------------------------
    // ポイント（ベクトル）登録
    // points: { id -> { vector, payload } } のリスト
    // --------------------------------------------------------
    struct Point {
        uint64_t id;
        std::vector<float> vector;
        json payload;  // 任意のメタデータ
    };

    bool upsertPoints(const std::string& collection_name,
                      const std::vector<Point>& points)
    {
        json body_points = json::array();
        for (const auto& p : points) {
            json pt = {
                {"id", p.id},
                {"vector", p.vector},
                {"payload", p.payload}
            };
            body_points.push_back(pt);
        }
        json body = {{"points", body_points}};

        std::string url = base_url_ + "/collections/" + collection_name + "/points";
        auto [status, resp] = request("PUT", url, body.dump());

        if (status == 200) {
            std::wcout << L"[OK] " << points.size() << L" 件のポイントを登録しました\n";
            return true;
        }
        //std::cerr << "[ERROR] ポイント登録失敗 (HTTP " << status << "): " << resp << "\n";
        std::wcerr << L"[ERROR] ポイント登録失敗 (HTTP " << status << "): \n";
        return false;
    }

    // --------------------------------------------------------
    // ベクトル検索
    // query_vector : 検索クエリベクトル
    // top_k        : 上位何件を返すか
    // score_threshold: スコアの閾値 (optional)
    // with_payload : payload を含めるか
    // --------------------------------------------------------
    struct SearchResult {
        uint64_t id;
        float score;
        json payload;
    };

    std::vector<SearchResult> search(const std::string& collection_name,
                                     const std::vector<float>& query_vector,
                                     size_t top_k = 5,
                                     std::optional<float> score_threshold = std::nullopt,
                                     bool with_payload = true)
    {
        json body = {
            {"vector", query_vector},
            {"limit", top_k},
            {"with_payload", with_payload}
        };
        if (score_threshold.has_value()) {
            body["score_threshold"] = score_threshold.value();
        }

        std::string url = base_url_ + "/collections/" + collection_name + "/points/search";
        auto [status, resp] = request("POST", url, body.dump());

        std::vector<SearchResult> results;
        if (status == 200) {
            auto j = json::parse(resp);
            for (auto& r : j["result"]) {
                SearchResult sr;
                sr.id    = r["id"].get<uint64_t>();
                sr.score = r["score"].get<float>();
                sr.payload = r.value("payload", json::object());
                results.push_back(sr);
            }
        } else {
            //std::cerr << "[ERROR] 検索失敗 (HTTP " << status << "): " << resp << "\n";
            std::wcerr << L"[ERROR] 検索失敗 (HTTP " << status << "): \n";
        }
        return results;
    }

    // --------------------------------------------------------
    // フィルター付きベクトル検索
    // filter: Qdrant フィルター JSON (例: must/should/must_not)
    // --------------------------------------------------------
    std::vector<SearchResult> searchWithFilter(const std::string& collection_name,
                                               const std::vector<float>& query_vector,
                                               const json& filter,
                                               size_t top_k = 5,
                                               bool with_payload = true)
    {
        json body = {
            {"vector", query_vector},
            {"limit", top_k},
            {"with_payload", with_payload},
            {"filter", filter}
        };

        std::string url = base_url_ + "/collections/" + collection_name + "/points/search";
        auto [status, resp] = request("POST", url, body.dump());

        std::vector<SearchResult> results;
        if (status == 200) {
            auto j = json::parse(resp);
            for (auto& r : j["result"]) {
                SearchResult sr;
                sr.id    = r["id"].get<uint64_t>();
                sr.score = r["score"].get<float>();
                sr.payload = r.value("payload", json::object());
                results.push_back(sr);
            }
        } else {
            std::cerr << "[ERROR] フィルター検索失敗 (HTTP " << status << "): " << resp << "\n";
        }
        return results;
    }

    // --------------------------------------------------------
    // ID指定でポイント取得
    // --------------------------------------------------------
    std::optional<Point> getPoint(const std::string& collection_name, uint64_t id) {
        std::string url = base_url_ + "/collections/" + collection_name
                        + "/points/" + std::to_string(id);
        auto [status, resp] = request("GET", url, "");

        if (status == 200) {
            auto j = json::parse(resp)["result"];
            Point p;
            p.id = j["id"].get<uint64_t>();
            p.payload = j.value("payload", json::object());
            if (j.contains("vector") && !j["vector"].is_null()) {
                p.vector = j["vector"].get<std::vector<float>>();
            }
            return p;
        }
        std::cerr << "[ERROR] ポイント取得失敗 (HTTP " << status << ")\n";
        return std::nullopt;
    }

    // --------------------------------------------------------
    // ポイント削除
    // --------------------------------------------------------
    bool deletePoints(const std::string& collection_name,
                      const std::vector<uint64_t>& ids)
    {
        json body = {{"points", ids}};
        std::string url = base_url_ + "/collections/" + collection_name + "/points/delete";
        auto [status, resp] = request("POST", url, body.dump());

        if (status == 200) {
            std::cout << "[OK] " << ids.size() << " 件のポイントを削除しました\n";
            return true;
        }
        std::cerr << "[ERROR] ポイント削除失敗 (HTTP " << status << "): " << resp << "\n";
        return false;
    }

private:
    std::string base_url_;
    std::string api_key_;

    // HTTP リクエスト共通処理 (GET/POST/PUT/DELETE)
    std::pair<long, std::string> request(const std::string& method,
                                         const std::string& url,
                                         const std::string& body)
    {
        CURL* curl = curl_easy_init();
        if (!curl) throw std::runtime_error("curl_easy_init() 失敗");

        std::string response_body;
        struct curl_slist* headers = nullptr;
        headers = curl_slist_append(headers, "Content-Type: application/json");
        if (!api_key_.empty()) {
            headers = curl_slist_append(headers,
                ("api-key: " + api_key_).c_str());
        }

        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_body);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);

        if (method == "POST") {
            curl_easy_setopt(curl, CURLOPT_POST, 1L);
            curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.c_str());
            curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, (long)body.size());
        } else if (method == "PUT") {
            curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "PUT");
            curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.c_str());
            curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, (long)body.size());
        } else if (method == "DELETE") {
            curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "DELETE");
        }
        // GET はデフォルト

        CURLcode res = curl_easy_perform(curl);
        long http_code = 0;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);

        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);

        if (res != CURLE_OK) {
            throw std::runtime_error(std::string("curl エラー: ") + curl_easy_strerror(res));
        }
        return {http_code, response_body};
    }
};

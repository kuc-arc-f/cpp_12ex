#pragma once
#include "sqlite3.h"
#include <iostream>
#include <string>
#include <sstream>
#include <stdexcept>

struct EmbedData {
    std::string id;
    std::vector<float> embedding;
    std::string content;
};
struct ResultEmbed {
    std::string id;
    std::vector<float> embedding;
    std::string content;
    float distance;
};

// ─────────────────────────────────────────────
//  DB クラス
// ─────────────────────────────────────────────
class Database {
public:
    explicit Database(const std::string& path) : db_(nullptr) {
        if (sqlite3_open(path.c_str(), &db_) != SQLITE_OK) {
            std::cerr << "DB オープン失敗: " << sqlite3_errmsg(db_) << "\n";
            std::exit(1);
        }
        exec("PRAGMA journal_mode=WAL;");
        //createTable();
    }
    ~Database() { if (db_) sqlite3_close(db_); }

    int get_count() {
        int ret = 0;
        std::string sql =
            "SELECT COUNT(*) FROM document;";

        sqlite3_stmt* stmt = prepare(sql.c_str());
        int count = 0;
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            std::string value = col_text(stmt, 0);
            //std::cout << "get_count=" << value << "\n";
            int i = std::stoi(value);
            count = i;
        }
        sqlite3_finalize(stmt);
        ret = count;
        return ret;
    }

    std::vector<EmbedData> get_items() {
        std::string sql =
            "SELECT id, content, embeddings FROM document;";

        sqlite3_stmt* stmt = prepare(sql.c_str());
        int count = 0;
        std::vector<EmbedData> items;
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            EmbedData data;
            data.id      = col_text(stmt, 0);
            data.content      = col_text(stmt, 1);
            std::string emb_str = col_text(stmt, 2);
            json j1 = json::parse(emb_str);
            auto vec = j1;
            int vlength = sizeof(vec) / sizeof(vec[0]);
            //std::cout << "vlen=" << vec.size() << std::endl;
            data.embedding    = vec.get<std::vector<float>>();
            items.push_back(data);
        }
        sqlite3_finalize(stmt);
        return items;
    }

    float cosine_similarity(const std::vector<float>& v1, const std::vector<float>& v2) {
        if (v1.size() != v2.size()) {
            throw std::invalid_argument("Vectors must be of the same length.");
        }

        float dot = 0.0f, norm1Sq = 0.0f, norm2Sq = 0.0f;

        for (size_t i = 0; i < v1.size(); i++) {
            const float a = v1[i];
            const float b = v2[i];
            dot     += a * b;
            norm1Sq += a * a;
            norm2Sq += b * b;
        }

        return dot / (std::sqrt(norm1Sq) * std::sqrt(norm2Sq));
    }

    std::string rag_search(std::vector<float> embedding) {
        std::string ret = "";
        try {
            int vlen = sizeof(embedding) / sizeof(embedding[0]);
            //std::cout << "embedding.vlen=" << embedding.size() << std::endl;            

            std::vector<EmbedData> items = get_items();
            std::vector<ResultEmbed> result_items;
            for (const auto& data : items) {
                std::string id = data.id;
                std::vector<float> vec = data.embedding;
                int vlength = sizeof(vec) / sizeof(vec[0]);
                //std::cout << "vlen=" << vec.size() << std::endl;            
                //std::cout << data.id << "\t| " << data.content << std::endl;

                float distance = cosine_similarity(embedding, vec);
                //std::cout << "distance=" << distance << std::endl;            
                ResultEmbed res_item;
                res_item.id = id;
                res_item.embedding = vec;
                res_item.content = data.content;
                res_item.distance = distance;
                //std::cout << data.id << ", distance=" << distance << std::endl;
                if(distance > 0.4) {
                    result_items.push_back(res_item);
                }
            }
            std::sort(result_items.begin(), result_items.end(),
                [](const ResultEmbed& a, const ResultEmbed& b) {
                    return a.distance > b.distance;
                }
            );   

            // 結果表示
            std::string out_str = "";
            for (const auto& item : result_items) {
                if (out_str.empty()) {
                    std::wcout << L"distance=" << item.distance << std::endl;
                        //<< L", id=" << item.id << std::endl;
                    out_str = item.content + "\n";
                }        
            }
            //std::cout << "out_str=" << out_str << std::endl;
            return out_str;
        } catch (const std::exception &e) {
            std::wcerr << e.what() << std::endl;
            return ret;
        }
        return ret;
    }

    bool add_vec(std::vector<float> embedding, std::string content, std::string uuid) {
        std::stringstream ss;
          ss << "[";
          for (size_t i = 0; i < embedding.size(); ++i) {
              if (i > 0) ss << ",";
              ss << embedding[i];
          }
          ss << "]";
        std::string emb_str = ss.str();
        std::string new_id= uuid;          

        const char* sql =
            "INSERT INTO document (id, content, embeddings) VALUES (?, ?, ?);";
        sqlite3_stmt* stmt = prepare(sql);
        sqlite3_bind_text(stmt, 1, new_id.c_str(),    -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 2, content.c_str(),    -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 3, emb_str.c_str(), -1, SQLITE_TRANSIENT);
        bool ok = sqlite3_step(stmt) == SQLITE_DONE;
        sqlite3_finalize(stmt);

        return ok;
    }

/*
    std::vector<Todo> getTodos(const std::string& filter = "all") {
        std::string sql =
            "SELECT id, title, done, priority, created_at FROM todos";
        if (filter == "active") sql += " WHERE done = 0";
        else if (filter == "done")   sql += " WHERE done = 1";
        sql += " ORDER BY CASE priority WHEN 'high' THEN 1"
            "  WHEN 'medium' THEN 2 ELSE 3 END, id;";

        sqlite3_stmt* stmt = prepare(sql.c_str());
        std::vector<Todo> todos;
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            Todo t;
            t.id = sqlite3_column_int(stmt, 0);
            t.title = col_text(stmt, 1);
            t.done = sqlite3_column_int(stmt, 2) != 0;
            t.priority = col_text(stmt, 3);
            t.created_at = col_text(stmt, 4);
            todos.push_back(t);
        }
        sqlite3_finalize(stmt);
        return todos;
    }

    std::vector<Todo> search(const std::string& keyword) {
        const char* sql =
            "SELECT id, title, done, priority, created_at FROM todos"
            " WHERE title LIKE ? ORDER BY id;";
        sqlite3_stmt* stmt = prepare(sql);
        std::string pat = "%" + keyword + "%";
        sqlite3_bind_text(stmt, 1, pat.c_str(), -1, SQLITE_TRANSIENT);
        std::vector<Todo> todos;
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            Todo t;
            t.id = sqlite3_column_int(stmt, 0);
            t.title = col_text(stmt, 1);
            t.done = sqlite3_column_int(stmt, 2) != 0;
            t.priority = col_text(stmt, 3);
            t.created_at = col_text(stmt, 4);
            todos.push_back(t);
        }
        sqlite3_finalize(stmt);
        return todos;
    }
*/

private:
    sqlite3* db_;

    void createTable() {
        exec(R"(
            CREATE TABLE IF NOT EXISTS todos (
                id         INTEGER PRIMARY KEY AUTOINCREMENT,
                title      TEXT    NOT NULL,
                done       INTEGER NOT NULL DEFAULT 0,
                priority   TEXT    NOT NULL DEFAULT 'medium',
                created_at TEXT    NOT NULL DEFAULT (datetime('now','localtime'))
            );
        )");
    }

    void exec(const std::string& sql) {
        char* err = nullptr;
        if (sqlite3_exec(db_, sql.c_str(), nullptr, nullptr, &err) != SQLITE_OK) {
            std::cerr << "SQL エラー: " << err << "\n";
            sqlite3_free(err);
        }
    }

    sqlite3_stmt* prepare(const char* sql) {
        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
            std::cerr << "Prepare 失敗: " << sqlite3_errmsg(db_) << "\n";
            std::exit(1);
        }
        return stmt;
    }

    static std::string col_text(sqlite3_stmt* s, int col) {
        const unsigned char* t = sqlite3_column_text(s, col);
        return t ? reinterpret_cast<const char*>(t) : "";
    }
};

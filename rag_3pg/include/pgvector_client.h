#pragma once
// ============================================================
//  pgvector_client.h
//  PGVector ベクトル登録・検索 クライアント (libpq 使用)
//  Windows Visual Studio 2019/2022 対応
// ============================================================

#include <string>
#include <vector>
#include <stdexcept>
#include <sstream>
#include <iomanip>

// PostgreSQL クライアントライブラリ
// NuGet: "libpq" または手動で PostgreSQL\include を追加
#include <libpq-fe.h>

// ============================================================
//  型エイリアス
// ============================================================
using FloatVec = std::vector<float>;

// ============================================================
//  接続情報
// ============================================================
struct PGConnConfig {
    std::string host     = "localhost";
    int         port     = 5432;
    std::string dbname   = "vectordb";
    std::string user     = "postgres";
    std::string password = "password";

    std::string toConnString() const {
        std::ostringstream oss;
        oss << "host="     << host
            << " port="    << port
            << " dbname="  << dbname
            << " user="    << user
            << " password=" << password;
        return oss.str();
    }
};

// ============================================================
//  検索結果
// ============================================================
struct SearchResult {
    int         id;
    std::string label;
    FloatVec    embedding;
    double      distance;
};

// ============================================================
//  PGVectorClient クラス
// ============================================================
class PGVectorClient {
public:
    explicit PGVectorClient(const PGConnConfig& cfg);
    ~PGVectorClient();

    // 接続・切断
    void connect();
    void disconnect();
    bool isConnected() const;

    // スキーマ初期化
    void initSchema(int dim);

    // ベクトル登録
    int  insertEmbedding(const std::string& label, const FloatVec& vec);

    // 近傍検索 (L2 距離)
    std::vector<SearchResult> searchNN(const FloatVec& query, int topK = 5);

    // コサイン類似度検索
    std::vector<SearchResult> searchCosine(const FloatVec& query, int topK = 5);

    // インデックス作成 (IVFFlat)
    void createIVFFlatIndex(int lists = 100);

    // インデックス作成 (HNSW)
    void createHNSWIndex(int m = 16, int efConstruction = 64);

private:
    PGConnConfig m_cfg;
    PGconn*      m_conn = nullptr;

    // ユーティリティ
    void         execSQL(const std::string& sql);
    PGresult*    execQuery(const std::string& sql);
    std::string  vecToLiteral(const FloatVec& v) const;
    FloatVec     literalToVec(const char* s) const;
    void         checkResult(PGresult* res, const std::string& ctx);
};

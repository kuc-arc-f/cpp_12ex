// ============================================================
//  pgvector_client.cpp
//  PGVector ベクトル登録・検索 実装
// ============================================================

#include "pgvector_client.h"
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <cstring>

// ============================================================
//  コンストラクタ / デストラクタ
// ============================================================
PGVectorClient::PGVectorClient(const PGConnConfig& cfg)
    : m_cfg(cfg), m_conn(nullptr)
{}

PGVectorClient::~PGVectorClient() {
    disconnect();
}

// ============================================================
//  接続
// ============================================================
void PGVectorClient::connect() {
    m_conn = PQconnectdb(m_cfg.toConnString().c_str());
    if (PQstatus(m_conn) != CONNECTION_OK) {
        std::string err = PQerrorMessage(m_conn);
        PQfinish(m_conn);
        m_conn = nullptr;
        throw std::runtime_error("PostgreSQL 接続失敗: " + err);
    }
    //std::wcout << L"[PGVector] 接続成功: "
    //          << m_cfg.host << L":" << m_cfg.port
    //          << L"/" << m_cfg.dbname << L"\n";
    std::wcout << L"[PGVector] 接続成功: ";
}

void PGVectorClient::disconnect() {
    if (m_conn) {
        PQfinish(m_conn);
        m_conn = nullptr;
        std::wcout << L"[PGVector] disconnect\n";
    }
}

bool PGVectorClient::isConnected() const {
    return m_conn && PQstatus(m_conn) == CONNECTION_OK;
}

// ============================================================
//  スキーマ初期化
//   - pgvector 拡張を有効化
//   - embeddings テーブルを作成
// ============================================================
void PGVectorClient::initSchema(int dim) {
    // pgvector 拡張
    execSQL("CREATE EXTENSION IF NOT EXISTS vector;");

    // テーブル作成
    std::ostringstream oss;
    oss << "CREATE TABLE IF NOT EXISTS embeddings ("
        << "  id        SERIAL PRIMARY KEY,"
        << "  label     TEXT NOT NULL,"
        << "  embedding vector(" << dim << ") NOT NULL,"
        << "  created_at TIMESTAMPTZ DEFAULT NOW()"
        << ");";
    execSQL(oss.str());

    //std::cout << "[PGVector] スキーマ初期化完了 (dim=" << dim << ")\n";
}

// ============================================================
//  ベクトル登録
// ============================================================
int PGVectorClient::insertEmbedding(const std::string& label,
                                    const FloatVec&    vec)
{
    std::string lit = vecToLiteral(vec);

    // パラメータバインド版クエリ
    const char* paramValues[2];
    paramValues[0] = label.c_str();
    paramValues[1] = lit.c_str();

    PGresult* res = PQexecParams(
        m_conn,
        "INSERT INTO documents (content, embedding) "
        "VALUES ($1, $2::vector) RETURNING id",
        2,          // パラメータ数
        nullptr,    // 型OID (自動推定)
        paramValues,
        nullptr,    // パラメータ長 (テキスト形式は不要)
        nullptr,    // バイナリフラグ
        0           // 結果形式: テキスト
    );
    checkResult(res, "insertEmbedding");

    int newId = std::stoi(PQgetvalue(res, 0, 0));
    PQclear(res);

    std::wcout << L"[PGVector] 登録完了: id=" << newId << L"\n";
    return newId;
}

// ============================================================
//  近傍検索 (L2 距離: <->)
// ============================================================
std::vector<SearchResult>
PGVectorClient::searchNN(const FloatVec& query, int topK)
{
    std::string lit = vecToLiteral(query);
    std::ostringstream oss;
    oss << "SELECT id, label, embedding, "
        << "       embedding <-> '" << lit << "'::vector AS distance "
        << "FROM embeddings "
        << "ORDER BY distance "
        << "LIMIT " << topK << ";";

    PGresult* res = execQuery(oss.str());

    std::vector<SearchResult> results;
    int rows = PQntuples(res);
    for (int i = 0; i < rows; ++i) {
        SearchResult sr;
        sr.id       = std::stoi(PQgetvalue(res, i, 0));
        sr.label    = PQgetvalue(res, i, 1);
        sr.embedding= literalToVec(PQgetvalue(res, i, 2));
        sr.distance = std::stod(PQgetvalue(res, i, 3));
        results.push_back(std::move(sr));
    }
    PQclear(res);
    return results;
}

// ============================================================
//  コサイン類似度検索 (1 - コサイン類似度: <=>)
// ============================================================
std::vector<SearchResult>
PGVectorClient::searchCosine(const FloatVec& query, int topK)
{
    std::string lit = vecToLiteral(query);
    std::ostringstream oss;
    //<< "       embedding <=> '" << lit << "'::vector AS distance "
    oss << "SELECT id, content, embedding, "
        << "       embedding <=> '" << lit << "' AS distance "
        << "FROM documents "
        << "ORDER BY distance "
        << "LIMIT " << topK << ";";

    PGresult* res = execQuery(oss.str());

    std::vector<SearchResult> results;
    int rows = PQntuples(res);
    for (int i = 0; i < rows; ++i) {
        SearchResult sr;
        sr.id       = std::stoi(PQgetvalue(res, i, 0));
        sr.label    = PQgetvalue(res, i, 1);
        sr.embedding= literalToVec(PQgetvalue(res, i, 2));
        sr.distance = std::stod(PQgetvalue(res, i, 3));
        results.push_back(std::move(sr));
    }
    PQclear(res);
    return results;
}

// ============================================================
//  IVFFlat インデックス作成
// ============================================================
void PGVectorClient::createIVFFlatIndex(int lists) {
    std::ostringstream oss;
    oss << "CREATE INDEX IF NOT EXISTS embeddings_ivfflat_idx "
        << "ON embeddings "
        << "USING ivfflat (embedding vector_l2_ops) "
        << "WITH (lists = " << lists << ");";
    execSQL(oss.str());
    std::cout << "[PGVector] IVFFlat インデックス作成完了 (lists="
              << lists << ")\n";
}

// ============================================================
//  HNSW インデックス作成
// ============================================================
void PGVectorClient::createHNSWIndex(int m, int efConstruction) {
    std::ostringstream oss;
    oss << "CREATE INDEX IF NOT EXISTS embeddings_hnsw_idx "
        << "ON embeddings "
        << "USING hnsw (embedding vector_l2_ops) "
        << "WITH (m = " << m
        << ", ef_construction = " << efConstruction << ");";
    execSQL(oss.str());
    std::cout << "[PGVector] HNSW インデックス作成完了 (m="
              << m << ", ef_construction=" << efConstruction << ")\n";
}

// ============================================================
//  Private: SQL 実行 (結果不要)
// ============================================================
void PGVectorClient::execSQL(const std::string& sql) {
    PGresult* res = PQexec(m_conn, sql.c_str());
    checkResult(res, sql);
    PQclear(res);
}

// ============================================================
//  Private: SELECT クエリ実行
// ============================================================
PGresult* PGVectorClient::execQuery(const std::string& sql) {
    PGresult* res = PQexec(m_conn, sql.c_str());
    if (PQresultStatus(res) != PGRES_TUPLES_OK) {
        std::string err = PQerrorMessage(m_conn);
        PQclear(res);
        throw std::runtime_error("クエリ失敗: " + err + "\nSQL: " + sql);
    }
    return res;
}

// ============================================================
//  Private: ベクトル → PostgreSQL リテラル文字列
//  例: [0.1, 0.2, 0.3] → "[0.1,0.2,0.3]"
// ============================================================
std::string PGVectorClient::vecToLiteral(const FloatVec& v) const {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(8);
    oss << "[";
    for (size_t i = 0; i < v.size(); ++i) {
        if (i) oss << ",";
        oss << v[i];
    }
    oss << "]";
    return oss.str();
}

// ============================================================
//  Private: PostgreSQL リテラル文字列 → ベクトル
//  例: "[0.1,0.2,0.3]" → {0.1f, 0.2f, 0.3f}
// ============================================================
FloatVec PGVectorClient::literalToVec(const char* s) const {
    FloatVec result;
    if (!s || s[0] == '\0') return result;

    std::string str(s);
    // '[' と ']' を除去
    if (str.front() == '[') str = str.substr(1);
    if (str.back()  == ']') str = str.substr(0, str.size() - 1);

    std::istringstream ss(str);
    std::string token;
    while (std::getline(ss, token, ',')) {
        result.push_back(std::stof(token));
    }
    return result;
}

// ============================================================
//  Private: PGresult エラーチェック
// ============================================================
void PGVectorClient::checkResult(PGresult* res, const std::string& ctx) {
    ExecStatusType status = PQresultStatus(res);
    if (status != PGRES_COMMAND_OK && status != PGRES_TUPLES_OK) {
        std::string err = PQerrorMessage(m_conn);
        PQclear(res);
        throw std::runtime_error("SQL エラー [" + ctx + "]: " + err);
    }
}

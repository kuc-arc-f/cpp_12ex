#pragma once
using json = nlohmann::json;

struct Todo {
    int id;
    std::string title;
};

struct TodoData {
    int max_id;
    std::vector<Todo> items;
};


// ─────────────────────────────────────────
//  todo helper
// ─────────────────────────────────────────
class MyTodo {
private:
    std::string m_file_path = "";

public:
    explicit MyTodo(const std::string& path) {
        m_file_path = path;
    }
    ~MyTodo() {
    }


    // JSONファイルからデータ読み込み
    TodoData load_data() {
        TodoData data{0, {}};
        std::ifstream file(m_file_path);
        if (file.is_open()) {
            try {
                json j = json::parse(file);
                if (j.contains("max_id") && j["max_id"].is_number()) {
                    data.max_id = j["max_id"].get<int>();
                }
                if (j.contains("items") && j["items"].is_array()) {
                    for (const auto& item : j["items"]) {
                        if (item.contains("id") && item.contains("title")) {
                            data.items.push_back({item["id"].get<int>(), item["title"].get<std::string>()});
                        }
                    }
                }
            } catch (const json::parse_error& e) {
                std::cerr << "JSON解析エラー: " << e.what() << "\n";
            }
        }
        return data;
    }

    // データをJSONファイルに保存
    void save_data(const TodoData& data) {
        json j;
        j["max_id"] = data.max_id;
        j["items"] = json::array();
        for (const auto& item : data.items) {
            j["items"].push_back({{"id", item.id}, {"title", item.title}});
        }
        std::ofstream file(m_file_path);
        if (file.is_open()) {
            file << j.dump(2); // インデント2で整形出力
            file.close();
        } else {
            std::cerr << "エラー: ファイルに書き込めません。\n";
        }
    }
    // TODO追加
    void add_todo(TodoData& data, const std::string& title) {
        data.max_id++;
        data.items.push_back({data.max_id, title});
        save_data(data);
        //std::cout << "追加完了: #" << data.max_id << " " << title << "\n";
        std::cout << "add: #" << data.max_id << " " << title << "\n";
    }
    // TODO削除
    void delete_todo(TodoData& data, int id) {
        auto it = std::remove_if(data.items.begin(), data.items.end(),
                                [id](const Todo& t) { return t.id == id; });
        if (it == data.items.end()) {
            //std::cout << "ID #" << id << " は存在しません。\n";
            std::cout << "ID #" << id << " none \n";
        } else {
            data.items.erase(it, data.items.end());
            save_data(data);
            //std::cout << "削除完了: #" << id << "\n";
            std::cout << "delete: #" << id << "\n";
        }
    } 
    
    std::string todo_to_json(const Todo& t) {
        std::ostringstream oss;
        oss << "{"
            << "\"id\":"    << t.id           << ","
            << "\"title\":\"" << t.title      << "\""
            //<< "\"done\":"  << (t.done ? "true" : "false")
            << "}";
        return oss.str();
    }

    std::string todos_to_json(const std::vector<Todo>& todos) {
        std::ostringstream oss;
        oss << "[";
        for (size_t i = 0; i < todos.size(); ++i) {
            if (i > 0) oss << ",";
            oss << todo_to_json(todos[i]);
        }
        oss << "]";
        return oss.str();
    }    
};

# rag_15sql

 Version: 0.9.1

 date    : 2026/05/30

 update :

***

C++ windows , RAG Search + SQLite

* LLVM CLang use
* SQLite DB
* visual studio 2026 community
* node 24
* windows11
* model: gemma-4-E2B-it-Q4_K_S.gguf
* llama.cpp , llama-server

***
### related

https://huggingface.co/unsloth/gemma-4-E2B-it-GGUF

***
https://www.sqlite.org/download.html

* sqlite-amalgamation-*.zip , download
* sqlite3.h , sqlite3.c

***
### setup
* llama-server start
* port 8090: gemma-4-E2B

```
#gemma-4-E2B

/usr/local/llama-b8642/llama-server -m /var/lm_data/unsloth/gemma-4-E2B-it-Q4_K_S.gguf \
 --chat-template-kwargs '{"enable_thinking": false}' --port 8090 

```
***
### .env

```
GEMINI_API_KEY=
```

***
### table

* table.sql

```
sqlite3 ./example.db < table.sql
```

***
### vcpkg install
```
vcpkg install nlohmann-json:x64-windows
vcpkg install curl:x64-windows
vcpkg install cpr:x64-windows
```

***
### build

```
clang++ -std=c++20 -O2 -I./include ^
-I/prog/vcpkg/installed/x64-windows/include ^
-L/prog/vcpkg/installed/x64-windows/lib -L./lib -lsqlite3  -llibcurl -lcpr ^
main.cpp -o main.exe

```


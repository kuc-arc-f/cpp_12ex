# rag_3pg

 Version: 0.9.1

 date    : 2026/05/10

 update :

***

C++ Window Webview2 + RAG + Pgvector React , CLang

* LLVM CLang use
* visual studio 2026 community
* node 24
* windows11

***
### vector data add

https://github.com/kuc-arc-f/cpp_4ex/tree/main/rag_1

***
### related

https://huggingface.co/unsloth/gemma-4-E2B-it-GGUF

https://huggingface.co/Qwen/Qwen3-Embedding-0.6B-GGUF

***
### setup
* llama-server start
* port 8080: Qwen3-Embedding-0.6B
* port 8090: gemma-4-E2B

```
#Qwen3-Embedding-0.6B

/home/user123/llama-server -m /var/lm_data/Qwen3-Embedding-0.6B-Q8_0.gguf --embedding  -c 1024 --port 8080

#gemma-4-E2B

/usr/local/llama-b8642/llama-server -m /var/lm_data/unsloth/gemma-4-E2B-it-Q4_K_S.gguf \
 --chat-template-kwargs '{"enable_thinking": false}' --port 8090 
```
***
### table

* table.sql

***
### vcpkg install
```
vcpkg install webview2:x64-windows
vcpkg install nlohmann-json:x64-windows
vcpkg install curl:x64-windows
```

***
### front build

```
npm i
npm run build
```

***
### build

```
clang++ -target x86_64-pc-windows-msvc -m64 -std=c++17 -O2 ^
  main.cpp ./include/pgvector_client.cpp -o main.exe ^
  -I./include -I/prog/vcpkg/installed/x64-windows/include ^
  -I/prog/postgresql-18.3-2/pgsql/include ^
  -L/prog/vcpkg/installed/x64-windows/lib ^
  -L/prog/postgresql-18.3-2/pgsql/lib ^
  -lWebView2Loader.dll -luser32 -lgdi32 -lole32 -loleaut32  -llibcurl -llibpq

```

***
### blog

https://zenn.dev/knaka0209/scraps/baaf4c51b6e3a7


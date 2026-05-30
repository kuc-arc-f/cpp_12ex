# rag_5qd

 Version: 0.9.1

 date    : 2026/05/30

 update :

***

C++ Window Webview2 + RAG Search + React , LLVM CLang


* LLVM CLang use
* Qdrant database
* visual studio 2026 community
* node 24
* windows11
* embedding : gemini-embedding-001
* model: gemma-4-E2B

***
### vector data add

https://github.com/kuc-arc-f/cpp_12ex/tree/main/rag_13qd

***
### related

https://huggingface.co/unsloth/gemma-4-E2B-it-GGUF

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
### vcpkg install
```
vcpkg install webview2:x64-windows
vcpkg install nlohmann-json:x64-windows
vcpkg install curl:x64-windows
vcpkg install cpr:x64-windows
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
clang++ -target x86_64-pc-windows-msvc -m64 -std=c++17 -O2 main.cpp -o main.exe ^
  -I/prog/vcpkg/installed/x64-windows/include ^
  -L/prog/vcpkg/installed/x64-windows/lib ^
  -lWebView2Loader.dll -luser32 -lgdi32 -lole32 -loleaut32  -llibcurl -lcpr

```

***
### blog

https://zenn.dev/knaka0209/scraps/32fa7a287c238b



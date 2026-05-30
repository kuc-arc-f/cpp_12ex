# rag_13qd

 Version: 0.9.1

 date    : 2026/05/30

 update :

***

C++ Window  RAG Search CLI , LLVM CLang

* Qdrant database
* LLVM CLang use
* visual studio 2026 community
* windows11
* embedding : gemini-embedding-001
* model: gemma-4-E2B

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
### env

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
### build

```
nmake all

```

***

```
#init
.\main.exe init

#embed
.\main.exe embed

#search
.\main.exe search hello
```

***
### blog

https://zenn.dev/knaka0209/scraps/f8db96e4a76798

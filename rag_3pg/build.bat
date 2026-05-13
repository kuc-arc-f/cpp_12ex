clang++ -target x86_64-pc-windows-msvc -m64 -std=c++17 -O2 ^
  main.cpp ./include/pgvector_client.cpp -o main.exe ^
  -I./include -I/prog/vcpkg/installed/x64-windows/include ^
  -I/prog/postgresql-18.3-2/pgsql/include ^
  -L/prog/vcpkg/installed/x64-windows/lib ^
  -L/prog/postgresql-18.3-2/pgsql/lib ^
  -lWebView2Loader.dll -luser32 -lgdi32 -lole32 -loleaut32  -llibcurl -llibpq


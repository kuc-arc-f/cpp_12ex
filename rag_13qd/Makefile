CXX = clang++
CXXFLAG_1= -I./include -I/prog/vcpkg/installed/x64-windows/include
CXXFLAG_LIB_1= -L/prog/vcpkg/installed/x64-windows/lib -llibcurl -lcpr

CXXFLAGS = -std=c++17 $(CXXFLAG_1) $(CXXFLAG_LIB_1)

TARGET = main.exe
all: $(TARGET)

$(TARGET): main.o
    $(CXX) $(CXXFLAGS) main.o -o $(TARGET)

main.o: main.cpp
    $(CXX) $(CXXFLAGS) -c main.cpp

clean:
    del *.o *.exe
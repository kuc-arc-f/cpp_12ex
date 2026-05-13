#pragma once
#include <iostream>

class MyConfig {
private:

public:
    int EMBED_SIZE = 1024;

    explicit MyConfig(std::string str){}
    ~MyConfig() {}
};

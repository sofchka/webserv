#ifndef SERVER_HPP
#define SERVER_HPP
#include <iostream>
#include <string>
#include <sstream>
#include <fstream>
#include <cstring>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>

struct Request
{
    std::string method;
    std::string path;
    std::string version;
};
Request parse_f(std::string request);
#endif
#ifndef SERVER_HPP
#define SERVER_HPP

#include <iostream>
#include <string>
#include <sstream>
#include <fstream>
#include <fcntl.h>
#include <cstring>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include "Config.hpp"
#include <vector>
#include <algorithm>

struct Request
{
    std::string method;
    std::string path;
    std::string version;
};

class Server
{
    private:
        std::vector<int> clients;
        int server_fd;
        Config config;
        ServerConfig server_config;
    public:
        Server();
        Server(const Server& other);
        Server& operator=(const Server& other);
        ~Server();
        void init();
        void run();
        void acceptClient();
        void handleClient(int fd);
};

Request parse_f(std::string request);
std::string to_str(size_t  len);
std::string get_type(const std::string &path);
std::string make_response(int status,const std::string& content,const std::string& type);

#endif

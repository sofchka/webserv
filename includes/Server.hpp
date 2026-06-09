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
#include <map>

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
        std::vector<int> server_fds;
        std::vector<ServerConfig> server_configs;
        std::map<int, size_t> client_servers;
        std::map<int, std::string> read_buffer;
        std::map<int, std::string> write_buffer;
        std::map<int, size_t> write_offset;
        std::map<int, bool> close_after_write;
        Config config;
        void closeClient(int fd);
        ssize_t send(int fd, const char* data, size_t size, int flags);
        void flushClient(int fd);
    public:
        Server();
        Server(const Server& other);
        Server& operator=(const Server& other);
        ~Server();
        void init(const std::string& config_path);
        void run();
        void acceptClient(size_t server_index);
        void handleClient(int fd);
};

Request parse_f(std::string request);
std::string to_str(size_t  len);
std::string get_type(const std::string &path);
std::string make_response(int status,const std::string& content,const std::string& type);
std::string make_response(int status,const std::string& content,const std::string& type,const ServerConfig* server);
#endif

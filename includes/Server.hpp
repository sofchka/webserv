#ifndef SERVER_HPP
#define SERVER_HPP

#include <iostream>
#include <string>
#include <sstream>
#include <fstream>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <cstring>
#include <unistd.h>
#include <ctime>
#include <arpa/inet.h>
#include <netinet/in.h>
#include "Config.hpp"
#include <vector>
#include <algorithm>
#include <map>
#include <ctime>

struct Request
{
    Request();

    bool valid;
    std::string method;
    std::string path;
    std::string query;
    std::string version;
    std::map<std::string, std::string> headers;
    size_t content_length;
    bool chunked;
    std::string body;
};

class Server
{
    private:
        std::vector<int> clients;
        std::vector<int> server_fds;
        std::vector<size_t> listener_defaults;
        std::vector<ServerConfig> server_configs;
        std::vector<size_t> listener_servers;
        std::map<int, size_t> client_servers;
        std::map<int, std::string> read_buffer;
        std::map<int, std::string> write_buffer;
        std::map<int, size_t> write_offset;
        std::map<int, bool> close_after_write;
<<<<<<< HEAD
        std::map<int, std::time_t> client_activity;
        Config config;
        void closeClient(int fd);
        void closeClientNow(int fd);
        void closeIdleClients();
        void replyErrorAndClose(int fd, int status, const std::string& message,
                                const ServerConfig& server);
=======
        std::map<int, time_t> client_last_activity;
        Config config;
        void closeClient(int fd);
        const ServerConfig* defaultServerForClient(int fd) const;
        void sendErrorAndClose(int fd, int status, const std::string& message);
>>>>>>> ac58abdad79afd01e0ac09bcf9b6558e2c58bd39
        ssize_t send(int fd, const char* data, size_t size, int flags);
        void flushClient(int fd);
        void closeTimedOutClients();
    public:
        Server();
        Server(const Server& other);
        Server& operator=(const Server& other);
        ~Server();
        bool executeCgi(const std::string& path,const Request& req,const LocationConfig& location,std::string& output);
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

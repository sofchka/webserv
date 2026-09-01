#ifndef CONFIG_HPP
#define CONFIG_HPP

#include <unistd.h>
#include <fcntl.h>
#include <iostream>
#include <stdlib.h>
#include <cstddef>
#include <map>
#include <string>
#include <vector>

struct LocationConfig
{
    std::string path;                                      // "/", "/upload"
    std::vector<std::string> methods;                      // GET, POST, DELETE
    std::string root;                                      // "web"
    std::string redir;                                     // "/new-page"
    bool autoindex;                                        // on/off(true/false)
    std::string index;                                     // "index.html"
    bool upload_enabled;                                   // true = upload on
    std::string upload_store;                              // "web/uploads"
    std::map<std::string, std::string> cgi_extensions;     // ".py" -> "/usr/bin/python3"

    LocationConfig();
    LocationConfig(const LocationConfig& other);
    LocationConfig& operator=(const LocationConfig& other);
    ~LocationConfig();
};

struct ServerConfig
{
    std::string host;                                      // "0.0.0.0"
    int port;                                              // 8080
    std::vector<std::string> server_names;                 // Host names
    std::map<int, std::string> error_pages;                // 404 -> "web/error/404.html"
    std::size_t client_max_body_size;                      // 1000000 = 1MB
    std::vector<LocationConfig> locations;

    ServerConfig();
    ServerConfig(const ServerConfig& other);
    ServerConfig& operator=(const ServerConfig& other);
    ~ServerConfig();
};

class Config
{
    private:
        std::vector<ServerConfig> _servers;

    public:
        Config();
        Config(const Config& other);
        Config& operator=(const Config& other);
        ~Config();

        bool load(const std::string& path);
        bool ParseConfigFile(int fd);
        const std::vector<ServerConfig>& getServers() const;
        const ServerConfig* findServer(const std::string& host, int port) const;
        const LocationConfig* findLocation(const ServerConfig& server,
                                           const std::string& request_path) const;
};

std::string trimConfigLine(const std::string& line);
std::vector<std::string> splitCleanTokens(const std::string& line, int* k);
bool parseListenValue(const std::string& value, std::string& host, int& port);
bool parseSizeValue(const std::string& value, std::size_t& size);
bool isMethodAllowed(const LocationConfig& location, const std::string& method);
bool hasConfExtention(const std::string& filename);

#endif

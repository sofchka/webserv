#ifndef CONFIG_HPP
#define CONFIG_HPP

#include <cstddef>
#include <map>
#include <string>
#include <vector>

struct LocationConfig
{
    std::string path;
    std::vector<std::string> methods;
    std::string root;
    std::string redirect;
    bool autoindex;
    std::string index;
    bool upload_enabled;
    std::string upload_store;
    std::map<std::string, std::string> cgi_extensions;

    LocationConfig();
};

struct ServerConfig
{
    std::string host;
    int port;
    std::map<int, std::string> error_pages;
    std::size_t client_max_body_size;
    std::vector<LocationConfig> locations;

    ServerConfig();
};

class Config
{
    private:
        std::vector<ServerConfig> _servers;

    public:
        Config();

        bool load(const std::string& path);
        bool validate() const;
        const std::vector<ServerConfig>& getServers() const;
        const ServerConfig* findServer(const std::string& host, int port) const;
        const LocationConfig* findLocation(const ServerConfig& server,
                                           const std::string& request_path) const;
};

std::string trimConfigLine(const std::string& line);
std::vector<std::string> splitConfigTokens(const std::string& line);
bool parseListenValue(const std::string& value, std::string& host, int& port);
bool parseSizeValue(const std::string& value, std::size_t& size);
bool isMethodAllowed(const LocationConfig& location, const std::string& method);

#endif

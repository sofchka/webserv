#include "../includes/Config.hpp"

LocationConfig::LocationConfig()
    : autoindex(false),
      upload_enabled(false)
{
}

ServerConfig::ServerConfig()
    : host("0.0.0.0"),
      port(8080),
      client_max_body_size(1000000)
{
}

Config::Config()
{
}

bool Config::load(const std::string& path)
{
    (void)path;
    return false;
}

bool Config::validate() const
{
    return false;
}

const std::vector<ServerConfig>& Config::getServers() const
{
    return _servers;
}

const ServerConfig* Config::findServer(const std::string& host, int port) const
{
    (void)host;
    (void)port;
    return NULL;
}

const LocationConfig* Config::findLocation(const ServerConfig& server,
                                           const std::string& request_path) const
{
    (void)server;
    (void)request_path;
    return NULL;
}

std::string trimConfigLine(const std::string& line)
{
    return line;
}

std::vector<std::string> splitConfigTokens(const std::string& line)
{
    (void)line;
    return std::vector<std::string>();
}

bool parseListenValue(const std::string& value, std::string& host, int& port)
{
    (void)value;
    (void)host;
    (void)port;
    return false;
}

bool parseSizeValue(const std::string& value, std::size_t& size)
{
    (void)value;
    (void)size;
    return false;
}

bool isMethodAllowed(const LocationConfig& location, const std::string& method)
{
    (void)location;
    (void)method;
    return false;
}

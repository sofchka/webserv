#include "../../includes/Config.hpp"

Config::Config()
{
}

Config::Config(const Config& other)
{
    *this = other;
}

Config& Config::operator=(const Config& other)
{
    if (this != &other)
        _servers = other._servers;
    return *this;
}

Config::~Config()
{
}
 
const std::vector<ServerConfig>& Config::getServers() const
{
    return _servers;
}

const ServerConfig* Config::findServer(const std::string& host, int port) const
{
    for (size_t i = 0; i < _servers.size(); i++)
    {
        if (_servers[i].host == host && _servers[i].port == port)
            return &_servers[i];
    }
    return NULL;
}

const LocationConfig* Config::findLocation(const ServerConfig& server,
                                           const std::string& request_path) const
{
    const LocationConfig* best_match = NULL;

    for (size_t i = 0; i < server.locations.size(); i++)
    {
        const std::string& path = server.locations[i].path;

        if (request_path.find(path) == 0
            && (best_match == NULL || path.size() > best_match->path.size()))
            best_match = &server.locations[i];
    }
    return best_match;
}

bool isMethodAllowed(const LocationConfig& location, const std::string& method)
{
    for (size_t i = 0; i < location.methods.size(); i++)
    {
        if (location.methods[i] == method)
            return true;
    }
    return false;
}

LocationConfig::LocationConfig() : autoindex(false), upload_enabled(false)
{
}

LocationConfig::LocationConfig(const LocationConfig& other)
{
    *this = other;
}

LocationConfig& LocationConfig::operator=(const LocationConfig& other)
{
    if (this != &other)
    {
        path = other.path;
        methods = other.methods;
        root = other.root;
        redir = other.redir;
        autoindex = other.autoindex;
        index = other.index;
        upload_enabled = other.upload_enabled;
        upload_store = other.upload_store;
        cgi_extensions = other.cgi_extensions;
    }
    return *this;
}

LocationConfig::~LocationConfig()
{
}

ServerConfig::ServerConfig() : host("0.0.0.0"), port(8080), client_max_body_size(1000000)
{
}

ServerConfig::ServerConfig(const ServerConfig& other)
{
    *this = other;
}

ServerConfig& ServerConfig::operator=(const ServerConfig& other)
{
    if (this != &other)
    {
        host = other.host;
        port = other.port;
        error_pages = other.error_pages;
        client_max_body_size = other.client_max_body_size;
        locations = other.locations;
    }
    return *this;
}

ServerConfig::~ServerConfig()
{
}

bool Config::load(const std::string& path)
{
    if (path.empty() || hasConfExtention(path) == false)
    {
        std::cerr << "Error: invalid config file extension\n";
        return false;
    }
    int fd = open(path.c_str(), O_RDONLY);
    if (fd < 0)
    {
        std::cerr << "Error: Permission Denied\nCould not open config file: " << path << std::endl;
        return false;
    }
    bool state = ParseConfigFile(fd);
    close(fd);
    return state;
}

bool Config::ParseConfigFile(int fd)
{
    int bytes_read;
    char buffer[4096];
    std::string content;

    while ((bytes_read = read(fd, buffer, sizeof(buffer))) > 0) // config file into string
        content.append(buffer, bytes_read);
    if (bytes_read < 0)
    {
        std::cerr << "Error: failed to read config file" << std::endl;
        return false;
    }

    ServerConfig currentServer;
    LocationConfig currentLocation;
    bool inServer = false;
    bool inLocation = false;
    std::string line;

    for (size_t i = 0; i <= content.size(); ++i)
    {
        if (i < content.size() && content[i] != '\n')
        {
            line += content[i];
            continue;
        }
        line = trimConfigLine(line);

        if (line.empty())
        {
            line.clear();
            continue;
        }
        int k = 0;
        std::vector<std::string> tokens = splitCleanTokens(line, &k);
        if (tokens.empty() && k == 0)
        {
            line.clear();
            continue;
        }

        // starting block for server
        if (k == 0 && tokens[0] == "server")
        {
            if (inServer)
            {
                std::cerr << "Error: nested server block" << std::endl;
                return false;
            }
            if (tokens.size() != 1)
            {
                std::cerr << "Error: invalid server block syntax" << std::endl;
                return false;
            }
            currentServer = ServerConfig();
            inServer = true;

            line.clear();
            continue;
        }

        // starting block for location
        if (k == 0 && tokens[0] == "location")
        {
            if (!inServer)
            {
                std::cerr << "Error: location block outside server block" << std::endl;
                return false;
            }
            if (inLocation)
            {
                std::cerr << "Error: nested location block" << std::endl;
                return false;
            }
            if (tokens.size() != 2)
            {
                std::cerr << "Error: invalid location block syntax" << std::endl;
                return false;
            }
            currentLocation = LocationConfig();
            currentLocation.path = tokens[1];
            inLocation = true;

            line.clear();
            continue;
        }

        // Closing blocks
        if (k == 1)
        {
            k = 0;
            if (tokens.size() != 0 && line != "}")
            {
                std::cerr << "Error: invalid closing bracket syntax" << std::endl;
                return false;
            }

            if (inLocation)
            {
                currentServer.locations.push_back(currentLocation);
                inLocation = false;
            }
            else if (inServer)
            {
                if (currentServer.locations.empty())
                {
                    std::cerr << "Error: server block has no location" << std::endl;
                    return false;
                }

                _servers.push_back(currentServer);
                inServer = false;
            }
            else
            {
                std::cerr << "Error: unmatched closing bracket" << std::endl;
                return false;
            }

            line.clear();
            continue;
        }

        // Server 
        if (inServer && !inLocation)
        {
            if (tokens[0] == "listen")
            {
                if (tokens.size() != 2)
                {
                    std::cerr << "Error: invalid listen directive" << std::endl;
                    return false;
                }

                if (!parseListenValue(tokens[1], currentServer.host, currentServer.port))
                {
                    std::cerr << "Error: invalid listen value" << std::endl;
                    return false;
                }
            }
            else if (tokens[0] == "error_page")
            {
                if (tokens.size() != 3)
                {
                    std::cerr << "Error: invalid error_page directive" << std::endl;
                    return false;
                }

                int code = atoi(tokens[1].c_str());

                if (code < 300 || code > 599)
                {
                    std::cerr << "Error: invalid error_page code" << std::endl;
                    return false;
                }
                if (access(tokens[2].c_str(), F_OK) != 0)
                {
                    std::cerr << "Error: error_page file does not exist: " << tokens[2] << std::endl;
                    return false;
                }
                currentServer.error_pages[code] = tokens[2];
            }
            else if (tokens[0] == "client_max_body_size")
            {
                if (tokens.size() != 2)
                {
                    std::cerr << "Error: invalid client_max_body_size directive" << std::endl;
                    return false;
                }

                if (!parseSizeValue(tokens[1], currentServer.client_max_body_size))
                {
                    std::cerr << "Error: invalid client_max_body_size value" << std::endl;
                    return false;
                }
            }
            else
            {
                std::cerr << "Error: unknown server directive: " << tokens[0] << std::endl;
                return false;
            }
        }

        // LOCATION DIRECTIVES
        else if (inServer && inLocation)
        {
            if (tokens[0] == "root")
            {
                if (tokens.size() != 2)
                {
                    std::cerr << "Error: invalid root directive" << std::endl;
                    return false;
                }
                if (access(tokens[1].c_str(), F_OK) != 0)
                {
                    std::cerr << "Error: root directory does not exist: " << tokens[1] << std::endl;
                    return false;
                }
                currentLocation.root = tokens[1];
            }
            else if (tokens[0] == "index")
            {
                if (tokens.size() != 2)
                {
                    std::cerr << "Error: invalid index directive" << std::endl;
                    return false;
                }
                if (access((currentLocation.root + "/" + tokens[1]).c_str(), F_OK) != 0)
                {
                    std::cerr << "Error: index file does not exist: " << currentLocation.root + "/" + tokens[1] << std::endl;
                    return false;
                }
                currentLocation.index = tokens[1];
            }
            else if (tokens[0] == "methods")
            {
                if (tokens.size() < 2)
                {
                    std::cerr << "Error: invalid allow_methods directive" << std::endl;
                    return false;
                }

                currentLocation.methods.clear();
                
                for (size_t j = 1; j < tokens.size(); ++j)
                {
                    if (tokens[j] != "GET" &&
                        tokens[j] != "POST" &&
                        tokens[j] != "DELETE")
                    {
                        std::cerr << "Error: invalid HTTP method: " << tokens[j] << std::endl;
                        return false;
                    }

                    currentLocation.methods.push_back(tokens[j]);
                }
            }
            else if (tokens[0] == "autoindex")
            {
                if (tokens.size() != 2)
                {
                    std::cerr << "Error: invalid autoindex directive" << std::endl;
                    return false;
                }
                if (tokens[1] == "on")
                    currentLocation.autoindex = true;
                else if (tokens[1] == "off")
                    currentLocation.autoindex = false;
                else
                {
                    std::cerr << "Error: invalid autoindex value" << std::endl;
                    return false;
                }
            }
            else if (tokens[0] == "return")
            {
                if (tokens.size() != 2)
                {
                    std::cerr << "Error: invalid return directive" << std::endl;
                    return false;
                }
                if (access(tokens[1].c_str(), F_OK) != 0)
                {
                    std::cerr << "Error: redirection target does not exist: " << tokens[1] << std::endl;
                    return false;
                }
                currentLocation.redir = tokens[1];
            }
            else if (tokens[0] == "upload")
            {
                if (tokens.size() != 2)
                {
                    std::cerr << "Error: invalid upload directive" << std::endl;
                    return false;
                }

                if (tokens[1] == "on")
                    currentLocation.upload_enabled = true;
                else if (tokens[1] == "off")
                    currentLocation.upload_enabled = false;
                else
                {
                    std::cerr << "Error: invalid upload value" << std::endl;
                    return false;
                }
            }
            else if (tokens[0] == "upload_store")
            {
                if (tokens.size() != 2)
                {
                    std::cerr << "Error: invalid upload_store directive" << std::endl;
                    return false;
                }
                if (access(tokens[1].c_str(), F_OK) != 0)
                {
                    std::cerr << "Error: upload_store directory does not exist: " << tokens[1] << std::endl;
                    return false;
                }
                currentLocation.upload_store = tokens[1];
            }
            else if (tokens[0] == "cgi")
            {
                if (tokens.size() != 3)
                {
                    std::cerr << "Error: invalid cgi directive" << std::endl;
                    return false;
                }

                currentLocation.cgi_extensions[tokens[1]] = tokens[2];
            }
            else
            {
                std::cerr << "Error: unknown location directive: " << tokens[0] << std::endl;
                return false;
            }
        }
        else
        {
            std::cerr << "Error: directive outside server block" << std::endl;
            return false;
        }

        line.clear();
    }

    if (inLocation)
    {
        std::cerr << "Error: unclosed location block" << std::endl;
        return false;
    }

    if (inServer)
    {
        std::cerr << "Error: unclosed server block" << std::endl;
        return false;
    }

    if (_servers.empty())
    {
        std::cerr << "Error: no server block found" << std::endl;
        return false;
    }

    return true;
}

#include "../../includes/Config.hpp"

Config::Config()
{
}

bool Config::load(const std::string& path)
{
    if (path.empty() || hasConfExtention(path) == false)
        return false;
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

// LocationConfig::LocationConfig() : autoindex(false), upload_enabled(false)
// {
// }

// ServerConfig::ServerConfig() : host("0.0.0.0"), port(8080), client_max_body_size(1000000)
// {
// }

bool Config::ParseConfigFile(int fd)
{
    int locationBlockLevel = 0;
    int serverBlockLevel = 0;
    int bytes_read;
    char buffer[4096];
    while ((bytes_read = read(fd, buffer, sizeof(buffer) - 1)) > 0)
    {
        buffer[bytes_read] = '\0';
        int i = 0;
        while (i < bytes_read && buffer[i] != '\0')
        {
            std::string line;
            while (i < bytes_read && buffer[i] != '\n')
            {
                line += buffer[i];
                i++;
            }
            if (i < bytes_read && buffer[i] == '\n')
                i++;
            line = trimConfigLine(line); // cleaning comments and whitespace
            if (line.empty())
                continue;
            if (isServerBlockStart(line)) // server block starts
            {
                serverBlockLevel = 1;
            }
            if (lineHasClosedBracket(line)) // block ends
            {
                if (locationBlockLevel == 1)
                    locationBlockLevel = 0;
                else if (serverBlockLevel == 1)
                    serverBlockLevel = 0;
                else
                {
                    std::cerr << "Error: Unmatched closing bracket\n";
                    return false;
                }
            }
            if (isLocationBlockStart(line)) // location block starts
            {
                if (serverBlockLevel == 0)
                {
                    std::cerr << "Error: location block outside of server block\n";
                    return false;
                }
                locationBlockLevel = 1;
            }
            std::vector<std::string> tokens = splitCleanTokens(line); // split line into tokens
            if (tokens.empty())
                continue;
            if (serverBlockLevel == 1 && locationBlockLevel == 0)
            {
                // ..
            }
            else if (serverBlockLevel == 1 && locationBlockLevel == 1)
            {
                //LocationConfig 
            }
        }
    }
    if (bytes_read < 0)
    {
        std::cerr << "Error: failed to read config file" << std::endl;
        return false;
    }
    if (serverBlockLevel != 0 || locationBlockLevel != 0)
    {
        std::cerr << "Error: unclosed block in config file" << std::endl;
        return false;
    }
    return true;
}

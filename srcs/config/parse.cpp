#include "../../includes/Config.hpp"

bool parseListenValue(const std::string& value, std::string& host, int& port)
{
    size_t colon = value.find(':');
    std::string port_part;

    if (colon == std::string::npos)
    {
        host = "0.0.0.0";
        port_part = value;
    }
    else
    {
        host = value.substr(0, colon);
        port_part = value.substr(colon + 1);
    }

    if (port_part.empty())
        return false;
    for (size_t i = 0; i < port_part.size(); i++)
    {
        if (port_part[i] < '0' || port_part[i] > '9')
            return false;
    }
    port = atoi(port_part.c_str());
    if (port < 1 || port > 65535)
        return false;
    return true;
}

bool parseSizeValue(const std::string& value, std::size_t& size)
{
    // TODO: Support common size suffixes if the config format should allow them
    // later, for example 1K, 1M, or 1G.
    if (value.empty())
        return false;
    for (size_t i = 0; i < value.size(); i++)
    {
        if (value[i] < '0' || value[i] > '9')
            return false;
    }
    size = static_cast<std::size_t>(atol(value.c_str()));
    return true;
}

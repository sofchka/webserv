#include "../../includes/Config.hpp"
#include <limits>

static bool isDigit(char c)
{
    return c >= '0' && c <= '9';
}

static bool parseUnsignedNumber(const std::string& value,
                                size_t end,
                                std::size_t& number)
{
    if (end == 0)
        return false;

    number = 0;
    for (size_t i = 0; i < end; i++)
    {
        if (!isDigit(value[i]))
            return false;
        if (number > (std::numeric_limits<std::size_t>::max()
            - static_cast<std::size_t>(value[i] - '0')) / 10)
            return false;
        number = number * 10 + static_cast<std::size_t>(value[i] - '0');
    }
    return true;
}

static bool parsePort(const std::string& value, int& port)
{
    std::size_t number;

    if (!parseUnsignedNumber(value, value.size(), number))
        return false;
    if (number < 1 || number > 65535)
        return false;
    port = static_cast<int>(number);
    return true;
}

static bool parseHostByte(const std::string& value, std::size_t start,
                          std::size_t end)
{
    std::size_t number;

    if (end <= start || end - start > 3)
        return false;
    if (!parseUnsignedNumber(value.substr(start, end - start),
                             end - start,
                             number))
        return false;
    return number <= 255;
}

static bool isValidIPv4(const std::string& host)
{
    std::size_t start = 0;
    int parts = 0;

    if (host.empty())
        return false;
    for (std::size_t i = 0; i <= host.size(); i++)
    {
        if (i == host.size() || host[i] == '.')
        {
            if (!parseHostByte(host, start, i))
                return false;
            start = i + 1;
            parts++;
        }
    }
    return parts == 4;
}

bool parseListenValue(const std::string& value, std::string& host, int& port)
{
    size_t colon = value.find(':');
    std::string new_host;
    std::string port_part;
    int new_port;

    if (colon == std::string::npos)
    {
        new_host = "0.0.0.0";
        port_part = value;
    }
    else
    {
        if (value.find(':', colon + 1) != std::string::npos)
            return false;
        new_host = value.substr(0, colon);
        port_part = value.substr(colon + 1);
    }

    if (!isValidIPv4(new_host) || !parsePort(port_part, new_port))
        return false;
    host = new_host;
    port = new_port;
    return true;
}

bool parseSizeValue(const std::string& value, std::size_t& size)
{
    size_t end;
    std::size_t number;
    std::size_t multiplier;

    if (value.empty())
        return false;
    end = value.size();
    multiplier = 1;
    if (value[end - 1] == 'K' || value[end - 1] == 'k')
    {
        multiplier = 1024;
        end--;
    }
    else if (value[end - 1] == 'M' || value[end - 1] == 'm')
    {
        multiplier = 1024 * 1024;
        end--;
    }
    else if (value[end - 1] == 'G' || value[end - 1] == 'g')
    {
        multiplier = 1024 * 1024 * 1024;
        end--;
    }

    if (!parseUnsignedNumber(value, end, number))
        return false;
    if (number > std::numeric_limits<std::size_t>::max() / multiplier)
        return false;
    size = number * multiplier;
    return true;
}

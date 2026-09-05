#include "../includes/Server.hpp"
#include <cctype>
#include <limits>

Request::Request()
    : valid(false),
      content_length(0),
      chunked(false)
{
}

static bool is_token_char(unsigned char c)
{
    static const char separators[] = "()<>@,;:\\\"/[]?={} \t";

    return c > 32 && c < 127 && std::strchr(separators, c) == NULL;
}

static std::string lower_header_name(const std::string& value)
{
    std::string out = value;

    for (size_t i = 0; i < out.size(); ++i)
        out[i] = static_cast<char>(std::tolower(
            static_cast<unsigned char>(out[i])));
    return out;
}

static void trim_span(const std::string& s, size_t& first, size_t& last)
{
    while (first < last &&
           (s[first] == ' ' || s[first] == '\t'))
        ++first;
    while (last > first &&
           (s[last - 1] == ' ' || s[last - 1] == '\t'))
        --last;
}

static bool parse_content_length(const std::string& value, size_t& length)
{
    size_t out = 0;

    if (value.empty())
        return false;
    for (size_t i = 0; i < value.size(); ++i)
    {
        unsigned char c = static_cast<unsigned char>(value[i]);

        if (!std::isdigit(c))
            return false;
        if (out > (std::numeric_limits<size_t>::max()
                   - static_cast<size_t>(c - '0')) / 10)
            return false;
        out = out * 10 + static_cast<size_t>(c - '0');
    }
    length = out;
    return true;
}

static int chunk_hex_value(char c)
{
    if (c >= '0' && c <= '9')
        return c - '0';
    if (c >= 'a' && c <= 'f')
        return c - 'a' + 10;
    if (c >= 'A' && c <= 'F')
        return c - 'A' + 10;
    return -1;
}

static bool parse_chunk_size(const std::string& line, size_t& size)
{
    size_t out = 0;
    size_t i = 0;

    if (line.empty())
        return false;
    while (i < line.size() && line[i] != ';')
    {
        int digit = chunk_hex_value(line[i]);

        if (digit < 0)
            return false;
        if (out > (std::numeric_limits<size_t>::max()
                   - static_cast<size_t>(digit)) / 16)
            return false;
        out = out * 16 + static_cast<size_t>(digit);
        i++;
    }
    if (i == 0)
        return false;
    size = out;
    return true;
}

static bool decode_chunked_body(const std::string& encoded,
                                std::string& decoded)
{
    size_t pos = 0;

    decoded.clear();
    while (true)
    {
        size_t line_end = encoded.find("\r\n", pos);
        size_t chunk_size = 0;

        if (line_end == std::string::npos)
            return false;
        if (!parse_chunk_size(encoded.substr(pos, line_end - pos),
                              chunk_size))
            return false;
        pos = line_end + 2;
        if (chunk_size == 0)
        {
            while (true)
            {
                line_end = encoded.find("\r\n", pos);
                if (line_end == std::string::npos)
                    return false;
                if (line_end == pos)
                    return true;
                pos = line_end + 2;
            }
        }
        if (chunk_size > encoded.size() - pos)
            return false;
        decoded.append(encoded, pos, chunk_size);
        pos += chunk_size;
        if (pos + 2 > encoded.size() ||
            encoded[pos] != '\r' || encoded[pos + 1] != '\n')
            return false;
        pos += 2;
    }
}

static bool valid_method(const std::string& method)
{
    if (method.empty())
        return false;
    for (size_t i = 0; i < method.size(); ++i)
    {
        if (!is_token_char(static_cast<unsigned char>(method[i])))
            return false;
    }
    return true;
}

static bool valid_version(const std::string& version)
{
    return version == "HTTP/1.0" || version == "HTTP/1.1";
}

Request parse_f(std::string request)
{
    Request req;
    size_t line_end = request.find("\r\n");

    if (line_end == std::string::npos || line_end == 0)
        return req;

    std::string f_line = request.substr(0, line_end);
    size_t pos1 = f_line.find(' ');
    size_t pos2;
    size_t headers_end;

    if (pos1 == std::string::npos || pos1 == 0)
        return req;
    pos2 = f_line.find(' ', pos1 + 1);
    if (pos2 == std::string::npos || pos2 == pos1 + 1 ||
        f_line.find(' ', pos2 + 1) != std::string::npos)
        return req;

    req.method = f_line.substr(0, pos1);
    req.path = f_line.substr(pos1 + 1, pos2 - pos1 - 1);
    req.version = f_line.substr(pos2 + 1);
    if (!valid_method(req.method) || req.path.empty() ||
        req.path[0] != '/' || !valid_version(req.version))
        return Request();

    size_t query_pos = req.path.find('?');
    if (query_pos != std::string::npos)
    {
        req.query = req.path.substr(query_pos + 1);
        req.path.erase(query_pos);
        if (req.path.empty())
            req.path = "/";
    }

    headers_end = request.find("\r\n\r\n", line_end + 2);
    if (headers_end == std::string::npos)
        return Request();

    bool saw_content_length = false;
    size_t pos = line_end + 2;
    while (pos < headers_end)
    {
        size_t end = request.find("\r\n", pos);
        size_t colon;
        size_t name_first = pos;
        size_t name_last;
        size_t value_first;
        size_t value_last;
        std::string name;
        std::string value;

        if (end == std::string::npos || end > headers_end || end == pos)
            return Request();
        colon = request.find(':', pos);
        if (colon == std::string::npos || colon >= end || colon == pos)
            return Request();
        name_last = colon;
        trim_span(request, name_first, name_last);
        if (name_first == name_last)
            return Request();
        for (size_t i = name_first; i < name_last; ++i)
        {
            if (!is_token_char(static_cast<unsigned char>(request[i])))
                return Request();
        }
        value_first = colon + 1;
        value_last = end;
        trim_span(request, value_first, value_last);
        name = lower_header_name(request.substr(name_first,
                                                name_last - name_first));
        value = request.substr(value_first, value_last - value_first);
        req.headers[name] = value;
        if (name == "content-length")
        {
            size_t parsed = 0;

            if (!parse_content_length(value, parsed))
                return Request();
            if (saw_content_length && parsed != req.content_length)
                return Request();
            saw_content_length = true;
            req.content_length = parsed;
        }
        else if (name == "transfer-encoding")
        {
            std::string lowered = lower_header_name(value);

            if (lowered.find("chunked") != std::string::npos)
                req.chunked = true;
        }
        pos = end + 2;
    }

    req.body = request.substr(headers_end + 4);
    if (req.chunked)
    {
        std::string decoded;

        if (!decode_chunked_body(req.body, decoded))
            return Request();
        req.body = decoded;
        req.content_length = req.body.size();
    }
    else if (saw_content_length && req.body.size() > req.content_length)
        req.body.erase(req.content_length);
    req.valid = true;
    return req;
}

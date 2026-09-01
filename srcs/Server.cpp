#include "../includes/Server.hpp"
#include <dirent.h>
#include <sys/stat.h>
#include <cstdio>
#include <cerrno>
#include <cctype>
#include <limits>
#include <ctime>
<<<<<<< HEAD
#include <sys/time.h>

static const size_t MAX_HEADER_SIZE = 8192;
static const size_t MAX_REQUEST_LINE_SIZE = 4096;
static const int CLIENT_TIMEOUT_SECONDS = 10;
=======

static const size_t MAX_REQUEST_LINE_SIZE = 4096;
static const size_t MAX_HEADER_SIZE = 8192;
static const size_t MAX_REQUEST_BUFFER_SIZE = 16 * 1024 * 1024;
static const time_t CLIENT_TIMEOUT_SECONDS = 30;
>>>>>>> ac58abdad79afd01e0ac09bcf9b6558e2c58bd39

static std::string joinPath(const std::string& left, const std::string& right)
{
    if (right.empty() || right == "/")
        return left;
    if (!left.empty() && left[left.size() - 1] == '/')
        return left + right.substr(right[0] == '/');
    if (right[0] == '/')
        return left + right;
    return left + "/" + right;
}

static std::string localPath(const LocationConfig& location,
                             const std::string& request_path)
{
    std::string path = request_path;
    size_t q = path.find('?');
    if (q != std::string::npos)
        path = path.substr(0, q);

    std::string relative = path;
    if (location.path != "/" &&
        relative.compare(0, location.path.size(), location.path) == 0)
    {
        relative = relative.substr(location.path.size());
    }
    if (relative.empty())
        relative = "/";

    return joinPath(location.root, relative);
}

static std::string extensionOf(const std::string& path)
{
    size_t slash = path.find_last_of('/');
    size_t dot = path.find_last_of('.');

    if (dot == std::string::npos ||
        (slash != std::string::npos && dot < slash))
        return "";
    return path.substr(dot);
}

static bool findCgiInterpreter(const LocationConfig& location,
                               const std::string& path,
                               std::string& interpreter)
{
    std::map<std::string, std::string>::const_iterator it;

    it = location.cgi_extensions.find(extensionOf(path));
    if (it == location.cgi_extensions.end())
        return false;
    interpreter = it->second;
    return true;
}

static bool isDirectory(const std::string& path)
{
    struct stat st;

    return stat(path.c_str(), &st) == 0 && S_ISDIR(st.st_mode);
}

static bool existsPath(const std::string& path)
{
    struct stat st;

    return stat(path.c_str(), &st) == 0;
}

static int hexValue(char c)
{
    if (c >= '0' && c <= '9')
        return c - '0';
    if (c >= 'a' && c <= 'f')
        return c - 'a' + 10;
    if (c >= 'A' && c <= 'F')
        return c - 'A' + 10;
    return -1;
}

static std::string decodePathForSecurity(const std::string& path)
{
    std::string decoded;

    for (size_t i = 0; i < path.size(); i++)
    {
        if (path[i] == '%' && i + 2 < path.size())
        {
            int high = hexValue(path[i + 1]);
            int low = hexValue(path[i + 2]);

            if (high >= 0 && low >= 0)
            {
                decoded += static_cast<char>(high * 16 + low);
                i += 2;
                continue;
            }
        }
        decoded += path[i];
    }
    return decoded;
}

static bool hasPathTraversal(const std::string& path)
{
    std::string decoded = decodePathForSecurity(path);
    size_t start = 0;

    if (decoded.find('\0') != std::string::npos ||
        decoded.find('\\') != std::string::npos)
        return true;
    while (start <= decoded.size())
    {
        size_t slash = decoded.find('/', start);
        std::string part;

        if (slash == std::string::npos)
            part = decoded.substr(start);
        else
            part = decoded.substr(start, slash - start);
        if (part == "..")
            return true;
        if (slash == std::string::npos)
            break;
        start = slash + 1;
    }
    return false;
}

static std::string statusText(int status)
{
    switch (status)
    {
        case 200: return "200 OK";
        case 201: return "201 Created";
        case 204: return "204 No Content";
        case 301: return "301 Moved Permanently";
        case 302: return "302 Found";
        case 400: return "400 Bad Request";
        case 403: return "403 Forbidden";
        case 404: return "404 Not Found";
        case 500: return "500 Internal Server Error";
        case 502: return "502 Bad Gateway";
    }
    if (status >= 100 && status <= 599)
        return to_str(status) + " CGI Status";
    return "200 OK";
}

static std::string headerValue(const Request& req, const std::string& name)
{
    std::map<std::string, std::string>::const_iterator it;

    it = req.headers.find(name);
    if (it == req.headers.end())
        return "";
    return it->second;
}

<<<<<<< HEAD
static std::string lowerValue(const std::string& value)
=======
static std::string lowerAscii(const std::string& value)
>>>>>>> ac58abdad79afd01e0ac09bcf9b6558e2c58bd39
{
    std::string out = value;

    for (size_t i = 0; i < out.size(); i++)
        out[i] = static_cast<char>(std::tolower(
            static_cast<unsigned char>(out[i])));
    return out;
}

<<<<<<< HEAD
static std::string hostNameFromHeader(const std::string& value)
{
    size_t first = 0;
    size_t last = value.size();
    size_t colon;

    while (first < last && (value[first] == ' ' || value[first] == '\t'))
        first++;
    while (last > first && (value[last - 1] == ' ' ||
                            value[last - 1] == '\t'))
        last--;
    if (first == last)
        return "";
    if (value[first] == '[')
    {
        size_t closing = value.find(']', first + 1);

        if (closing != std::string::npos && closing < last)
            return lowerValue(value.substr(first + 1, closing - first - 1));
    }
    colon = value.find(':', first);
    if (colon != std::string::npos && colon < last)
        last = colon;
    while (last > first && value[last - 1] == '.')
        last--;
    return lowerValue(value.substr(first, last - first));
=======
static bool parseHostPort(const std::string& value,
                          std::string& host,
                          int& port,
                          bool& has_port)
{
    size_t colon = value.find(':');
    size_t end = value.size();

    host = value;
    port = 0;
    has_port = false;
    if (colon == std::string::npos)
    {
        host = lowerAscii(value);
        return !host.empty();
    }
    if (value.find(':', colon + 1) != std::string::npos || colon == 0)
        return false;
    host = lowerAscii(value.substr(0, colon));
    if (colon + 1 == end)
        return false;
    has_port = true;
    for (size_t i = colon + 1; i < end; i++)
    {
        if (!std::isdigit(static_cast<unsigned char>(value[i])))
            return false;
        port = port * 10 + (value[i] - '0');
        if (port > 65535)
            return false;
    }
    return !host.empty() && port > 0;
}

static bool sameListenSocket(const ServerConfig& left,
                             const ServerConfig& right)
{
    return left.host == right.host && left.port == right.port;
}

static bool canServeAcceptedSocket(const ServerConfig& candidate,
                                   const ServerConfig& accepted)
{
    if (candidate.port != accepted.port)
        return false;
    return candidate.host == accepted.host ||
           candidate.host == "0.0.0.0" ||
           accepted.host == "0.0.0.0";
>>>>>>> ac58abdad79afd01e0ac09bcf9b6558e2c58bd39
}

static bool serverNameMatches(const ServerConfig& server,
                              const std::string& host)
{
<<<<<<< HEAD
    if (host.empty())
        return false;
    for (size_t i = 0; i < server.server_names.size(); i++)
    {
        if (lowerValue(server.server_names[i]) == host)
            return true;
    }
    return lowerValue(server.host) == host;
}

static const ServerConfig& selectServerConfigByHost(
    const std::vector<ServerConfig>& servers,
    size_t default_index,
    const std::string& host_header)
{
    std::string host = hostNameFromHeader(host_header);
    std::string listen_host = servers[default_index].host;
    int port = servers[default_index].port;

    for (size_t i = 0; i < servers.size(); i++)
    {
        if (servers[i].host == listen_host && servers[i].port == port &&
            serverNameMatches(servers[i], host))
            return servers[i];
    }
    return servers[default_index];
}

static const ServerConfig& selectServerConfig(
    const std::vector<ServerConfig>& servers,
    size_t default_index,
    const Request& req)
{
    return selectServerConfigByHost(servers,
                                    default_index,
                                    headerValue(req, "host"));
}

static std::string rawHeaderValue(const std::string& request,
                                  const std::string& header_name)
{
    size_t line_end = request.find("\r\n");
    size_t headers_end = request.find("\r\n\r\n");
    size_t pos;

    if (line_end == std::string::npos || headers_end == std::string::npos)
        return "";
    pos = line_end + 2;
    while (pos < headers_end)
    {
        size_t end = request.find("\r\n", pos);
        size_t colon;
        std::string name;
        std::string value;

        if (end == std::string::npos || end > headers_end)
            break;
        colon = request.find(':', pos);
        if (colon != std::string::npos && colon < end)
        {
            name = request.substr(pos, colon - pos);
            value = request.substr(colon + 1, end - colon - 1);
            if (lowerValue(name) == header_name)
                return value;
        }
        pos = end + 2;
    }
    return "";
=======
    if (lowerAscii(server.host) == host)
        return true;
    for (size_t i = 0; i < server.server_names.size(); i++)
    {
        if (lowerAscii(server.server_names[i]) == host)
            return true;
    }
    return false;
}

static size_t selectServerIndex(const std::vector<ServerConfig>& servers,
                                size_t default_index,
                                const Request& req)
{
    if (default_index >= servers.size())
        return default_index;

    std::string host;
    int host_port;
    bool has_port;
    std::string host_header = headerValue(req, "host");

    if (host_header.empty() ||
        !parseHostPort(host_header, host, host_port, has_port))
        return default_index;
    if (has_port && host_port != servers[default_index].port)
        return default_index;
    for (size_t i = 0; i < servers.size(); i++)
    {
        if (canServeAcceptedSocket(servers[i], servers[default_index]) &&
            serverNameMatches(servers[i], host))
            return i;
    }
    return default_index;
>>>>>>> ac58abdad79afd01e0ac09bcf9b6558e2c58bd39
}

static std::string basenameOf(const std::string& path)
{
    size_t slash = path.find_last_of('/');

    if (slash == std::string::npos || slash + 1 >= path.size())
        return "upload.bin";
    return path.substr(slash + 1);
}

<<<<<<< HEAD
static std::string uploadedFilename(const std::string& filename)
{
    size_t slash = filename.find_last_of("/\\");
    std::string base;

    if (slash == std::string::npos)
        base = filename;
    else
        base = filename.substr(slash + 1);
    if (base.empty() || base == "." || base == "..")
        return "";
    return base;
=======
static std::string trimSpaces(const std::string& value)
{
    size_t first = 0;
    size_t last = value.size();

    while (first < last &&
           (value[first] == ' ' || value[first] == '\t'))
        first++;
    while (last > first &&
           (value[last - 1] == ' ' || value[last - 1] == '\t'))
        last--;
    return value.substr(first, last - first);
}

static std::string unquoteHeaderValue(const std::string& value)
{
    std::string out;

    if (value.size() < 2 || value[0] != '"' ||
        value[value.size() - 1] != '"')
        return value;
    for (size_t i = 1; i + 1 < value.size(); i++)
    {
        if (value[i] == '\\' && i + 2 < value.size())
            i++;
        out += value[i];
    }
    return out;
}

static bool headerParameter(const std::string& header,
                            const std::string& name,
                            std::string& value)
{
    size_t start = 0;
    std::string wanted = lowerAscii(name);

    while (start <= header.size())
    {
        size_t semi = header.find(';', start);
        size_t equals;
        std::string part;

        if (semi == std::string::npos)
            part = header.substr(start);
        else
            part = header.substr(start, semi - start);
        part = trimSpaces(part);
        equals = part.find('=');
        if (equals != std::string::npos &&
            lowerAscii(trimSpaces(part.substr(0, equals))) == wanted)
        {
            value = unquoteHeaderValue(trimSpaces(part.substr(equals + 1)));
            return true;
        }
        if (semi == std::string::npos)
            break;
        start = semi + 1;
    }
    return false;
}

static bool multipartBoundary(const Request& req,
                              std::string& boundary,
                              bool& bad_request)
{
    std::string content_type = headerValue(req, "content-type");
    size_t semi = content_type.find(';');
    std::string media_type;

    bad_request = false;
    if (semi == std::string::npos)
        media_type = trimSpaces(content_type);
    else
        media_type = trimSpaces(content_type.substr(0, semi));
    if (lowerAscii(media_type) != "multipart/form-data")
        return false;
    if (!headerParameter(content_type, "boundary", boundary) ||
        boundary.empty())
    {
        bad_request = true;
        return true;
    }
    return true;
}

static bool safeUploadFilename(const std::string& raw,
                               std::string& filename)
{
    size_t slash = raw.find_last_of("/\\");

    if (slash == std::string::npos)
        filename = raw;
    else
        filename = raw.substr(slash + 1);
    if (filename.empty() || filename == "." || filename == "..")
        return false;
    if (filename.find('/') != std::string::npos ||
        filename.find('\\') != std::string::npos ||
        filename.find("..") != std::string::npos)
        return false;
    return true;
}

static bool parsePartHeaders(const std::string& headers,
                             std::map<std::string, std::string>& out)
{
    size_t pos = 0;

    out.clear();
    while (pos < headers.size())
    {
        size_t end = headers.find("\r\n", pos);
        size_t colon;
        std::string name;
        std::string value;

        if (end == std::string::npos)
            end = headers.size();
        if (end == pos)
        {
            pos = end + 2;
            continue;
        }
        colon = headers.find(':', pos);
        if (colon == std::string::npos || colon >= end || colon == pos)
            return false;
        name = lowerAscii(trimSpaces(headers.substr(pos, colon - pos)));
        value = trimSpaces(headers.substr(colon + 1, end - colon - 1));
        if (name.empty())
            return false;
        out[name] = value;
        if (end == headers.size())
            break;
        pos = end + 2;
    }
    return true;
}

static bool writeUploadFile(const std::string& directory,
                            const std::string& filename,
                            const std::string& content)
{
    std::string upload_path = joinPath(directory, filename);
    std::ofstream upload(upload_path.c_str(),
                         std::ios::out | std::ios::binary);

    if (!upload.is_open())
        return false;
    upload.write(content.data(), content.size());
    return upload.good();
}

static int saveMultipartUploads(const Request& req,
                                const LocationConfig& location,
                                const std::string& boundary)
{
    std::string delimiter = "--" + boundary;
    size_t pos = 0;
    int uploaded = 0;

    if (req.body.compare(0, delimiter.size(), delimiter) != 0)
        return -1;
    pos = delimiter.size();
    while (pos < req.body.size())
    {
        std::map<std::string, std::string> part_headers;
        std::map<std::string, std::string>::const_iterator disposition;
        std::string filename;
        size_t headers_end;
        size_t next_delimiter;
        size_t content_start;
        size_t content_end;

        if (req.body.compare(pos, 2, "--") == 0)
            return uploaded;
        if (req.body.compare(pos, 2, "\r\n") != 0)
            return -1;
        pos += 2;
        headers_end = req.body.find("\r\n\r\n", pos);
        if (headers_end == std::string::npos)
            return -1;
        if (!parsePartHeaders(req.body.substr(pos, headers_end - pos),
                              part_headers))
            return -1;
        content_start = headers_end + 4;
        next_delimiter = req.body.find("\r\n" + delimiter, content_start);
        if (next_delimiter == std::string::npos)
            return -1;
        content_end = next_delimiter;
        disposition = part_headers.find("content-disposition");
        if (disposition != part_headers.end() &&
            headerParameter(disposition->second, "filename", filename) &&
            !filename.empty())
        {
            if (!safeUploadFilename(filename, filename))
                return -1;
            if (!writeUploadFile(location.upload_store,
                                 filename,
                                 req.body.substr(content_start,
                                                 content_end - content_start)))
                return -2;
            uploaded++;
        }
        pos = next_delimiter + 2 + delimiter.size();
    }
    return -1;
>>>>>>> ac58abdad79afd01e0ac09bcf9b6558e2c58bd39
}

static std::string lowerHeaderName(const std::string& value)
{
    std::string out = value;

    for (size_t i = 0; i < out.size(); i++)
        out[i] = static_cast<char>(std::tolower(
            static_cast<unsigned char>(out[i])));
    return out;
}

static void trimHeaderSpan(const std::string& s, size_t& first, size_t& last)
{
    while (first < last && (s[first] == ' ' || s[first] == '\t'))
        first++;
    while (last > first && (s[last - 1] == ' ' || s[last - 1] == '\t'))
        last--;
}

static bool parseHeaderContentLength(const std::string& value, size_t& length)
{
    size_t out = 0;

    if (value.empty())
        return false;
    for (size_t i = 0; i < value.size(); i++)
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

static bool hasTransferEncodingChunked(const std::string& value)
{
    std::string lowered = lowerHeaderName(value);
    size_t start = 0;

    while (start <= lowered.size())
    {
        size_t comma = lowered.find(',', start);
        size_t first = start;
        size_t last;

        if (comma == std::string::npos)
            last = lowered.size();
        else
            last = comma;
        trimHeaderSpan(lowered, first, last);
        if (lowered.substr(first, last - first) == "chunked")
            return true;
        if (comma == std::string::npos)
            break;
        start = comma + 1;
    }
    return false;
}

static std::string trimHeaderValue(const std::string& value)
{
    size_t first = 0;
    size_t last = value.size();

    trimHeaderSpan(value, first, last);
    return value.substr(first, last - first);
}

static std::string unquoteHeaderValue(const std::string& value)
{
    std::string out;

    if (value.size() < 2 || value[0] != '"' ||
        value[value.size() - 1] != '"')
        return value;
    for (size_t i = 1; i + 1 < value.size(); i++)
    {
        if (value[i] == '\\' && i + 2 < value.size())
            i++;
        out += value[i];
    }
    return out;
}

static bool headerParamValue(const std::string& header_value,
                             const std::string& param_name,
                             std::string& param_value)
{
    size_t start = 0;

    while (start <= header_value.size())
    {
        size_t semi = header_value.find(';', start);
        size_t end;
        size_t eq;
        std::string part;

        if (semi == std::string::npos)
            end = header_value.size();
        else
            end = semi;
        part = trimHeaderValue(header_value.substr(start, end - start));
        eq = part.find('=');
        if (eq != std::string::npos &&
            lowerValue(trimHeaderValue(part.substr(0, eq))) == param_name)
        {
            param_value = unquoteHeaderValue(
                trimHeaderValue(part.substr(eq + 1)));
            return true;
        }
        if (semi == std::string::npos)
            break;
        start = semi + 1;
    }
    return false;
}

static bool multipartBoundary(const Request& req, std::string& boundary)
{
    std::string content_type = headerValue(req, "content-type");
    std::string media_type;
    size_t semi = content_type.find(';');

    if (semi == std::string::npos)
        media_type = trimHeaderValue(content_type);
    else
        media_type = trimHeaderValue(content_type.substr(0, semi));
    if (lowerValue(media_type) != "multipart/form-data")
        return false;
    if (!headerParamValue(content_type, "boundary", boundary))
        return false;
    return !boundary.empty();
}

static std::string multipartFilename(const std::string& part_headers)
{
    size_t pos = 0;

    while (pos < part_headers.size())
    {
        size_t end = part_headers.find("\r\n", pos);
        size_t colon;
        std::string name;
        std::string value;

        if (end == std::string::npos)
            end = part_headers.size();
        colon = part_headers.find(':', pos);
        if (colon != std::string::npos && colon < end)
        {
            name = lowerValue(trimHeaderValue(
                part_headers.substr(pos, colon - pos)));
            value = trimHeaderValue(
                part_headers.substr(colon + 1, end - colon - 1));
            if (name == "content-disposition")
            {
                std::string filename;

                if (headerParamValue(value, "filename", filename))
                    return uploadedFilename(filename);
            }
        }
        if (end == part_headers.size())
            break;
        pos = end + 2;
    }
    return "";
}

static bool writeUploadFile(const std::string& upload_store,
                            const std::string& filename,
                            const std::string& content)
{
    std::string upload_path = joinPath(upload_store, filename);
    std::ofstream upload(upload_path.c_str(),
                         std::ios::out | std::ios::binary);

    if (!upload.is_open())
        return false;
    upload.write(content.data(), content.size());
    return upload.good();
}

static bool saveMultipartUploads(const Request& req,
                                 const LocationConfig& location,
                                 int& status)
{
    std::string boundary;
    std::string delimiter;
    size_t pos;
    size_t saved = 0;

    status = 400;
    if (!multipartBoundary(req, boundary))
        return false;
    delimiter = "--" + boundary;
    pos = req.body.find(delimiter);
    if (pos == std::string::npos)
        return false;
    while (true)
    {
        size_t headers_start;
        size_t headers_end;
        size_t data_start;
        size_t next_delimiter;
        std::string filename;
        std::string content;

        pos += delimiter.size();
        if (req.body.compare(pos, 2, "--") == 0)
            break;
        if (req.body.compare(pos, 2, "\r\n") != 0)
            return false;
        headers_start = pos + 2;
        headers_end = req.body.find("\r\n\r\n", headers_start);
        if (headers_end == std::string::npos)
            return false;
        data_start = headers_end + 4;
        next_delimiter = req.body.find("\r\n" + delimiter, data_start);
        if (next_delimiter == std::string::npos)
            return false;
        filename = multipartFilename(
            req.body.substr(headers_start, headers_end - headers_start));
        if (!filename.empty())
        {
            content = req.body.substr(data_start, next_delimiter - data_start);
            status = 500;
            if (!writeUploadFile(location.upload_store, filename, content))
                return false;
            saved++;
            status = 400;
        }
        pos = next_delimiter + 2;
    }
    return saved > 0;
}

static bool parseChunkSizeLine(const std::string& line, size_t& size)
{
    size_t out = 0;
    size_t i = 0;

    if (line.empty())
        return false;
    while (i < line.size() && line[i] != ';')
    {
        int digit = hexValue(line[i]);

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

static bool hasCompleteChunkedBody(const std::string& request,
                                   size_t body_start,
                                   bool& bad_request,
                                   size_t& decoded_length)
{
    size_t pos = body_start;

    decoded_length = 0;
    while (true)
    {
        size_t line_end = request.find("\r\n", pos);
        size_t chunk_size = 0;

        if (line_end == std::string::npos)
            return false;
        if (!parseChunkSizeLine(request.substr(pos, line_end - pos),
                                chunk_size))
        {
            bad_request = true;
            return true;
        }
        pos = line_end + 2;
        if (chunk_size == 0)
        {
            while (true)
            {
                line_end = request.find("\r\n", pos);
                if (line_end == std::string::npos)
                    return false;
                if (line_end == pos)
                    return true;
                pos = line_end + 2;
            }
        }
        if (chunk_size > std::numeric_limits<size_t>::max() - decoded_length)
        {
            bad_request = true;
            return true;
        }
        decoded_length += chunk_size;
        if (chunk_size > request.size() - pos)
            return false;
        pos += chunk_size;
        if (pos + 2 > request.size())
            return false;
        if (request[pos] != '\r' || request[pos + 1] != '\n')
        {
            bad_request = true;
            return true;
        }
        pos += 2;
    }
}

static bool hasCompleteRequestBody(const std::string& request,
                                   bool& bad_request,
                                   size_t& content_length)
{
    size_t headers_end = request.find("\r\n\r\n");
    bool saw_content_length = false;
    bool chunked = false;
    size_t line_end;
    size_t pos;

    bad_request = false;
    content_length = 0;
    if (headers_end == std::string::npos)
        return false;
    line_end = request.find("\r\n");
    if (line_end == std::string::npos)
        return true;
    pos = line_end + 2;
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
        size_t parsed = 0;

        if (end == std::string::npos || end > headers_end)
            break;
        colon = request.find(':', pos);
        if (colon == std::string::npos || colon >= end)
        {
            pos = end + 2;
            continue;
        }
        name_last = colon;
        trimHeaderSpan(request, name_first, name_last);
        value_first = colon + 1;
        value_last = end;
        trimHeaderSpan(request, value_first, value_last);
        name = lowerHeaderName(request.substr(name_first,
                                              name_last - name_first));
        if (name == "content-length")
        {
            value = request.substr(value_first, value_last - value_first);
            if (!parseHeaderContentLength(value, parsed) ||
                (saw_content_length && parsed != content_length))
            {
                bad_request = true;
                return true;
            }
            saw_content_length = true;
            content_length = parsed;
        }
        else if (name == "transfer-encoding")
        {
            value = request.substr(value_first, value_last - value_first);
            if (hasTransferEncodingChunked(value))
                chunked = true;
        }
        pos = end + 2;
    }
    if (chunked)
        return hasCompleteChunkedBody(request,
                                      headers_end + 4,
                                      bad_request,
                                      content_length);
    if (!saw_content_length)
        return true;
    return request.size() >= headers_end + 4 + content_length;
}

static std::string autoIndexPage(const std::string& request_path,
                                 const std::string& full_path)
{
    DIR* dir = opendir(full_path.c_str());
    std::string body = "<html><body><h1>Index of " + request_path +
                       "</h1><ul>";

    if (dir == NULL)
        return "";
    struct dirent* entry;
    while ((entry = readdir(dir)) != NULL)
    {
        std::string name = entry->d_name;
        std::string href = request_path;

        if (href.empty() || href[href.size() - 1] != '/')
            href += "/";
        href += name;
        body += "<li><a href=\"" + href + "\">" + name + "</a></li>";
    }
    closedir(dir);
    body += "</ul></body></html>";
    return body;
}

static std::string httpHeaderNameForCgi(const std::string& header)
{
    std::string name = "HTTP_";

    for (size_t i = 0; i < header.size(); i++)
    {
        if (header[i] == '-')
            name += '_';
        else
            name += static_cast<char>(std::toupper(
                static_cast<unsigned char>(header[i])));
    }
    return name;
}

static void addEnv(std::vector<std::string>& env,
                   const std::string& name,
                   const std::string& value)
{
    env.push_back(name + "=" + value);
}

static std::vector<std::string> buildCgiEnv(const std::string& path,
                                            const Request& req)
{
    std::vector<std::string> env;
    std::map<std::string, std::string>::const_iterator it;
    std::string content_length = headerValue(req, "content-length");
    std::string content_type = headerValue(req, "content-type");

    addEnv(env, "GATEWAY_INTERFACE", "CGI/1.1");
    addEnv(env, "SERVER_PROTOCOL", req.version);
    addEnv(env, "SERVER_SOFTWARE", "webserv");
    addEnv(env, "REQUEST_METHOD", req.method);
    addEnv(env, "SCRIPT_FILENAME", path);
    addEnv(env, "SCRIPT_NAME", req.path);
    addEnv(env, "QUERY_STRING", req.query);
    addEnv(env, "PATH_INFO", "");
    addEnv(env, "REDIRECT_STATUS", "200");
    addEnv(env, "CONTENT_LENGTH", content_length);
    addEnv(env, "CONTENT_TYPE", content_type);
    for (it = req.headers.begin(); it != req.headers.end(); ++it)
    {
        if (it->first != "content-length" && it->first != "content-type")
            addEnv(env, httpHeaderNameForCgi(it->first), it->second);
    }
    return env;
}

static char** makeEnvp(std::vector<std::string>& env)
{
    char** envp = new char*[env.size() + 1];

    for (size_t i = 0; i < env.size(); i++)
        envp[i] = const_cast<char*>(env[i].c_str());
    envp[env.size()] = NULL;
    return envp;
}

static bool writeAllToFd(int fd, const std::string& data)
{
    size_t offset = 0;

    while (offset < data.size())
    {
        ssize_t written = write(fd,
                                data.c_str() + offset,
                                data.size() - offset);

        if (written < 0 && errno == EINTR)
            continue;
        if (written <= 0)
            return false;
        offset += static_cast<size_t>(written);
    }
    return true;
}

static int parseCgiStatus(const std::string& value)
{
    size_t i = 0;
    int status = 0;

    while (i < value.size() && value[i] == ' ')
        i++;
    while (i < value.size() && std::isdigit(
        static_cast<unsigned char>(value[i])))
    {
        status = status * 10 + value[i] - '0';
        i++;
    }
    if (status < 100 || status > 599)
        return 200;
    return status;
}

static std::string buildCgiHttpResponse(const std::string& cgi_output)
{
    size_t header_end = cgi_output.find("\r\n\r\n");
    size_t separator_len = 4;
    std::string headers_block;
    std::string body;
    std::string content_type = "text/html";
    std::vector<std::string> headers;
    int status = 200;

    if (header_end == std::string::npos)
    {
        header_end = cgi_output.find("\n\n");
        separator_len = 2;
    }
    if (header_end == std::string::npos)
    {
        headers_block = "";
        body = cgi_output;
    }
    else
    {
        headers_block = cgi_output.substr(0, header_end);
        body = cgi_output.substr(header_end + separator_len);
    }

    for (size_t pos = 0; pos < headers_block.size();)
    {
        size_t end = headers_block.find('\n', pos);
        std::string line;
        size_t colon;
        size_t first;
        size_t last;
        std::string name;
        std::string value;

        if (end == std::string::npos)
            end = headers_block.size();
        line = headers_block.substr(pos, end - pos);
        if (!line.empty() && line[line.size() - 1] == '\r')
            line.erase(line.size() - 1);
        colon = line.find(':');
        if (colon != std::string::npos)
        {
            first = 0;
            last = colon;
            trimHeaderSpan(line, first, last);
            name = line.substr(first, last - first);
            first = colon + 1;
            last = line.size();
            trimHeaderSpan(line, first, last);
            value = line.substr(first, last - first);
            if (lowerHeaderName(name) == "status")
                status = parseCgiStatus(value);
            else if (lowerHeaderName(name) == "content-type")
                content_type = value;
            else if (lowerHeaderName(name) != "content-length" &&
                     lowerHeaderName(name) != "connection")
                headers.push_back(name + ": " + value);
            if (lowerHeaderName(name) == "location" && status == 200)
                status = 302;
        }
        pos = end + 1;
    }

    std::string response;

    response += "HTTP/1.1 ";
    response += statusText(status);
    response += "\r\nConnection: close\r\nContent-Type: ";
    response += content_type;
    response += "\r\nContent-Length: ";
    response += to_str(body.size());
    for (size_t i = 0; i < headers.size(); i++)
        response += "\r\n" + headers[i];
    response += "\r\n\r\n";
    response += body;
    return response;
}

Server::Server()
{
}

Server::Server(const Server& other)
{
    *this = other;
}

Server& Server::operator=(const Server& other)
{
    if (this != &other)
    {
        clients = other.clients;
        config = other.config;
        server_fds = other.server_fds;
        listener_defaults = other.listener_defaults;
        server_configs = other.server_configs;
        listener_servers = other.listener_servers;
        client_servers = other.client_servers;
        read_buffer = other.read_buffer;
        write_buffer = other.write_buffer;
        write_offset = other.write_offset;
        close_after_write = other.close_after_write;
<<<<<<< HEAD
        client_activity = other.client_activity;
=======
        client_last_activity = other.client_last_activity;
>>>>>>> ac58abdad79afd01e0ac09bcf9b6558e2c58bd39
    }

    return *this;
}

Server::~Server()
{
    for (size_t i = 0; i < clients.size(); i++)
        close(clients[i]);
    for (size_t i = 0; i < server_fds.size(); i++)
        close(server_fds[i]);
}

void Server::init(const std::string& config_path)
{
    if (!config.load(config_path))
        exit(1);

    const std::vector<ServerConfig>& servers = config.getServers();

    for (size_t i = 0; i < servers.size(); i++)
    {
        size_t config_index = server_configs.size();
        bool existing_listener = false;

        for (size_t j = 0; j < server_configs.size(); j++)
        {
            if (sameListenSocket(server_configs[j], servers[i]))
            {
                existing_listener = true;
                break;
            }
        }

        server_configs.push_back(servers[i]);
        if (existing_listener)
            continue;

        int server_fd;
        bool already_listening = false;

        for (size_t j = 0; j < server_configs.size(); j++)
        {
            if (server_configs[j].host == servers[i].host &&
                server_configs[j].port == servers[i].port)
            {
                already_listening = true;
                break;
            }
        }

        server_configs.push_back(servers[i]);
        if (already_listening)
            continue;

        server_fd = socket(AF_INET, SOCK_STREAM, 0);

        if (server_fd < 0)
        {
            perror("socket");
            exit(1);
        }

        fcntl(server_fd, F_SETFL, O_NONBLOCK);

        int opt = 1;

        setsockopt(server_fd,
                   SOL_SOCKET,
                   SO_REUSEADDR,
                   &opt,
                   sizeof(opt));

        struct sockaddr_in addr;

        memset(&addr, 0, sizeof(addr));

        addr.sin_family = AF_INET;
        if (servers[i].host == "0.0.0.0")
            addr.sin_addr.s_addr = INADDR_ANY;
        else
            addr.sin_addr.s_addr = inet_addr(servers[i].host.c_str());
        addr.sin_port = htons(servers[i].port);

        if (bind(server_fd,
                 (struct sockaddr*)&addr,
                 sizeof(addr)) < 0)
        {
            perror("bind");
            close(server_fd);
            exit(1);
        }

        if (listen(server_fd, 10) < 0)
        {
            perror("listen");
            close(server_fd);
            exit(1);
        }

        server_fds.push_back(server_fd);
<<<<<<< HEAD
        listener_servers.push_back(server_configs.size() - 1);
=======
        listener_defaults.push_back(config_index);
>>>>>>> ac58abdad79afd01e0ac09bcf9b6558e2c58bd39

        std::cout << "Server started on "
                  << servers[i].host << ":" << servers[i].port << "\n";
    }
}

void Server::acceptClient(size_t server_index)
{
    int client_fd;

    client_fd = accept(server_fds[server_index], NULL, NULL);

    if (client_fd < 0)
        return;

    fcntl(client_fd, F_SETFL, O_NONBLOCK);

    clients.push_back(client_fd);
<<<<<<< HEAD
    client_servers[client_fd] = listener_servers[server_index];
    client_activity[client_fd] = std::time(NULL);
=======
    client_servers[client_fd] = listener_defaults[server_index];
    client_last_activity[client_fd] = std::time(NULL);
>>>>>>> ac58abdad79afd01e0ac09bcf9b6558e2c58bd39

    std::cout << "New client connected\n";
}

void Server::closeClientNow(int fd)
{
    close(fd);
    clients.erase(
        std::remove(clients.begin(), clients.end(), fd),
        clients.end()
    );
    client_servers.erase(fd);
    read_buffer.erase(fd);
    write_buffer.erase(fd);
    write_offset.erase(fd);
    close_after_write.erase(fd);
    client_activity.erase(fd);
}

void Server::closeClient(int fd)
{
    if (write_buffer.find(fd) != write_buffer.end())
    {
        close_after_write[fd] = true;
        return;
    }
<<<<<<< HEAD
    closeClientNow(fd);
}

void Server::replyErrorAndClose(int fd,
                                int status,
                                const std::string& message,
                                const ServerConfig& server)
{
    std::string resp = make_response(status,
                                     message,
                                     "text/plain",
                                     &server);

    send(fd,
         resp.c_str(),
         resp.size(),
         0);
    closeClient(fd);
}

void Server::closeIdleClients()
{
    std::time_t now = std::time(NULL);
    size_t i = 0;

    while (i < clients.size())
    {
        int fd = clients[i];
        std::map<int, std::time_t>::iterator it = client_activity.find(fd);

        if (it != client_activity.end() &&
            now - it->second >= CLIENT_TIMEOUT_SECONDS)
        {
            std::map<int, size_t>::iterator server_index;

            server_index = client_servers.find(fd);
            if (server_index != client_servers.end() &&
                server_index->second < server_configs.size())
                replyErrorAndClose(fd,
                                   408,
                                   "Request Timeout",
                                   server_configs[server_index->second]);
            else
                closeClientNow(fd);
            if (i < clients.size() && clients[i] == fd)
                i++;
        }
        else
            i++;
    }
=======
    close(fd);
    clients.erase(
        std::remove(clients.begin(), clients.end(), fd),
        clients.end()
    );
    client_servers.erase(fd);
    read_buffer.erase(fd);
    write_offset.erase(fd);
    close_after_write.erase(fd);
    client_last_activity.erase(fd);
}

const ServerConfig* Server::defaultServerForClient(int fd) const
{
    std::map<int, size_t>::const_iterator it = client_servers.find(fd);

    if (it == client_servers.end() || it->second >= server_configs.size())
        return NULL;
    return &server_configs[it->second];
}

void Server::sendErrorAndClose(int fd, int status, const std::string& message)
{
    const ServerConfig* server = defaultServerForClient(fd);
    std::string resp = make_response(status,
                                     message,
                                     "text/plain",
                                     server);

    send(fd, resp.c_str(), resp.size(), 0);
    closeClient(fd);
>>>>>>> ac58abdad79afd01e0ac09bcf9b6558e2c58bd39
}

ssize_t Server::send(int fd, const char* data, size_t size, int flags)
{
    (void)flags;
    if (size == 0)
        return 0;
    if (write_buffer.find(fd) == write_buffer.end())
        write_offset[fd] = 0;
    write_buffer[fd].append(data, size);
    return static_cast<ssize_t>(size);
}

void Server::flushClient(int fd)
{
    std::map<int, std::string>::iterator out = write_buffer.find(fd);

    if (out == write_buffer.end())
        return;

    size_t offset = write_offset[fd];
    ssize_t bytes = ::send(fd,
                           out->second.c_str() + offset,
                           out->second.size() - offset,
                           0);

    if (bytes < 0 && (errno == EAGAIN || errno == EWOULDBLOCK))
        return;
    if (bytes <= 0)
    {
        close(fd);
        clients.erase(
            std::remove(clients.begin(), clients.end(), fd),
            clients.end()
        );
        client_servers.erase(fd);
        read_buffer.erase(fd);
        write_buffer.erase(fd);
        write_offset.erase(fd);
        close_after_write.erase(fd);
<<<<<<< HEAD
        client_activity.erase(fd);
=======
        client_last_activity.erase(fd);
>>>>>>> ac58abdad79afd01e0ac09bcf9b6558e2c58bd39
        return;
    }

    client_last_activity[fd] = std::time(NULL);
    offset += static_cast<size_t>(bytes);
    client_activity[fd] = std::time(NULL);
    if (offset < out->second.size())
    {
        write_offset[fd] = offset;
        return;
    }

    write_buffer.erase(out);
    write_offset.erase(fd);
    if (close_after_write.erase(fd) != 0)
        closeClient(fd);
}

void Server::closeTimedOutClients()
{
    time_t now = std::time(NULL);
    size_t i = 0;

    while (i < clients.size())
    {
        int fd = clients[i];
        std::map<int, time_t>::iterator last = client_last_activity.find(fd);

        if (write_buffer.find(fd) != write_buffer.end())
        {
            i++;
            continue;
        }
        if (last == client_last_activity.end())
            client_last_activity[fd] = now;
        else if (now - last->second >= CLIENT_TIMEOUT_SECONDS)
        {
            sendErrorAndClose(fd, 408, "Request Timeout");
            if (i < clients.size() && clients[i] == fd)
                i++;
        }
        else
            i++;
    }
}

bool Server::executeCgi(const std::string& path,
                        const Request& req,
                        const LocationConfig& location,
                        std::string& output)
{
    std::string script;

    if (!findCgiInterpreter(location, path, script))
        return false;
    if (!existsPath(path))
        return false;

    int input_pipe[2];
    int output_pipe[2];

    if (pipe(input_pipe) < 0)
        return false;
    if (pipe(output_pipe) < 0)
    {
        close(input_pipe[0]);
        close(input_pipe[1]);
        return false;
    }

    pid_t pid = fork();

    if (pid == 0)
    {
        std::vector<std::string> env = buildCgiEnv(path, req);
        char** envp = makeEnvp(env);

        dup2(input_pipe[0], STDIN_FILENO);
        dup2(output_pipe[1], STDOUT_FILENO);
        close(input_pipe[0]);
        close(input_pipe[1]);
        close(output_pipe[0]);
        close(output_pipe[1]);

        char *args[] = {
            (char*)script.c_str(),
            (char*)path.c_str(),
            NULL
        };

        execve(script.c_str(), args, envp);
        exit(1);
    }
    if (pid < 0)
    {
        close(input_pipe[0]);
        close(input_pipe[1]);
        close(output_pipe[0]);
        close(output_pipe[1]);
        return false;
    }

    char buffer[1024];
    int status = 0;

    close(input_pipe[0]);
    close(output_pipe[1]);
    if (!writeAllToFd(input_pipe[1], req.body))
    {
        close(input_pipe[1]);
        close(output_pipe[0]);
        waitpid(pid, &status, 0);
        return false;
    }
    close(input_pipe[1]);

    while (true)
    {
        ssize_t n = read(output_pipe[0], buffer, sizeof(buffer));

        if (n < 0 && errno == EINTR)
            continue;
        if (n <= 0)
            break;
        output.append(buffer, n);
    }
    close(output_pipe[0]);
    waitpid(pid, &status, 0);
    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0)
        return false;
    return true;
}

void Server::handleClient(int fd)
{
    char buffer[1024];
    int bytes = recv(fd, buffer, sizeof(buffer), 0);
    bool bad_request;
    bool complete;
    size_t content_length;
    size_t headers_end;
    size_t request_line_end;
    std::map<int, size_t>::iterator server_index;

    if (bytes <= 0)
    {
        closeClient(fd);
        return;
    }
    client_activity[fd] = std::time(NULL);
    read_buffer[fd].append(buffer, bytes);
<<<<<<< HEAD
    server_index = client_servers.find(fd);
=======
    client_last_activity[fd] = std::time(NULL);

    if (read_buffer[fd].size() > MAX_REQUEST_BUFFER_SIZE)
    {
        sendErrorAndClose(fd, 413, "Payload Too Large");
        return;
    }

    size_t line_end = read_buffer[fd].find("\r\n");
    if ((line_end != std::string::npos && line_end > MAX_REQUEST_LINE_SIZE) ||
        (line_end == std::string::npos &&
         read_buffer[fd].size() > MAX_REQUEST_LINE_SIZE))
    {
        sendErrorAndClose(fd, 414, "URI Too Long");
        return;
    }

    size_t headers_end = read_buffer[fd].find("\r\n\r\n");
    if (headers_end == std::string::npos &&
        read_buffer[fd].size() > MAX_HEADER_SIZE)
    {
        sendErrorAndClose(fd, 431, "Request Header Fields Too Large");
        return;
    }
    if (headers_end != std::string::npos &&
        headers_end + 4 > MAX_HEADER_SIZE)
    {
        sendErrorAndClose(fd, 431, "Request Header Fields Too Large");
        return;
    }

    bool complete_request = hasCompleteRequestBody(read_buffer[fd],
                                                   bad_request,
                                                   content_length);
    if (bad_request)
    {
        sendErrorAndClose(fd, 400, "Bad Request");
        return;
    }
    if (!complete_request)
    {
        if (headers_end != std::string::npos && content_length > 0)
        {
            std::string header_only = read_buffer[fd].substr(0,
                                                            headers_end + 4);
            Request header_req = parse_f(header_only);
            std::map<int, size_t>::iterator server_index;

            server_index = client_servers.find(fd);
            if (server_index == client_servers.end())
            {
                closeClient(fd);
                return;
            }
            size_t current_index = server_index->second;

            if (header_req.valid)
                current_index = selectServerIndex(server_configs,
                                                  current_index,
                                                  header_req);
            if (current_index < server_configs.size() &&
                content_length > server_configs[current_index].client_max_body_size)
            {
                sendErrorAndClose(fd, 413, "Payload Too Large");
                return;
            }
        }
        return;
    }
    std::string raw_request = read_buffer[fd];
    Request req = parse_f(raw_request);
    read_buffer[fd].clear();
    std::map<int, size_t>::iterator server_index = client_servers.find(fd);
>>>>>>> ac58abdad79afd01e0ac09bcf9b6558e2c58bd39
    if (server_index == client_servers.end())
    {
        closeClient(fd);
        return;
    }
    const ServerConfig& default_server = server_configs[server_index->second];

<<<<<<< HEAD
    request_line_end = read_buffer[fd].find("\r\n");
    if (request_line_end == std::string::npos &&
        read_buffer[fd].size() > MAX_REQUEST_LINE_SIZE)
=======
    size_t current_index = selectServerIndex(server_configs,
                                             server_index->second,
                                             req);
    const ServerConfig& current_server = server_configs[current_index];
    if (content_length > current_server.client_max_body_size)
>>>>>>> ac58abdad79afd01e0ac09bcf9b6558e2c58bd39
    {
        replyErrorAndClose(fd,
                           414,
                           "URI Too Long",
                           default_server);
        return;
    }
    headers_end = read_buffer[fd].find("\r\n\r\n");
    if (headers_end == std::string::npos &&
        read_buffer[fd].size() > MAX_HEADER_SIZE)
    {
        replyErrorAndClose(fd,
                           431,
                           "Request Header Fields Too Large",
                           default_server);
        return;
    }
    if (headers_end != std::string::npos &&
        headers_end + 4 > MAX_HEADER_SIZE)
    {
        replyErrorAndClose(fd,
                           431,
                           "Request Header Fields Too Large",
                           default_server);
        return;
    }
    complete = hasCompleteRequestBody(read_buffer[fd],
                                      bad_request,
                                      content_length);
    if (bad_request)
    {
        replyErrorAndClose(fd,
                           400,
                           "Bad Request",
                           default_server);
        read_buffer[fd].clear();
        return;
    }
    if (headers_end != std::string::npos)
    {
        const ServerConfig& limit_server = selectServerConfigByHost(
            server_configs,
            server_index->second,
            rawHeaderValue(read_buffer[fd], "host"));

        if (content_length > limit_server.client_max_body_size)
        {
            replyErrorAndClose(fd,
                               413,
                               "Payload Too Large",
                               limit_server);
            read_buffer[fd].clear();
            return;
        }
    }
    if (!complete)
        return;
    std::string raw_request = read_buffer[fd];
    Request req = parse_f(raw_request);
    read_buffer[fd].clear();

    const ServerConfig& current_server = selectServerConfig(server_configs,
                                                            server_index->second,
                                                            req);
    if (!req.valid)
    {
        replyErrorAndClose(fd,
                           400,
                           "Bad Request",
                           current_server);
        return;
    }
    if (hasPathTraversal(req.path))
    {
        std::string resp;

        resp = make_response(403,
                             "Forbidden",
                             "text/plain",
                             &current_server);

        send(fd,
             resp.c_str(),
             resp.size(),
             0);

        closeClient(fd);

        return;
    }
    const LocationConfig* location = config.findLocation(current_server, req.path);
    if (location == NULL)
    {
        std::string resp;

        resp = make_response(404,
                             "Not Found",
                             "text/plain",
                             &current_server);

        send(fd,
             resp.c_str(),
             resp.size(),
             0);

        closeClient(fd);

        return;
    }
    if (!isMethodAllowed(*location, req.method))
    {
        std::string resp;

        resp = make_response(405,
                             "Method Not Allowed",
                             "text/plain",
                             &current_server);

        send(fd,
             resp.c_str(),
             resp.size(),
             0);

        closeClient(fd);

        return;
    }
    if (req.path == "/")
        req.path = "/" + location->index;
    std::string body = req.body;

    if (body.size() > current_server.client_max_body_size)
    {
        std::string resp;

        resp = make_response(413,
                             "Payload Too Large",
                             "text/plain",
                             &current_server);

        send(fd,
             resp.c_str(),
             resp.size(),
             0);

        closeClient(fd);

        return;
    }
    if (!location->redir.empty())
    {
        std::string response =
            "HTTP/1.1 302 Found\r\n"
            "Location: " + location->redir + "\r\n"
            "Content-Length: 0\r\n"
            "\r\n";

        send(fd,
             response.c_str(),
             response.size(),
             0);

        closeClient(fd);

        return;
    }

   std::string full_path = localPath(*location, req.path);

   std::string cgi_interpreter;
   if (findCgiInterpreter(*location, full_path, cgi_interpreter))
    {
        std::string output;

        if (!executeCgi(full_path, req, *location, output))
        {
            std::string resp;

            resp = make_response(502,
                                 "Bad Gateway",
                                 "text/plain",
                                 &current_server);

            send(fd, resp.c_str(), resp.size(), 0);
            closeClient(fd);
            return;
        }

        std::string resp = buildCgiHttpResponse(output);

        send(fd, resp.c_str(), resp.size(), 0);
        closeClient(fd);
        return;
    }
    if (req.method == "POST")
    {
        if (!location->upload_enabled)
        {
            std::string resp;

            resp = make_response(403,
                                 "Forbidden",
                                 "text/plain",
                                 &current_server);

            send(fd,
                 resp.c_str(),
                 resp.size(),
                 0);

            closeClient(fd);

            return;
        }

<<<<<<< HEAD
        if (lowerValue(headerValue(req, "content-type")).find(
                "multipart/form-data") == 0)
=======
        std::string boundary;
        bool bad_multipart;
        bool is_multipart = multipartBoundary(req, boundary, bad_multipart);

        if (is_multipart)
        {
            int uploaded;

            if (bad_multipart)
                uploaded = -1;
            else
                uploaded = saveMultipartUploads(req, *location, boundary);
            if (uploaded < 0)
            {
                std::string resp;
                int status = 400;
                std::string message = "Bad Request";

                if (uploaded == -2)
                {
                    status = 500;
                    message = "Internal Server Error";
                }
                resp = make_response(status,
                                     message,
                                     "text/plain",
                                     &current_server);

                send(fd,
                     resp.c_str(),
                     resp.size(),
                     0);

                closeClient(fd);

                return;
            }
            if (uploaded == 0)
            {
                std::string resp;

                resp = make_response(400,
                                     "No file uploaded",
                                     "text/plain",
                                     &current_server);

                send(fd,
                     resp.c_str(),
                     resp.size(),
                     0);

                closeClient(fd);

                return;
            }

            std::string resp = make_response(201,
                                             "Created",
                                             "text/plain",
                                             &current_server);

            send(fd,
                 resp.c_str(),
                 resp.size(),
                 0);

            closeClient(fd);

            return;
        }

        std::string upload_path = joinPath(location->upload_store,
                                          basenameOf(req.path));
        std::ofstream upload(upload_path.c_str(),
                             std::ios::out | std::ios::binary);

        if (!upload.is_open())
>>>>>>> ac58abdad79afd01e0ac09bcf9b6558e2c58bd39
        {
            int status = 400;

            if (!saveMultipartUploads(req, *location, status))
            {
                std::string message;
                std::string resp;

                if (status == 500)
                    message = "Internal Server Error";
                else
                    message = "Bad Request";
                resp = make_response(status,
                                     message,
                                     "text/plain",
                                     &current_server);

                send(fd,
                     resp.c_str(),
                     resp.size(),
                     0);

                closeClient(fd);

                return;
            }
        }
        else
        {
            if (!writeUploadFile(location->upload_store,
                                 basenameOf(req.path),
                                 body))
            {
                std::string resp;

                resp = make_response(500,
                                     "Internal Server Error",
                                     "text/plain",
                                     &current_server);

                send(fd,
                     resp.c_str(),
                     resp.size(),
                     0);

                closeClient(fd);

                return;
            }
        }
        std::string resp = make_response(201,
                                         "Created",
                                         "text/plain",
                                         &current_server);

        send(fd,
             resp.c_str(),
             resp.size(),
             0);

        closeClient(fd);

        return;
    }
    if (req.method == "DELETE")
    {
        if (!existsPath(full_path) || isDirectory(full_path))
        {
            std::string resp;

            resp = make_response(404,
                                 "Not Found",
                                 "text/plain",
                                 &current_server);

            send(fd,
                 resp.c_str(),
                 resp.size(),
                 0);

            closeClient(fd);

            return;
        }
        if (std::remove(full_path.c_str()) != 0)
        {
            std::string resp;

            resp = make_response(403,
                                 "Forbidden",
                                 "text/plain",
                                 &current_server);

            send(fd,
                 resp.c_str(),
                 resp.size(),
                 0);

            closeClient(fd);

            return;
        }

        std::string resp = make_response(204,
                                         "",
                                         "text/plain",
                                         &current_server);

        send(fd,
             resp.c_str(),
             resp.size(),
             0);

        closeClient(fd);

        return;
    }
    if (req.method != "GET")
    {
        std::string resp;

        resp = make_response(501,
                             "Not Implemented",
                             "text/plain",
                             &current_server);

        send(fd,
             resp.c_str(),
             resp.size(),
             0);

        closeClient(fd);

        return;
    }

    if (isDirectory(full_path))
    {
        std::string index_path = joinPath(full_path, location->index);

        if (!location->index.empty() && existsPath(index_path))
            full_path = index_path;
        else if (location->autoindex)
        {
            std::string listing = autoIndexPage(req.path, full_path);

            if (listing.empty())
            {
                std::string resp;

                resp = make_response(403,
                                     "Forbidden",
                                     "text/plain",
                                     &current_server);

                send(fd,
                     resp.c_str(),
                     resp.size(),
                     0);

                closeClient(fd);

                return;
            }

            std::string resp = make_response(200,
                                             listing,
                                             "text/html",
                                             &current_server);

            send(fd,
                 resp.c_str(),
                 resp.size(),
                 0);

            closeClient(fd);

            return;
        }
        else
        {
            std::string resp;

            resp = make_response(403,
                                 "Forbidden",
                                 "text/plain",
                                 &current_server);

            send(fd,
                 resp.c_str(),
                 resp.size(),
                 0);

            closeClient(fd);

            return;
        }
    }

    std::ifstream file(full_path.c_str(), std::ios::in | std::ios::binary);

    if (!file.is_open())
    {
        std::string resp;

        resp = make_response(404,
                             "Not Found",
                             "text/plain",
                             &current_server);

        send(fd,
             resp.c_str(),
             resp.size(),
             0);

        closeClient(fd);

        return;
    }

    std::string content(
        (std::istreambuf_iterator<char>(file)),
        std::istreambuf_iterator<char>()
    );

    std::string type;

    type = get_type(full_path);

    std::string response;

    response = make_response(200,
                             content,
                             type);

    send(fd,
         response.c_str(),
         response.size(),
         0);

    closeClient(fd);
}

void Server::run()
{
    while (true)
    {
        fd_set readfds;
        fd_set writefds;
        struct timeval timeout;
<<<<<<< HEAD

        closeIdleClients();
=======
>>>>>>> ac58abdad79afd01e0ac09bcf9b6558e2c58bd39

        closeTimedOutClients();
        FD_ZERO(&readfds);
        FD_ZERO(&writefds);
        timeout.tv_sec = 1;
        timeout.tv_usec = 0;

        int max_fd = -1;

        for (size_t i = 0; i < server_fds.size(); i++)
        {
            FD_SET(server_fds[i], &readfds);

            if (server_fds[i] > max_fd)
                max_fd = server_fds[i];
        }

        for (size_t i = 0; i < clients.size(); i++)
        {
            if (write_buffer.find(clients[i]) != write_buffer.end())
                FD_SET(clients[i], &writefds);
            else
                FD_SET(clients[i], &readfds);

            if (clients[i] > max_fd)
                max_fd = clients[i];
        }

        int activity;
        timeout.tv_sec = 1;
        timeout.tv_usec = 0;
        activity = select(max_fd + 1,
                          &readfds,
                          &writefds,
                          NULL,
                          &timeout);
        if (activity < 0)
        {
            perror("select");
            continue;
        }
        if (activity == 0)
            continue;

        for (size_t i = 0; i < server_fds.size(); i++)
        {
            if (FD_ISSET(server_fds[i], &readfds))
                acceptClient(i);
        }

        size_t i = 0;
        while (i < clients.size())
        {
            int client_fd = clients[i];

            if (FD_ISSET(client_fd, &readfds))
            {
                handleClient(client_fd);
                if (i < clients.size() && clients[i] == client_fd)
                    i++;
            }
            else if (FD_ISSET(client_fd, &writefds))
            {
                flushClient(client_fd);
                if (i < clients.size() && clients[i] == client_fd)
                    i++;
            }
            else
                i++;
        }
    }
}

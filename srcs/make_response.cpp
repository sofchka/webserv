#include "../includes/Server.hpp"
#include <ctime>

static std::string status_text(int status)
{
    switch (status)
    {
        case 100: return "100 Continue";
        case 101: return "101 Switching Protocols";
        case 200: return "200 OK";
        case 201: return "201 Created";
        case 202: return "202 Accepted";
        case 203: return "203 Non-Authoritative Information";
        case 204: return "204 No Content";
        case 205: return "205 Reset Content";
        case 206: return "206 Partial Content";
        case 300: return "300 Multiple Choices";
        case 301: return "301 Moved Permanently";
        case 302: return "302 Found";
        case 303: return "303 See Other";
        case 304: return "304 Not Modified";
        case 307: return "307 Temporary Redirect";
        case 308: return "308 Permanent Redirect";
        case 400: return "400 Bad Request";
        case 401: return "401 Unauthorized";
        case 402: return "402 Payment Required";
        case 403: return "403 Forbidden";
        case 404: return "404 Not Found";
        case 405: return "405 Method Not Allowed";
        case 406: return "406 Not Acceptable";
        case 408: return "408 Request Timeout";
        case 409: return "409 Conflict";
        case 410: return "410 Gone";
        case 411: return "411 Length Required";
        case 413: return "413 Payload Too Large";
        case 414: return "414 URI Too Long";
        case 415: return "415 Unsupported Media Type";
        case 418: return "418 I'm a teapot";
        case 426: return "426 Upgrade Required";
        case 429: return "429 Too Many Requests";
        case 431: return "431 Request Header Fields Too Large";
        case 500: return "500 Internal Server Error";
        case 501: return "501 Not Implemented";
        case 502: return "502 Bad Gateway";
        case 503: return "503 Service Unavailable";
        case 504: return "504 Gateway Timeout";
        case 505: return "505 HTTP Version Not Supported";
    }
    return "500 Internal Server Error";
}

static bool read_file(const std::string& path, std::string& body)
{
    std::ifstream file(path.c_str());

    if (!file.is_open())
        return false;
    body.assign((std::istreambuf_iterator<char>(file)),
                std::istreambuf_iterator<char>());
    return true;
}

static std::string generated_error_page(int status)
{
    std::string text = status_text(status);

    return "<html><head><title>" + text + "</title></head>"
           "<body><h1>" + text + "</h1></body></html>";
}

static bool error_page(int status,
                       const ServerConfig* server,
                       std::string& body)
{
    if (server != NULL)
    {
        std::map<int, std::string>::const_iterator it;

        it = server->error_pages.find(status);
        if (it != server->error_pages.end() && read_file(it->second, body))
            return true;
    }
    return read_file("web/error/" + to_str(status) + ".html", body);
}

static std::string http_date()
{
    char buffer[32];
    std::time_t now = std::time(NULL);
    std::tm* tm = std::gmtime(&now);

    if (tm == NULL)
        return "";
    if (std::strftime(buffer,
                      sizeof(buffer),
                      "%a, %d %b %Y %H:%M:%S GMT",
                      tm) == 0)
        return "";
    return std::string(buffer);
}

std::string make_response(int status,
                          const std::string& content,
                          const std::string& type)
{
    return make_response(status, content, type, NULL);
}

std::string make_response(int status,
                          const std::string& content,
                          const std::string& type,
                          const ServerConfig* server)
{
    std::string body = content;
    std::string content_type = type;

    if (status >= 400)
    {
        if (!error_page(status, server, body))
            body = generated_error_page(status);
        content_type = "text/html";
    }

    std::string date = http_date();
    std::string length = to_str(body.size());
    std::string response;

    response.reserve(96 + date.size() + content_type.size() + body.size());
    response += "HTTP/1.1 ";
    response += status_text(status);
    response += "\r\nDate: ";
    response += date;
    response += "\r\nConnection: close\r\nContent-Type: ";
    response += content_type;
    response += "\r\nContent-Length: ";
    response += length;
    response += "\r\n\r\n";
    response += body;

    return response;
}

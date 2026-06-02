#include "../includes/Server.hpp"

Request parse_f(std::string request)
{
    // TODO: Validate malformed request lines, parse headers, Content-Length,
    // Transfer-Encoding, query strings, and the request body.
    size_t line_end = request.find("\r\n");
    std::string f_line = request.substr(0, line_end);
    size_t pos1 = f_line.find(" ");
    size_t pos2 = f_line.find(" ", pos1 + 1);
    Request req;
    req.method = f_line.substr(0, pos1);
    req.path = f_line.substr(pos1 + 1, pos2 - pos1 - 1);
    req.version = f_line.substr(pos2 + 1);
    return req;
}

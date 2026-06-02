#include "../includes/Server.hpp"

static std::string status_text(int status)
{
    // TODO: Expand status coverage and serve configured/default error pages.
    if (status == 200)
        return "200 OK";
    if (status == 404)
        return "404 Not Found";
    if (status == 405)
        return "405 Method Not Allowed";
    return "500 Internal Server Error";
}

std::string make_response(int status,
                          const std::string& content,
                          const std::string& type)
{
    // TODO: Add required headers such as Connection and Date where appropriate.
    std::string body = content;

    std::string response =
        "HTTP/1.1 " + status_text(status) + "\r\n" +
        "Content-Type: " + type + "\r\n" +
        "Content-Length: " + to_str(body.size()) + "\r\n" +
        "\r\n" +
        body;

    return response;
}

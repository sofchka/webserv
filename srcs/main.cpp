#include "../includes/Server.hpp"

std::string to_str(size_t  len)
{
    std::stringstream ss;
    ss << len;
    return ss.str();
}
int main()
{
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0)
    {
        perror("socket");
        return 1;
    }

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(8080);

    if (bind(server_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0)
    {
        perror("bind");
        return 1;
    }

    if (listen(server_fd, 10) < 0)
    {
        perror("listen");
        return 1;
    }
    std::cout << "Server started on port 8080\n";
    while (true)
    {
        int client_fd = accept(server_fd, NULL, NULL);
        if (client_fd < 0)
        {
            perror("accept");
            continue;
        }
        char buffer[1024];
        memset(buffer, 0, sizeof(buffer));
        int bytes = recv(client_fd, buffer, sizeof(buffer), 0);
        if (bytes <= 0)
        {
            close(client_fd);
            continue;
        }
        std::string request(buffer);
        parse_f(request);
        std::string path = "/index.html";
        size_t line_end = request.find("\r\n");
        if (line_end != std::string::npos)
        {
            std::string first_line = request.substr(0, line_end);
            size_t pos1 = first_line.find(" ");
            size_t pos2 = first_line.find(" ", pos1 + 1);
            path = first_line.substr(pos1 + 1, pos2 - pos1 - 1);
        }

        if (path == "/")
            path = "/index.html";

        std::string full_path = "../web" + path;
        std::ifstream file(full_path.c_str());
        if (!file.is_open())
        {
            std::string resp =
                "HTTP/1.1 404 Not Found\r\n"
                "Content-Length: 0\r\n\r\n";

            send(client_fd, resp.c_str(), resp.size(), 0);
            close(client_fd);
            continue;
        }
        std::string content(
            (std::istreambuf_iterator<char>(file)),
            std::istreambuf_iterator<char>()
        );
        std::string len = to_str(content.size());
        std::string response =
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: text/html\r\n"
            "Content-Length: " + len + "\r\n"
            "\r\n" +
            content;
        send(client_fd, response.c_str(), response.size(), 0);
        close(client_fd);
    }

    close(server_fd);
}
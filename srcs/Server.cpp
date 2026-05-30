#include "../includes/Server.hpp"

Server::Server()
{
    server_fd = -1;
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
        server_fd = other.server_fd;
    }

    return *this;
}

Server::~Server()
{
    if (server_fd >= 0)
        close(server_fd);
}

void Server::init()
{
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
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(8080);

    if (bind(server_fd,
             (struct sockaddr*)&addr,
             sizeof(addr)) < 0)
    {
        perror("bind");
        exit(1);
    }

    if (listen(server_fd, 10) < 0)
    {
        perror("listen");
        exit(1);
    }

    std::cout << "Server started on port 8080\n";
}

void Server::acceptClient()
{
    int client_fd;

    client_fd = accept(server_fd, NULL, NULL);

    if (client_fd < 0)
        return;

    fcntl(client_fd, F_SETFL, O_NONBLOCK);

    clients.push_back(client_fd);

    std::cout << "New client connected\n";
}

void Server::handleClient(int fd)
{
    char buffer[1024];

    memset(buffer, 0, sizeof(buffer));

    int bytes = recv(fd,
                     buffer,
                     sizeof(buffer),
                     0);

    if (bytes <= 0)
    {
        close(fd);

        for (size_t i = 0; i < clients.size(); i++)
        {
            if (clients[i] == fd)
            {
                clients.erase(clients.begin() + i);
                break;
            }
        }

        return;
    }

    std::string request(buffer);

    Request req = parse_f(request);

    if (req.method != "GET")
    {
        std::string resp;

        resp = make_response(405,
                             "Method Not Allowed",
                             "text/plain");

        send(fd,
             resp.c_str(),
             resp.size(),
             0);

        close(fd);

        return;
    }
    if (req.path == "/")
        req.path = "/index.html";
    std::string full_path;
    full_path = "../web" + req.path;
    std::ifstream file(full_path.c_str());

    if (!file.is_open())
    {
        std::string resp;

        resp = make_response(404,
                             "Not Found",
                             "text/plain");

        send(fd,
             resp.c_str(),
             resp.size(),
             0);

        close(fd);

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

    close(fd);
}

void Server::run()
{
    while (true)
    {
        fd_set readfds;

        FD_ZERO(&readfds);

        FD_SET(server_fd, &readfds);

        int max_fd = server_fd;

        for (size_t i = 0; i < clients.size(); i++)
        {
            FD_SET(clients[i], &readfds);

            if (clients[i] > max_fd)
                max_fd = clients[i];
        }

        int activity;

        activity = select(max_fd + 1,
                          &readfds,
                          NULL,
                          NULL,
                          NULL);

        if (activity < 0)
        {
            perror("select");
            continue;
        }

        if (FD_ISSET(server_fd, &readfds))
            acceptClient();

        for (size_t i = 0; i < clients.size(); i++)
        {
            if (FD_ISSET(clients[i], &readfds))
                handleClient(clients[i]);
        }
    }
}
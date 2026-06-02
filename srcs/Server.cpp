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
    if (!config.load("conf/default.conf"))
        exit(1);
    server_config = config.getServers()[0];

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
    if (server_config.host == "0.0.0.0")
        addr.sin_addr.s_addr = INADDR_ANY;
    else
        addr.sin_addr.s_addr = inet_addr(server_config.host.c_str());
    addr.sin_port = htons(server_config.port);

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

    std::cout << "Server started on port " << server_config.port << "\n";
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
        clients.erase(
    std::remove(clients.begin(), clients.end(), fd),
    clients.end()
);

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
    const LocationConfig* location = config.findLocation(server_config, req.path);    /// idk if this is right
    if (location == NULL || !isMethodAllowed(*location, req.method))
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
        clients.erase(
    std::remove(clients.begin(), clients.end(), fd),
    clients.end()
);

        return;
    }
    if (req.path == "/")
        req.path = "/" + location->index;
    std::string full_path;
    full_path = location->root + req.path;
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
        clients.erase(
    std::remove(clients.begin(), clients.end(), fd),
    clients.end()
);

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
    clients.erase(
    std::remove(clients.begin(), clients.end(), fd),
    clients.end()
);
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

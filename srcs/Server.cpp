#include "../includes/Server.hpp"

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
        server_configs = other.server_configs;
        client_servers = other.client_servers;
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
        int server_fd;

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
        server_configs.push_back(servers[i]);

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
    client_servers[client_fd] = server_index;

    std::cout << "New client connected\n";
}

void Server::closeClient(int fd)
{
    close(fd);
    clients.erase(
        std::remove(clients.begin(), clients.end(), fd),
        clients.end()
    );
    client_servers.erase(fd);
}

void Server::handleClient(int fd)
{
    // TODO: Store per-client read/write buffers. A real HTTP request can arrive in
    // many recv() calls, and responses may need many send() calls.
    char buffer[1024];

    memset(buffer, 0, sizeof(buffer));

    int bytes = recv(fd,
                     buffer,
                     sizeof(buffer),
                     0);

    if (bytes <= 0)
    {
        closeClient(fd);

        return;
    }

    std::string request(buffer);

    Request req = parse_f(request);
    std::map<int, size_t>::iterator server_index = client_servers.find(fd);
    if (server_index == client_servers.end())
    {
        closeClient(fd);
        return;
    }

    const ServerConfig& current_server = server_configs[server_index->second];
    const LocationConfig* location = config.findLocation(current_server, req.path);
    if (location == NULL)
    {
        std::string resp;

        resp = make_response(404,
                             "Not Found",
                             "text/plain");

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
                             "text/plain");

        send(fd,
             resp.c_str(),
             resp.size(),
             0);

        closeClient(fd);

        return;
    }
    if (req.path == "/")
        req.path = "/" + location->index;
    // TODO: Dispatch by method and location settings here:
    // GET serves files/directories, POST handles upload or CGI body input,
    // DELETE removes allowed resources, redir sends 3xx, autoindex lists dirs.
    // Also enforce current_server.client_max_body_size before accepting bodies.
    std::string full_path;
    // TODO: For non-root locations, strip the location prefix before joining root.
    // Example: location /upload root web/uploads; /upload/a.txt -> web/uploads/a.txt.
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
    // TODO: Monitor writable sockets too. The subject requires one select/poll
    // equivalent for all socket I/O, with both reading and writing readiness.
    while (true)
    {
        fd_set readfds;

        FD_ZERO(&readfds);

        int max_fd = -1;

        for (size_t i = 0; i < server_fds.size(); i++)
        {
            FD_SET(server_fds[i], &readfds);

            if (server_fds[i] > max_fd)
                max_fd = server_fds[i];
        }

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
            else
                i++;
        }
    }
}

#include "../includes/Server.hpp"
#include <dirent.h>
#include <sys/stat.h>
#include <cstdio>
#include <cerrno>

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

static std::string basenameOf(const std::string& path)
{
    size_t slash = path.find_last_of('/');

    if (slash == std::string::npos || slash + 1 >= path.size())
        return "upload.bin";
    return path.substr(slash + 1);
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
        read_buffer = other.read_buffer;
        write_buffer = other.write_buffer;
        write_offset = other.write_offset;
        close_after_write = other.close_after_write;
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
    if (write_buffer.find(fd) != write_buffer.end())
    {
        close_after_write[fd] = true;
        return;
    }
    close(fd);
    clients.erase(
        std::remove(clients.begin(), clients.end(), fd),
        clients.end()
    );
    client_servers.erase(fd);
    read_buffer.erase(fd);
    write_offset.erase(fd);
    close_after_write.erase(fd);
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
        return;
    }

    offset += static_cast<size_t>(bytes);
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

void Server::handleClient(int fd)
{
    char buffer[1024];
    int bytes = recv(fd, buffer, sizeof(buffer), 0);
    if (bytes <= 0)
    {
        closeClient(fd);
        return;
    }
    read_buffer[fd].append(buffer, bytes);
    if (read_buffer[fd].find("\r\n\r\n") == std::string::npos)
        return;
    std::string raw_request = read_buffer[fd];
    Request req = parse_f(raw_request);
    read_buffer[fd].clear();
    std::map<int, size_t>::iterator server_index = client_servers.find(fd);
    if (server_index == client_servers.end())
    {
        closeClient(fd);
        return;
    }

    const ServerConfig& current_server = server_configs[server_index->second];
    if (!req.valid)
    {
        std::string resp;

        resp = make_response(400,
                             "Bad Request",
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

    if ((req.method == "GET" || req.method == "POST") &&
        location->cgi_extensions.find(extensionOf(full_path)) !=
        location->cgi_extensions.end())
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

        std::string upload_path = joinPath(location->upload_store,
                                          basenameOf(req.path));
        std::ofstream upload(upload_path.c_str(),
                             std::ios::out | std::ios::binary);

        if (!upload.is_open())
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
        upload.write(body.data(), body.size());
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

        FD_ZERO(&readfds);
        FD_ZERO(&writefds);

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
        activity = select(max_fd + 1,
                          &readfds,
                          &writefds,
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

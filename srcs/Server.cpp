#include "../includes/Server.hpp"
#include <dirent.h>
#include <sys/stat.h>
#include <cstdio>
#include <cerrno>
#include <cctype>
#include <limits>

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

static std::string basenameOf(const std::string& path)
{
    size_t slash = path.find_last_of('/');

    if (slash == std::string::npos || slash + 1 >= path.size())
        return "upload.bin";
    return path.substr(slash + 1);
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

static bool hasCompleteRequestBody(const std::string& request,
                                   bool& bad_request,
                                   size_t& content_length)
{
    size_t headers_end = request.find("\r\n\r\n");
    bool saw_content_length = false;
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
        pos = end + 2;
    }
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
    size_t content_length;

    if (bytes <= 0)
    {
        closeClient(fd);
        return;
    }
    read_buffer[fd].append(buffer, bytes);
    if (!hasCompleteRequestBody(read_buffer[fd],
                                bad_request,
                                content_length))
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
    if (content_length > current_server.client_max_body_size)
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
    if (bad_request)
        req.valid = false;
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

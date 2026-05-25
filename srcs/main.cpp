#include "../includes/Server.hpp"

int main()
{
    Config config;
    std::vector<int> clients;
    if (config.load("conf/default.conf") == false)
    {
        std::cerr << "Error: Could not load config file\n";
        return 1;
    }
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0)
    {
        perror("socket");
        return 1;
    }
    fcntl(server_fd, F_SETFL, O_NONBLOCK);//clientnery vor linen shat vochte 1 client 

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
        int activity = select(max_fd + 1,
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
        {
            int client_fd = accept(server_fd, NULL, NULL);

            if (client_fd >= 0)
            {
                fcntl(client_fd, F_SETFL, O_NONBLOCK);
                clients.push_back(client_fd);
            }
        }
        for (size_t i = 0; i < clients.size(); i++)
        {
            int client_fd = clients[i];

            if (FD_ISSET(client_fd, &readfds))
            {
                char buffer[1024];

                memset(buffer, 0, sizeof(buffer));

                int bytes = recv(client_fd, buffer, sizeof(buffer), 0);

                if (bytes <= 0)
                {
                    close(client_fd);
                    clients.erase(clients.begin() + i);
                    i--;
                    continue;
                }

                std::string request(buffer);

                Request req = parse_f(request);

                if (req.method != "GET")
                {
                    std::string resp =
                        make_response(405,
                                      "Method Not Allowed",
                                      "text/plain");

                    send(client_fd,
                         resp.c_str(),
                         resp.size(),
                         0);

                    close(client_fd);

                    clients.erase(clients.begin() + i);

                    i--;

                    continue;
                }

                if (req.path == "/")
                    req.path = "/index.html";

                std::string full_path =
                    "../web" + req.path;

                std::ifstream file(full_path.c_str());

                if (!file.is_open())
                {
                    std::string resp =
                        make_response(404,
                                      "Not Found",
                                      "text/plain");

                    send(client_fd,
                         resp.c_str(),
                         resp.size(),
                         0);

                    close(client_fd);

                    clients.erase(clients.begin() + i);

                    i--;

                    continue;
                }

                std::string content(
                    (std::istreambuf_iterator<char>(file)),
                    std::istreambuf_iterator<char>()
                );

                std::string type =
                    get_type(full_path);

                std::string response =
                    make_response(200,
                                  content,
                                  type);

                send(client_fd,
                     response.c_str(),
                     response.size(),
                     0);

                close(client_fd);

                clients.erase(clients.begin() + i);

                i--;
            }
        }
    }

    close(server_fd);
}
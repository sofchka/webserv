#include "../includes/Server.hpp"

void init()
{

}
void run()
{

}
void acceptClient()
{

}
void handleClient(int fd)
{

}

Server::Server(){}
Server &Server::operator=(const Server& other)
{
    if (this != &other)
    {
        this->clients = other.clients;
        this->Config = other.Config;
        this->server_fd = other.server_fd;
    }
    return *this;
}
Server::Server(const Server &other)
{
    this->clients = other.clients;
    this->Config = other.Config;
    this->server_fd = other.server_fd;
}
Server::~Server(){}
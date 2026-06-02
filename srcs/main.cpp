#include "../includes/Server.hpp"

int main(int argc, char **argv)
{
    std::string config_path;

    if (argc > 2)
    {
        std::cerr << "Usage: ./webserv [configuration file(no mandatory)]" << std::endl;
        return 1;
    }
    if (argc == 2)
        config_path = argv[1];
    else
        config_path = "conf/default.conf";

    Server Server;
    Server.init(config_path);
    Server.run();
}

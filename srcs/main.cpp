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
}
#include "../includes/Server.hpp"

void parse_f(std::string request)
{
    size_t line_end = request.find("\r\n");
    std::string f_line = request.substr(0, line_end);
    size_t pos1 = f_line.find(" ");
    size_t pos2 = f_line.find(" ", pos1 + 1);
    std::string method = f_line.substr(0, pos1);
    std::string path = f_line.substr(pos1 + 1, pos2 - pos1 - 1);
    std::string version = f_line.substr(pos2 + 1);
    std::cout<<"Method: "<<method<<std::endl;
    std::cout<<"Path: "<<path<<std::endl;
    std::cout<<"Version: "<<version<<std::endl; 
}
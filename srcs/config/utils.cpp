#include "../../includes/Config.hpp"

std::string trimConfigLine(const std::string& line)
{
    std::string result;
    size_t start;
    size_t end;
    // comments removal
    for (size_t i = 0; i < line.length(); i++)
    {
        if (line[i] == '#')
            break;
        result += line[i];
    }
    // space start
    start = 0;
    while (start < result.length()
        && (result[start] == ' ' || result[start] == '\t'
            || result[start] == '\r'))
    {
        start++;
    }

    // if only spaces
    if (start == result.length())
        return "";

    // last spces
    end = result.length() - 1;
    while (end > start
        && (result[end] == ' ' || result[end] == '\t'
            || result[end] == '\r' || result[end] == '\n'))
    {
        end--;
    }

    return result.substr(start, end - start + 1);
}

std::vector<std::string> splitCleanTokens(const std::string& line)
{
    std::vector<std::string> tokens;
    std::string current;

    for (size_t i = 0; i < line.size(); i++)
    {
        if (line[i] == ' ' || line[i] == '\t' || line[i] == ';' ||
            line[i] == '{' || line[i] == '}')
        {
            if (!current.empty())
            {
                tokens.push_back(current); // adding to vector
                current.clear(); // set to ""
            }
        }
        else
            current += line[i];
    }

    if (!current.empty())
        tokens.push_back(current);

    return tokens;
}

bool isMethodAllowed(const LocationConfig& location, const std::string& method)
{
    (void)location;
    (void)method;
    return false;
}






bool hasConfExtention(const std::string& filename)
{
    int len = filename.length();
    if (len < 5)
        return false;
    if (filename.substr(len - 5) == ".conf")
        return true;
    return false;
}

bool isServerBlockStart(const std::string& line)
{
    return line == "server {" || line == "server{" || line == "server";
}

bool isLocationBlockStart(const std::string& line)
{
    return line.size() >= 9 && line.substr(0, 9) == "location ";
}

bool lineHasClosedBracket(const std::string& line)
{
    for (size_t i = 0; i < line.length(); ++i)
    {
        if (line[i] == '}')
            return true;
    }
    return false;
}
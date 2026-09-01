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

std::vector<std::string> splitCleanTokens(const std::string& line, int *k)
{
    std::vector<std::string> tokens;
    std::string current;

    for (size_t i = 0; i < line.size(); i++)
    {
        if (line[i] == '}')
        {
            *k = 1;
            continue;
        }
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

bool hasConfExtention(const std::string& filename)
{
    const std::string extension = ".conf";

    if (filename.length() < extension.length())
        return false;
    if (filename.compare(filename.length() - extension.length(),
                         extension.length(),
                         extension) == 0)
        return true;
    return false;
}

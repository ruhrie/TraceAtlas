#pragma once
#include <exception>
#include <sstream>
#include <string>

class AtlasException : public std::exception
{
protected:
    std::string msg;

public:
    AtlasException(const std::string &arg, const char *file, int line)
    {
        std::ostringstream o;
        o << file << ":" << line << ": " << arg;
        msg = o.str();
    }
    using std::exception::what;
    const char *what()
    {
        return msg.c_str();
    }
};

#define AtlasException(arg) AtlasException(arg, __FILE__, __LINE__);
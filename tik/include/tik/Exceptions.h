#pragma once
#include <exception>
#include <sstream>
#include <string>

class TikException : public std::exception
{
protected:
    std::string msg;

public:
    TikException(const std::string &arg, const char *file, int line)
    {
        std::ostringstream o;
        o << file << ":" << line << ": " << arg;
        msg = o.str();
    }
    const char *what()
    {
        return msg.c_str();
    }
};

#define TikException(arg) TikException(arg, __FILE__, __LINE__);
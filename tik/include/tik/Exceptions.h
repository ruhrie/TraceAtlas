#pragma once
#include <exception>
#include <string>
class NotImplementedException : public std::exception
{
private:
    std::string Message = "";

public:
    NotImplementedException();
    NotImplementedException(std::string message)
    {
        Message = message;
    }
    const char *what()
    {
        std::string res = "This code is not implemented";
        if(Message != "")
        {
            res += ": " + Message;
        }
        return res.c_str();
    }
};

class KernelException : public std::exception
{
private:
    std::string Message = "";

public:
    KernelException();
    KernelException(std::string message)
    {
        Message = message;
    }
    const char *what()
    {
        std::string res = "This code is not implemented";
        if(Message != "")
        {
            res += ": " + Message;
        }
        return res.c_str();
    }
};
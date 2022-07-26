#pragma once

#include <exception>
#include <string>


namespace nginxpp {

class Exception : public std::exception {
public:
    explicit Exception(std::string message) noexcept : m_message(std::move(message)) {
    }

    [[nodiscard]] const char *what() const noexcept override {
        return m_message.c_str();
    }

private:
    std::string m_message;
};


class SocketException : public Exception {
    using Exception::Exception;
};


class ParserException : public Exception {
public:
    using Exception::Exception;
    ParserException() : Exception("Bad Request") {
    }
};

class ServerException : public Exception {
    using Exception::Exception;
};

class HttpVersionException : public ServerException {
    using ServerException::ServerException;
};


} //namespace nginxpp

#ifndef SOCKET_HPP
#define SOCKET_HPP

#include <cerrno>
#include <cstddef>
#include <cstring>
// #include <iostream>
#include <iostream>
#include <netdb.h>
#include <string>
#include <sys/socket.h>
#include <unistd.h>

struct Client {
    int fd;
    struct sockaddr_in client_addr;
    int client_addr_len = sizeof(sockaddr_in);

    void close() {
        //? might error?
        ::close(fd);
    }
};

class Socket {
public:
    Socket(std::string port) : port(port) {};
    Socket(std::string port, int backlog) : port(port), connection_backlog(backlog) {};
    ~Socket() {
        close();
    }

    class Socket_Error {
        public:
            std::string msg;

            Socket_Error(std::string location,std::string error) {
                msg = location + ": " + error;
            }
    };
    
    void init();
    void accept(struct Client& client) const;
    int send(const struct Client& client, const std::string& msg) const;
    int send(const int& fd, const std::string& msg) const;
    int recv(const struct Client& client, std::string& buf) const;
    int recv(const int& fd, std::string& buf) const;
    void close() const;
    int get_server_fd() const;

private:
    int server_fd;
    int reuse = 1;
    int connection_backlog = 5;
    std::string port;
};

 void Socket::init() {
    int rv;
    struct addrinfo hints, *ai, *p;

    memset(&hints, 0, sizeof hints);
    // hints.ai_family = AF_INET;
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    if ((rv = ::getaddrinfo(nullptr, port.c_str(), &hints, &ai)) != 0) {
        std::string err_msg = ::gai_strerror(rv);
        throw Socket_Error("getaddrinfo(): ", err_msg);
    }

    std::string location;
    int err_num;
    for (p = ai; p != nullptr; p = p->ai_next) {
        server_fd = ::socket(p->ai_family, p->ai_socktype,p->ai_protocol);
        if (server_fd < 0) {
            location = "socket()";
            err_num = errno;
            continue;
        }

        if (::setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0) {
            throw Socket_Error("setsockopt()",std::strerror(errno));
        }

        if (::bind(server_fd, p->ai_addr, p->ai_addrlen) < 0) {
            location = "bind()";
            err_num = errno;
            ::close(server_fd);
            continue;
        }

        break;
    }

    // If we got here, it means we didn't get bound
    if (p == nullptr) {
        //?: error might be wrong
        throw Socket_Error(location, std::strerror(err_num));
    }

    ::freeaddrinfo(ai);

    if (::listen(server_fd, connection_backlog) != 0) {
        throw Socket_Error("listen()",std::strerror(errno));
    }

}

void Socket::accept(struct Client& client) const {
        client.fd = ::accept(
        server_fd,
        (struct sockaddr *) &client.client_addr, 
    (socklen_t *) &client.client_addr_len);

    if (client.fd < 0) { throw Socket_Error("accept()",std::strerror(errno));}
}

int Socket::send(const struct Client& client, const std::string& msg) const {
    // TODO: Handle partial sends
    int bytes_sent = ::send(client.fd , msg.c_str(), msg.length() , 0);

    if (bytes_sent < 0) { throw Socket_Error("send()",std::strerror(errno)); }

    return bytes_sent;
}

int Socket::send(const int& fd, const std::string& msg) const {
    // TODO: Handle partial sends
    int bytes_sent = ::send(fd , msg.c_str(), msg.length() , 0);

    if (bytes_sent < 0) { throw Socket_Error("send()",std::strerror(errno)); }

    return bytes_sent;
}

int Socket::recv(const struct Client& client, std::string& buf) const {
    // TODO: Handle partial recvs
    size_t size = buf.capacity();
    // std::cout << "size: " << size << std::endl;
    char* temp[size];
    // memset(temp, 0, size);
    int bytes_recieved = ::recv(client.fd, temp, size, 0);
    // std::cout << "messege recieved: " << (char *)temp << std::endl;
    buf = (char *)temp;
    // std::cout << "messege recieved: " << buf << std::endl;
    return bytes_recieved;
}

int Socket::recv(const int& fd, std::string& buf) const {
    // TODO: Handle partial recvs
    size_t size = buf.capacity();
    // std::cout << "size: " << size << std::endl;
    char* temp[size];
    // memset(temp, 0, size);
    int bytes_recieved = ::recv(fd, temp, size, 0);
    // std::cout << "messege recieved: " << (char *)temp << std::endl;
    buf = (char *)temp;
    // std::cout << "messege recieved: " << buf << std::endl;
    return bytes_recieved;
}

void Socket::close() const {
    //? might error
    int sc_err = ::close(server_fd);
    std::cout << sc_err << "\n";
}

int Socket::get_server_fd() const {
    return server_fd;
}

#endif
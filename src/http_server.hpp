#ifndef HTTP_SERVER_HPP
#define HTTP_SERVER_HPP

#include <algorithm>
#include <cstdio>
#include <iostream>
#include <cstdlib>
// #include <iterator>
#include <sstream>
#include <string>
#include <cstring>
#include <unordered_map>

#include <sys/epoll.h>
#include <unistd.h>
#include <zlib.h>

#include "socket.hpp"
#include "http_req.hpp"
#include "file.hpp"

enum CONTYPE {
    NONE,
    PLAIN,
    APP_OCT
};

class Http_Server {
private:
    Socket *socket;
    const std::string port;
    std::string directory, buf;
    std::unordered_map<int, struct Client> client_map;

    static const int MAX_EVENTS = 10;
    int event_count;
    struct epoll_event event, events[MAX_EVENTS];

    int poll_fd, server_fd;

    void init_epoll();
    void add_client();
    void handle_disconnect(int client_fd);
    bool check_path(const std::string& uri, const std::string& path, std::string& rest);
    void gzip(std::string data, std::string& out);
    std::string make_msg(std::string& body, CONTYPE type, std::string compress = "", std::string close = "");
    void send_200(int client_fd, std::string close = "");
    void send_201(int client_fd ,std::string close = "");
    void send_404(int client_fd, std::string close = "");
    void handle_post(Http_Req req, int client_fd);
    void handle_get(Http_Req req, int client_fd);   
    
public: 
    Http_Server(const std::string port, const std::string dir);

    ~Http_Server() {
        close(poll_fd);
        delete socket;
    }

    void serve();
};

Http_Server::Http_Server(const std::string port, const std::string dir): port(port) {
    if (dir.back() != '/') {
        directory =  dir + "/";
    } else {
        directory = dir;
    }

    try { 
        socket = new Socket(port); 
        socket->init();
    }
    catch (Socket::Socket_Error) { throw; }
    server_fd = socket->get_server_fd();

    // buf = new std::string(1024, '\0');
    buf.resize(1024);
    buf.clear();

    init_epoll();
}

void Http_Server::init_epoll() {
    poll_fd = epoll_create1(0);
    if (poll_fd == -1) {
        std::cout << "Failed to create an epoll file descriptor\n";
        throw 1;
    }

    event.events = EPOLLIN;
    event.data.fd = server_fd;

    if (epoll_ctl(poll_fd, EPOLL_CTL_ADD, server_fd, &event)) {
        std::cout << "Failed to add file descriptor to epoll\n";
        close(poll_fd);
        throw 1;
    }
}

void Http_Server::add_client() {
    struct Client client;
    socket->accept(client);
    std::cout << "Client connected\n";
    client_map[client.fd] = client;

    event.events = EPOLLIN;
    event.data.fd = client.fd;

    if (epoll_ctl(poll_fd, EPOLL_CTL_ADD, client.fd, &event)) {
        std::cout << "Failed to add file descriptor to epoll\n";
        close(poll_fd);
        throw 1;
    }
}

void Http_Server::handle_disconnect(int client_fd) {
    client_map.erase(client_fd);
    std::cout << "Client disconnected from socket " << client_fd << "\n";

    if (epoll_ctl(poll_fd, EPOLL_CTL_DEL, client_fd, nullptr)) {
        std::cout << "Failed to add file descriptor to epoll\n";
        close(poll_fd);
        throw 1;
    }

    close(client_fd);
}

bool Http_Server::check_path(const std::string& uri, const std::string& path, std::string& rest) {
    int path_size = path.length();
    if (uri.substr(0,path_size) == path) {
        if (path_size != uri.length()) {
        rest = uri.substr(path_size + 1);
        }
        return true;
    }

    return false;
}

void Http_Server::gzip(std::string data, std::string& out) {
    /*
    One thing to notice: if you're handling GZIP (instead of deflate),
    which you may find when making a HTTP request,
    you need to replace inflateInit(&strm) with inflateInit2(&strm, 16 + MAX_WBITS),
    since GZIP has a bigger header.
    */
    z_stream zs;
    zs.zalloc = Z_NULL;
    zs.zfree = Z_NULL;
    zs.opaque = Z_NULL;
    zs.avail_in = (uInt)data.size();
    zs.next_in = (Bytef *)data.c_str();
    zs.avail_out = (uInt)out.size();
    zs.next_out = (Bytef *)out.data();

    // Add 16 to windowBits to write a simple gzip header and trailer around the compressed data instead of a zlib wrapper
    deflateInit2(&zs, Z_DEFAULT_COMPRESSION, Z_DEFLATED, 15 | 16, 8, Z_DEFAULT_STRATEGY);
    deflate(&zs, Z_FINISH);
    deflateEnd(&zs);
    // return zs.total_out;
}

std::string Http_Server::make_msg(std::string& body, CONTYPE type, std::string compress, std::string close) {
    std::string head;

    // if (method == "GET") {
        head = "HTTP/1.1 200 OK\r\n";
    // } 
    // else if (method == "POST") {

    //     head = "HTTP/1.1 201 Created\r\n";
    // }

    switch (type) {
        case NONE:
            break;
        case PLAIN:
            head += "Content-Type: text/plain\r\n";
            break;
        case APP_OCT:
            head += "Content-Type: application/octet-stream\r\n";
            break;

    }

    if (!compress.empty()) {
        head += "Content-Encoding: " + compress + "\r\n";
    }
    if (!close.empty()) {
        head += "Connection: " + close + "\r\n";
    }

    head += "Content-Length: " + std::to_string(body.size()) + "\r\n";
    head += "\r\n";

    return head + body;
}

void Http_Server::send_200(int client_fd, std::string close) {
    std::string msg = "HTTP/1.1 200 OK\r\n";

    if (!close.empty()) {
        msg += "Connection: " + close + "\r\n";
    }

    msg += "Content-Length: 0\r\n\r\n";
    int bytes_sent = socket->send(client_fd, msg);
    std::cout << "bytes sent: " << bytes_sent << "\n";
}

void Http_Server::send_201(int client_fd ,std::string close) {
    std::string msg = "HTTP/1.1 201 Created\r\n";

    if (!close.empty()) {
        msg += "Connection: " + close + "\r\n";
    }

    msg += "Content-Length: 0\r\n\r\n";
    int bytes_sent = socket->send(client_fd, msg);
    std::cout << "bytes sent: " << bytes_sent << "\n";
}

void Http_Server::send_404(int client_fd, std::string close) {
    std::string msg = "HTTP/1.1 404 Not Found\r\n";

    if (!close.empty()) {
        msg += "Connection: " + close + "\r\n";
    }

    msg += "Content-Length: 0\r\n\r\n";
    int bytes_sent = socket->send(client_fd, msg);
    std::cout << "bytes sent: " << bytes_sent << "\n";
}

void Http_Server::handle_post(Http_Req req, int client_fd) {
    std::string rest;
    if (check_path(req.uri, "/files", rest)){
        // Extract file name from rest
        std::stringstream check_rest(rest);
        std::string data, file_name, ext;
        std::string close = "";

        if (auto close_val = req.headers.find("connection"); close_val != req.headers.end()) {
        
            if (close_val->second.front() == "close") {
                close = "close";
            }
            
        }

        std::getline(check_rest, file_name, '/');

        try {
            File file(directory + file_name, File::WRITE);
            file.write(req.body);
        } catch(File::File_Error e) {
            std::cerr << e.what();
            send_404(client_fd);
            return;
        }

        send_201(client_fd, close);
        if (!close.empty()) handle_disconnect(client_fd);
    }
}

void Http_Server::handle_get(Http_Req req, int client_fd) {
    std::string rest;

    if (req.uri == "/") {
        std::string close = "";

        if (auto close_val = req.headers.find("connection"); close_val != req.headers.end()) {
        
            if (close_val->second.front() == "close") {
                close = "close";
            }
            
        }

        send_200(client_fd, close);
        if (!close.empty()) handle_disconnect(client_fd);
    } else if (check_path(req.uri, "/echo", rest)) {
        std::string compress = "";
        std::string close = "";

        if (auto close_val = req.headers.find("connection"); close_val != req.headers.end()) {
            
                if (close_val->second.front() == "close") {
                    close = "close";
                }
            
        }

        if (auto enc_vals = req.headers.find("accept-encoding"); enc_vals != req.headers.end()) {
            for (auto val: enc_vals->second) {
                // std::cout << val << '\n';
                if (val == "gzip") {
                    compress = "gzip";
                }
            }
        }

        int size = rest.size() + 20;
        // int size = 100;
        std::string body(size,'\0');
        
        if (compress.empty()) {
            body = rest;
        } else {
            std::cout << body.size() << '\n';
            gzip(rest, body);
            std::cout << body.size() << '\n';
        }


        int bytes_sent = socket->send(client_fd, make_msg(body, CONTYPE::PLAIN, compress, close));
        std::cout << "bytes sent: " << bytes_sent << "\n";
        if (!close.empty()) handle_disconnect(client_fd);
    } else if (check_path(req.uri, "/user-agent", rest)) {
        // std::string body = req.headers["User-Agent"];
        std::string data = req.headers["user-agent"].front();
        // auto body = req.headers["User-Agent"].size();
        std::string compress = "";
        std::string close = "";

        if (auto close_val = req.headers.find("connection"); close_val != req.headers.end()) {
            
                if (close_val->second.front() == "close") {
                    close = "close";
                }
            
        }

        if (auto enc_vals = req.headers.find("accept-encoding"); enc_vals != req.headers.end()) {
            for (auto val: enc_vals->second) {
                std::cout << val << '\n';
                if (val == "gzip") {
                    compress = "gzip";
                }
            }
        }

        int size = data.size() + 20;
        // int size = 100;
        // std::string body(size,'\0');
        std::string body;
        body.resize(size);

        
        if (compress.empty()) {
            body = data;
        } else {
            std::cout << body.size() << '\n';
            gzip(data, body);
            std::cout << body.size() << '\n';
        }
        
        int bytes_sent = socket->send(client_fd, make_msg(body, CONTYPE::PLAIN, compress, close));
        std::cout << "bytes sent: " << bytes_sent << "\n";
        if (!close.empty()) handle_disconnect(client_fd);
    } else if (check_path(req.uri, "/files", rest)){
        // Extract file name from rest
        std::stringstream check_rest(rest);
        std::string data, file_name;
        std::string compress = "";
        std::string close = "";

        if (auto close_val = req.headers.find("connection"); close_val != req.headers.end()) {
            
                if (close_val->second.front() == "close") {
                    close = "close";
                }
            
        }

        if (auto enc_vals = req.headers.find("accept-encoding"); enc_vals != req.headers.end()) {
            for (auto val: enc_vals->second) {
                std::cout << val << '\n';
                if (val == "gzip") {
                    compress = "gzip";
                }
            }
        }

        std::getline(check_rest, file_name, '/');

        try {
            File file(directory + file_name, File::READ);
            data = file.read();
        } catch(File::File_Error e) {
            std::cerr << e.what();
            send_404(client_fd);
            return;
        }

        int size = data.size() + 20;
        // int size = 100;
        std::string body(size,'\0');
        
        if (compress.empty()) {
            body = data;
        } else {
            gzip(data, body);
        }

        int bytes_sent = socket->send(client_fd, make_msg(body, CONTYPE::APP_OCT, compress, close));
        std::cout << "bytes sent: " << bytes_sent << "\n";
        if (!close.empty()) handle_disconnect(client_fd);
    } else {
        std::string close = "";

        if (auto close_val = req.headers.find("connection"); close_val != req.headers.end()) {
            
                if (close_val->second.front() == "close") {
                    close = "close";
                }
            
        }

        send_404(client_fd, close);
        if (!close.empty()) handle_disconnect(client_fd);
    }
}

void Http_Server::serve() {
    while (true) {
        std::cout << "Waiting for a client to connect...\n";
        event_count = epoll_wait(poll_fd, events, MAX_EVENTS, -1);

        for (int i = 0; i < event_count; i++) {
            if (events[i].data.fd == server_fd) {
                add_client();
            } else {

                const int& client_fd = events[i].data.fd;
                int bytes_recieved = socket->recv(client_fd, buf);

                if (bytes_recieved == 0) {
                    handle_disconnect(client_fd);
                    continue;
                }
                
                Http_Req http_req;

                try { http_req.parse(buf); } 
                catch(Http_Req::Http_Error e) {
                    std::cerr << e.what() << std::endl;
                } 

                std::string rest;

                if (http_req.method == "POST") {
                    handle_post(http_req, client_fd);
                } else if (http_req.method == "GET") {
                    handle_get(http_req, client_fd);
                }

                buf.clear();
            }
        }
    }
}

#endif
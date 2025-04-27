#ifndef HTTP_REQ_H
#define HTTP_REQ_H

#include <algorithm>
#include <cctype>
#include <sstream>
#include <iostream>
#include <string>
#include <unordered_map>
#include <vector>

class Http_Req {
    public:
        std::string method, uri, version, body;
        std::unordered_map<std::string,std::vector<std::string>> headers;


        class Http_Error {
            public:
                std::string value;

                Http_Error(std::string error): value(error) {}
        };

        void parse(const std::string& req) {
            std::stringstream check_all(req);
            std::string line, token;

            std::getline(check_all, line, '\n');
            line.pop_back();

            std::stringstream check_line(line);

            try {

                std::getline(check_line, token, ' ');
                method = token;

                validate_method(method);

                std::getline(check_line, token, ' ');
                uri = token;

                validate_uri(uri);

                std::getline(check_line, token, ' ');
                version = token;

                validate_version(version);

            } catch (Http_Error) {
                throw;
            }


            while (std::getline(check_all, line, '\n')) {
                line.pop_back();
                if (line.empty()) {
                    if (check_all.peek() > 0) {
                         // parse body
                        std::string temp;
                        while (std::getline(check_all, temp, '\n')) {
                            body += temp + "\n";
                        }
                        body.pop_back();
                    } 

                } else {
                    std::stringstream check_line(line);
                    std::string key, value;
                    std::vector<std::string> vals;

                    std::getline(check_line, key, ' ');
                    key.pop_back();
 
                    std::transform(key.begin(), key.end(), key.begin(),
                        [](unsigned char c) { return std::tolower(c);});
                    std::getline(check_line, value, ',');  
                    

                    do {
                        value.erase(std::remove_if(value.begin(), value.end(), isspace), value.end());
                        vals.push_back(value);
                    }
                    while (std::getline(check_line, value, ','));
                    headers[key] = vals;
                }
            }
        }

        void show(void) {
            std::cout << "Method: " << method << " \n";
            std::cout << "URI: " << uri << " \n";
            std::cout << "HTTP Version: " << version << " \n";

            for (auto [k, v]: headers) {
                std::cout << k << ": ";
                for (auto val: v) {
                    std::cout << val << ", ";
                }
                std::cout << " \n";
            }
        }

    private:
        void validate_method(std::string method) {
            if (method != "GET" && method != "POST" && method != "DELETE") {
                throw Http_Error("bad method");
            }
        }

        void validate_uri(std::string uri) {
            if (uri[0] != '/') {
                 throw Http_Error("bad uri");
            }
        }

        void validate_version(std::string version) {
            if (version.substr(0,4) != "HTTP") {
                throw Http_Error("bad http version");
            }
            
            std::string ver_num = version.substr(5);

            if (ver_num != "1.0" && ver_num != "1.1") {
                throw Http_Error("unsupported version");
            }
            
        }
};

#endif
#include <cstdio>
#include <iostream>
#include <cstdlib>
// #include <sstream>
#include <string>
#include <cstring>

#include <sys/epoll.h>
#include <unistd.h>
// #include <unordered_map>

#include "http_server.hpp"

int main(int argc, char **argv) {
  // Flush after every std::cout / std::cerr
  std::cout << std::unitbuf;
  std::cerr << std::unitbuf;


  if (argc != 1 && argc != 3) {
    std::cout << "Run with no arguments or with: --directory <path>";
    return 1;
  }

  if (argc == 3 && argv[1] != std::string("--directory")) {
    std::cout << "Wrong first argument, usage: --directory <path>";
    return 1;
  }
  
  std::string port = "4221";
  std::string directory = argv[2];

  Http_Server server(port, directory);
  server.serve();

  return 0;
}

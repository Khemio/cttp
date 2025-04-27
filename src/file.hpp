#ifndef FILE_HPP
#define FILE_HPP

#include <cctype>
#include <fstream>
#include <sstream>
#include <iostream>
#include <string>
#include <algorithm>

class File {
    private:
        std::fstream file;

        auto enum_to_val() {
            switch (mode) {
                case WRITE:
                    return std::ios::out;
                case READ:
                    return std::ios::in;
                case READ_WRITE:
                    return std::ios::out | std::ios::in;
            }

            return std::ios::in;
        }
    public:
        enum MODE {
            WRITE,
            READ,
            READ_WRITE
        };

        class File_Error {
            const std::string value;
            public:
                File_Error(std::string value): value(value) {}

                std::string what() {
                    return value;
                }
        };

        const std::string path;
        const MODE mode;

        File(std::string path, MODE mode): path(path), mode(mode) {
            file.open(path, enum_to_val());

            if (file.fail()) {
                // TODO: add exception
                // std::cout << "Failed to open file\n";
                // throw -1;
                throw File_Error("Failed to open file\n");
            }
        };

        ~File() {
            file.close();
        }

        std::string read() {
            if (mode != READ && mode != READ_WRITE) {
                // std::cout << "Incorrect file mode\n";
                // throw -2;
                throw File_Error("Incorrect file mode\n");
            }
            std::string data, temp;

            // data_stream << file.rdbuf();
            // data = data_stream.str();

            while (std::getline(file, temp, '\n')) {
                data += temp + '\n';
            }

            data.pop_back();
            // std::cout << data.size() << '\n';
            return data;
        }

        void write(std::string data) {
            if (mode != WRITE && mode != READ_WRITE) {
                // std::cout << "Incorrect file mode\n";
                // throw -2;
                throw File_Error("Incorrect file mode\n");
            }
            // std::cout << data.size() << '\n';
            // data.erase(std::remove_if(data.begin(), data.end(), isprint), data.end());
            data.erase(std::remove_if(data.begin(), data.end(),
                 [](auto c){return !isprint(c);}), data.end());
            // data.push_back('\0');
            file << data;
        }
};

#endif

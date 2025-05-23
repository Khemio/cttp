#ifndef FILE_HPP
#define FILE_HPP

#include <cctype>
#include <fstream>
#include <iostream>
#include <string>
#include <algorithm>

class File {
private:
    std::fstream file;
    auto enum_to_val();

public:
    enum MODE {
        WRITE,
        READ,
        READ_WRITE
    };

    class File_Error; 
    const std::string path;
    const MODE mode;

    File(std::string path, MODE mode);
    ~File() { file.close(); }

    std::string read();
    void write(std::string data);
};

auto File::enum_to_val() {
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

class File::File_Error {
    const std::string value;

    public:
    File_Error(std::string value): value(value) {}

    std::string what() { return value; }
};

File::File(std::string path, MODE mode): path(path), mode(mode) {
    file.open(path, enum_to_val());

    if (file.fail()) { throw File_Error("Failed to open file\n"); }
}

std::string File::read() {
    if (mode != READ && mode != READ_WRITE) { throw File_Error("Incorrect file mode\n"); }

    std::string data, temp;

    while (std::getline(file, temp, '\n')) { data += temp + '\n'; }

    data.pop_back();
    return data;
}

void File::write(std::string data) {
    if (mode != WRITE && mode != READ_WRITE) { throw File_Error("Incorrect file mode\n"); }

    data.erase(std::remove_if(data.begin(), data.end(),
            [](auto c){return !isspace(c) && !isprint(c); }), data.end());
    file << data;
}

#endif

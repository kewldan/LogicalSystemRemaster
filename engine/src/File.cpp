#include "File.h"

char *Engine::File::readFile(const char *path) {
    std::ifstream stream(path, std::ios::out | std::ios::binary);
    if (!stream) {
        PLOGW << "The requested file [" << path << "] does not exist";
        return nullptr;
    }
    int size = (int) std::filesystem::file_size(path);
    if(!size){
        PLOGW << "The requested file [" << path << "] does not exist";
        return nullptr;
    }
    char *bin = new char[size];
    stream.read(bin, size);
    stream.close();
    return bin;
}

bool Engine::File::writeFile(const char *path, const char *data, unsigned int size) {
    std::ofstream stream(path, std::ios::out | std::ios::binary);
    stream.write(data, size);
    stream.close();
    return stream.good();
}

bool Engine::File::exists(const char *path) {
    return std::filesystem::exists(path);
}

char *Engine::File::readString(const char *path) {
    std::ifstream stream(path, std::ios::out | std::ios::binary);
    if (!stream) {
        PLOGW << "The requested string [" << path << "] does not exist";
        return nullptr;
    }
    uintmax_t size = std::filesystem::file_size(path);
    char *bin = new char[size + 1ULL];
    stream.read(bin, size);
    stream.close();
    bin[size] = 0;
    return bin;
}

bool Engine::File::writeString(const char *path, const char *data) {
    return File::writeFile(path, data, strlen(data));
}

char *Engine::File::readResourceFile(const char *path, int *size) {
    auto myResource = ::FindResource(nullptr, path, RT_RCDATA);
    if(!myResource) {
        PLOGW << "The requested resource file [" << path << "] does not exist";
        return nullptr;
    }
    auto myResourceData = ::LoadResource(nullptr, myResource);
    if(size != nullptr)
        *size = (int) ::SizeofResource(nullptr, myResource);
    return static_cast<char *>(::LockResource(myResourceData));
}

char *Engine::File::readResourceString(const char *path) {
    auto myResource = ::FindResource(nullptr, path, RT_RCDATA);
    if(!myResource) {
        PLOGW << "The requested resource string [" << path << "] does not exist";
        return nullptr;
    }
    auto myResourceData = ::LoadResource(nullptr, myResource);
    auto pMyBinaryData = ::LockResource(myResourceData);
    DWORD size = ::SizeofResource(nullptr, myResource);
    char* str = new char[size + 1];
    str[size] = 0;
    memcpy(str, pMyBinaryData, size);
    return str;
}

bool Engine::File::resourceExists(const char *path) {
    return ::FindResource(nullptr, path, RT_RCDATA);
}

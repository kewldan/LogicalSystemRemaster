#include "File.h"

char* Engine::File::readFile(const char* path)
{
	std::ifstream stream(path, std::ios::out | std::ios::binary);
	if (!stream)
		return 0;
	int size = (int)std::filesystem::file_size(path);
	char* bin = new char[size];
	stream.read(bin, size);
	stream.close();
	return bin;
}

bool Engine::File::writeFile(const char* path, const char* data, unsigned int size)
{
	std::ofstream stream(path, std::ios::out | std::ios::binary);
	stream.write(data, size);
	stream.close();
	return stream.good();
}

bool Engine::File::exists(const char* path) {
	return std::filesystem::exists(path);
}

char* Engine::File::readString(const char* path) {
	std::ifstream stream(path, std::ios::out | std::ios::binary);
	if (!stream)
		return 0;
	uintmax_t size = std::filesystem::file_size(path);
	char* bin = new char[size + 1ULL];
	stream.read(bin, size);
	stream.close();
	bin[size] = 0;
	return bin;
}

bool Engine::File::writeString(const char* path, const char* data) {
	return File::writeFile(path, data, strlen(data));
}

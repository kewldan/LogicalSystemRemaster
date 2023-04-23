#pragma once

#include <fstream>
#include <iostream>
#include <filesystem>
#include "plog/Log.h"

namespace Engine {
	class File {
	public:
		static char* readFile(const char* path);
		static bool writeFile(const char* path, const char* data, unsigned int size);
		static char* readString(const char* path);
		static bool writeString(const char* path, const char* data);
		static bool exists(const char* path);
	};
}
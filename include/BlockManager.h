#pragma once

#include "Block.h"
#include <unordered_map>
#include <thread>
#include <omp.h>
#include <fstream>
#include <iostream>
#include <filesystem>
#include "engine/File.h"

const float quadVertices[] = {
	0.0f, 1.0f, 0.f, 0.0f, 1.0f,
	0.0f, 0.0f, 0.f, 0.0f, 0.0f,
	1.0f, 1.0f, 0.f, 1.0f, 1.0f,
	1.0f, 0.0f, 0.f, 1.0f, 0.0f,
};

typedef std::unordered_map<long long, Block*> Blocks;

class BlockManager {
public:
	unsigned int atlas, VAO, * VBO;
	glm::mat4* mvp;
	glm::vec2* info;
	Blocks blocks;
	BlockType* types;

	BlockManager();
	void set(int x, int y, Block* block);
	Block* get(int x, int y);
	bool has(int x, int y);
	void erase(int x, int y);
	int length();
	void update();
	void setActive(int x, int y);
	void setActive(int x, int y, BlockRotation rotation);
	void setActive(int x, int y, BlockRotation rotation, int l);
	void uploadBuffers(int count);
	bool save(const char* path);
	bool load(const char* path);
};

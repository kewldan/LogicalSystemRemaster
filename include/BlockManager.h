#pragma once

#include "Block.h"
#include <unordered_map>
#include <thread>
#include <omp.h>
#include <fstream>
#include <iostream>
#include <filesystem>
#include <mutex>
#include "File.h"
#include "Window.h"

typedef std::unordered_map<long long, Block*> Blocks;

class BlockManager {
private:
	void thread_tick();
public:
	unsigned int atlas, VAO, * VBO;
	glm::mat4* mvp;
	glm::vec2* info;
	Blocks blocks;
	BlockType* types;
	bool simulate, shouldStop;
	int TPS;
	double tickTime;
	bool mvpChanged = false;
	std::thread thread;
	std::mutex mutex;
	Engine::Window* window;

	BlockManager(Engine::Window* window, const float vertices[], int count);
	void set(int x, int y, Block* block);
	Block* get(int x, int y);
	bool has(int x, int y);
	void erase(int x, int y);
	void rotate(int x, int y, BlockRotation rotation);
	int length();
	void update();
	void setActive(int x, int y);
	void setActive(int x, int y, BlockRotation rotation);
	void setActive(int x, int y, BlockRotation rotation, int l);
	void uploadMVPBuffer(int count);
	void uploadInfoBuffer(int count);
	void draw(int count);
	bool save(const char* path);
	bool load(const char* path);
};

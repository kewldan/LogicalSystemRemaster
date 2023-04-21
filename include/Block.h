#pragma once

#include "engine/Texture.h"
#include "glm/glm.hpp"
#include <glm/gtx/transform.hpp>
#include <functional>

#define BlockRotation unsigned char

const glm::mat4 scaleMatrix = glm::scale(glm::mat4(1), glm::vec3(32, 32, 1));
const glm::mat4 originMatrix = glm::translate(glm::mat4(1), glm::vec3(-0.5, -0.5, 0));

class BlockType {
public:
	char id;
	std::function<bool(int)> isActive;
	
	BlockType();
	BlockType(char id, std::function<bool(int)> func);
};

BlockRotation rotateBlock(BlockRotation r, int k);

class Block {
public:
	int connections;
	BlockRotation rotation;
	bool active;
	BlockType* type;
	glm::mat4 mvp;
	glm::mat4 getMVP(long long x, int y);

	Block(BlockType* type, BlockRotation rotation);
	Block(const char* buffer, BlockType* types, long long *pos);
	Block();
	void write(char* buffer, long long pos);
public:
	static int X(long long l) {
		return static_cast<int>(l >> 32);
	}
	static int Y(long long l) {
		return static_cast<int>(l & 0xFFFFFFFFL);
	}
	static long long TO_LONG(int x, int y) {
		long long res = 0;
		res |= static_cast<long long>(x) << 32;
		res |= y & 0xFFFFFFFFL;
		return res;
	}
};
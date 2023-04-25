#pragma once

#include "Texture.h"
#include "glm/glm.hpp"
#include <glm/gtx/transform.hpp>
#include <functional>

#define BlockRotation unsigned char

const glm::mat4 scaleMatrix = glm::scale(glm::mat4(1), glm::vec3(32, 32, 1));
const glm::mat4 originMatrix = glm::translate(glm::mat4(1), glm::vec3(-0.5, -0.5, 0));
const glm::mat4 rotationMatrices[] = {
	glm::rotate(glm::mat4(1), glm::radians(-90.f), glm::vec3(0.f, 0.f, 1.f)),
	glm::rotate(glm::mat4(1), glm::radians(180.f), glm::vec3(0.f, 0.f, 1.f)),
	glm::rotate(glm::mat4(1), glm::radians(90.f), glm::vec3(0.f, 0.f, 1.f))
};

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
	glm::mat4 getMVP();

	Block(int x, int y, BlockType* type, BlockRotation rotation);
	Block(const char* buffer, BlockType* types, long long *pos);
	void write(char* buffer, long long pos);
	void updateMvp(int x, int y);
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
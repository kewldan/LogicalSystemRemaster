#include "Block.h"

BlockRotation rotateBlock(BlockRotation r, int k) {
	if (r == 0 && k == -1) return 3;
	return (r + k) % 4;
}

BlockType::BlockType()
{
	id = 0;
}

BlockType::BlockType(char id, std::function<bool(int)> func)
{
	this->id = id;
	isActive = func;
}

glm::mat4 Block::getMVP(long long x, int y)
{
	mvp = glm::translate(glm::mat4(1), glm::vec3(x, y, -0.1f));
	mvp *= scaleMatrix;
	if (rotation != 0) {
		mvp = glm::rotate(mvp, glm::radians(rotation == 2 ? 180.f : (rotation == 1 ? -90.f : 90.f)), glm::vec3(0.f, 0.f, 1.f));
	}
	mvp *= originMatrix;
	return mvp;
}

Block::Block(BlockType* type, BlockRotation rotation)
{
	this->type = type;
	this->rotation = rotation;
	mvp = glm::mat4(1);
	active = false;
	connections = 0;
}

Block::Block(const char* buffer, BlockType* types, long long* pos)
{
	connections = 0;
	mvp = glm::mat4(1);
	memcpy(pos, buffer, 8);
	int t = 0;
	memcpy(&t, buffer + 8, 1);
	type = &types[t];
	memcpy(&rotation, buffer + 9, 1);
	memcpy(&active, buffer + 10, 1);
}

Block::Block()
{
	active = false;
	connections = 0;
	mvp = glm::mat4(1);
	rotation = 0;
	type = 0;
}

void Block::write(char* buffer, long long pos)
{
	memcpy(buffer, &pos, 8);
	memcpy(buffer + 8, &type->id, 1);
	memcpy(buffer + 9, &rotation, 1);
	memcpy(buffer + 10, &active, 1);
}

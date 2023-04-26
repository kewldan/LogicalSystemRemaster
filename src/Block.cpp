#include "Block.h"

BlockRotation rotateBlock(BlockRotation r, int k) {
	if (r == 0 && k == -1) return 3;
	return (r + k) % 4;
}

long long Block_TO_LONG(int x, int y) {
	return static_cast<long long>(x) << 32 | (y & 0xFFFFFFFFL);
}

BlockType::BlockType()
{
	id = 0;
}

BlockType::BlockType(int id, std::function<bool(int)> func)
{
	this->id = id;
	isActive = func;
}

glm::mat4 Block::getMVP()
{
	return mvp;
}

Block::Block(int x, int y, BlockType* type, BlockRotation rotation)
{
	this->type = type;
	this->rotation = rotation;
	mvp = glm::mat4(1);
	active = false;
	connections = 0;
	selected = false;
	updateMvp(x << 5, y << 5);
}

Block::Block(const char* buffer, BlockType* types, long long* pos)
{
	connections = 0;
	mvp = glm::mat4(1);
	memcpy(pos, buffer, 8);
	int t = 0;
	memcpy(&t, buffer + 8, 1);
	type = &types[t];
	selected = false;
	memcpy(&rotation, buffer + 9, 1);
	memcpy(&active, buffer + 10, 1);
	updateMvp(Block_X(*pos) << 5, Block_Y(*pos) << 5);
}

void Block::write(char* buffer, long long pos)
{
	memcpy(buffer, &pos, 8);
	memcpy(buffer + 8, &type->id, 1);
	memcpy(buffer + 9, &rotation, 1);
	memcpy(buffer + 10, &active, 1);
}

void Block::updateMvp(int x, int y)
{
	mvp = glm::translate(glm::mat4(1), glm::vec3(x, y, -0.2f));
	mvp *= rotationMatrices[rotation];
}

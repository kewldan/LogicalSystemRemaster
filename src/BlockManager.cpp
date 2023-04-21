#include "BlockManager.h"

BlockManager::BlockManager()
{
	glGenTextures(1, &atlas);

	glBindTexture(GL_TEXTURE_2D_ARRAY, atlas);

	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

	int width, height, nrChannels;

	unsigned char* data = stbi_load("./data/textures/blocks.png", &width, &height, &nrChannels, 0);
	if (data) {
		assert((void("Image channels must be 3 or 4!"), nrChannels == 3 || nrChannels == 4));
		glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_RGBA,
			32, 32, 15, 0,
			GL_RGBA, GL_UNSIGNED_BYTE, data);


		glObjectLabelBuild(GL_TEXTURE, atlas, "Texture", "blocks.png");

		PLOGI << "Texture [blocks.png] loaded (" << width << "x" << height << ")";
	}
	else {
		PLOGE << "Failed to load texture [blocks.png]";
		PLOGE << stbi_failure_reason();
	}
	stbi_image_free(data);

	mvp = new glm::mat4[BLOCK_BATCHING];
	info = new glm::vec2[BLOCK_BATCHING];
	VBO = new unsigned int[3];

	glGenVertexArrays(1, &VAO);
	glGenBuffers(3, VBO);
	glBindVertexArray(VAO);

	glBindBuffer(GL_ARRAY_BUFFER, VBO[0]);
	glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), &quadVertices, GL_STATIC_DRAW);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
	glEnableVertexAttribArray(1);
	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));

	glBindBuffer(GL_ARRAY_BUFFER, VBO[1]);
	glBufferData(GL_ARRAY_BUFFER, sizeof(glm::vec2) * BLOCK_BATCHING, NULL, GL_DYNAMIC_DRAW);
	glEnableVertexAttribArray(2);
	glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 2, (void*)0);

	glBindBuffer(GL_ARRAY_BUFFER, VBO[2]);
	glBufferData(GL_ARRAY_BUFFER, sizeof(glm::mat4) * BLOCK_BATCHING, NULL, GL_DYNAMIC_DRAW);
	glEnableVertexAttribArray(3);
	glVertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE, 4 * sizeof(glm::vec4), (void*)0);
	glEnableVertexAttribArray(4);
	glVertexAttribPointer(4, 4, GL_FLOAT, GL_FALSE, 4 * sizeof(glm::vec4), (void*)(1 * sizeof(glm::vec4)));
	glEnableVertexAttribArray(5);
	glVertexAttribPointer(5, 4, GL_FLOAT, GL_FALSE, 4 * sizeof(glm::vec4), (void*)(2 * sizeof(glm::vec4)));
	glEnableVertexAttribArray(6);
	glVertexAttribPointer(6, 4, GL_FLOAT, GL_FALSE, 4 * sizeof(glm::vec4), (void*)(3 * sizeof(glm::vec4)));

	glVertexAttribDivisor(2, 1);
	glVertexAttribDivisor(3, 1);
	glVertexAttribDivisor(4, 1);
	glVertexAttribDivisor(5, 1);
	glVertexAttribDivisor(6, 1);

	types = new BlockType[15];
	types[0] = BlockType(0x0, [](int c) {return c > 0; });
	types[1] = BlockType(0x1, [](int c) {return c > 0; });
	types[2] = BlockType(0x2, [](int c) {return c > 0; });
	types[3] = BlockType(0x3, [](int c) {return c > 0; });
	types[4] = BlockType(0x4, [](int c) {return c > 0; });
	types[5] = BlockType(0x5, [](int c) {return c > 0; });
	types[6] = BlockType(0x6, [](int c) {return c > 0; });
	types[7] = BlockType(0x7, [](int c) {return c > 0; });
	types[8] = BlockType(0x8, [](int c) {return c >= 2; });
	types[9] = BlockType(0x9, [](int c) {return c >= 2; });
	types[10] = BlockType(0xA, [](int c) {return c % 2 == 1; });
	types[11] = BlockType(0xB, [](int c) {return c % 2 == 1; });
	types[12] = BlockType(0xC, [](int c) {return false; });
	types[13] = BlockType(0xD, [](int c) {return c > 0; });
	types[14] = BlockType(0xE, [](int c) {return c > 0; });
}

void BlockManager::set(int x, int y, Block* block)
{
	blocks[Block::TO_LONG(x, y)] = block;
}

Block* BlockManager::get(int x, int y)
{
	if (has(x, y))
		return blocks[Block::TO_LONG(x, y)];
	else
		return nullptr;
}

bool BlockManager::has(int x, int y)
{
	return blocks.contains(Block::TO_LONG(x, y));
}

void BlockManager::erase(int x, int y)
{
	if (has(x, y))
		blocks.erase(Block::TO_LONG(x, y));
}

int BlockManager::length()
{
	return (int)blocks.size();
}

void BlockManager::update()
{
	for (auto it = blocks.begin(); it != blocks.end(); ++it) {
		Block* block = it->second;
		int x = Block::X(it->first);
		int y = Block::Y(it->first);
		BlockRotation r = block->rotation;

		if (!block->active) { // Not family
			if (block->type->id == types[7].id || block->type->id == types[9].id || block->type->id == types[11].id) { // Not, Nand, Nxor
				if (!block->active) setActive(x, y, r);
			}
		}

		if (block->type->id == types[13].id) { // Timer
			block->active ^= 1;
			if (block->active) setActive(x, y, r);
		}

		if (block->active) {
			if (block->type->id == types[0].id || block->type->id == types[8].id || block->type->id == types[10].id) { // Wire, And, Xor
				setActive(x, y, r);
			}
			else if (block->type->id == types[5].id) { // 2 wire
				setActive(x, y, r, 2);
			}
			else if (block->type->id == types[6].id) { // 3 wire
				setActive(x, y, r, 3);
			}
			else if (block->type->id == types[1].id) {
				setActive(x, y, r);
				setActive(x, y, rotateBlock(r, 1));
			}
			else if (block->type->id == types[2].id) {
				setActive(x, y, r);
				setActive(x, y, rotateBlock(r, -1));
			}
			else if (block->type->id == types[3].id) {
				setActive(x, y, rotateBlock(r, -1));
				setActive(x, y, rotateBlock(r, 1));
			}
			else if (block->type->id == types[4].id) {
				setActive(x, y, rotateBlock(r, -1));
				setActive(x, y, r);
				setActive(x, y, rotateBlock(r, 1));
			}
			else if (block->type->id == types[12].id) { // Button
				setActive(x + 1, y);
				setActive(x, y - 1);
				setActive(x, y + 1);
				setActive(x - 1, y);
				setActive(x, y);
			}
		}
	}

	for (auto it = blocks.begin(); it != blocks.end(); ++it) {
		Block* block = it->second;
		if (block->type->id != 13 && block->type->id != 12) { // Except timer
			block->active = block->type->isActive(block->connections);
		}
		block->connections = 0;
	}
}

void BlockManager::setActive(int x, int y)
{
	if (has(x, y)) {
		get(x, y)->connections += 1;
	}
}

void BlockManager::setActive(int x, int y, BlockRotation rotation)
{
	setActive(x, y, rotation, 1);
}

void BlockManager::setActive(int x, int y, BlockRotation rotation, int l)
{
	if (rotation == 0) {
		setActive(x, y + l);
	}
	else if (rotation == 1) {
		setActive(x + l, y);
	}
	else if (rotation == 2) {
		setActive(x, y - l);
	}
	else if (rotation == 3) {
		setActive(x - l, y);
	}
}

void BlockManager::uploadBuffers(int count)
{
	glBindBuffer(GL_ARRAY_BUFFER, VBO[1]);
	glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(glm::vec2) * count, info);
	glBindBuffer(GL_ARRAY_BUFFER, VBO[2]);
	glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(glm::mat4) * count, mvp);
	glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, count);
}

bool BlockManager::save(const char* path)
{
	char* bin = new char[blocks.size() * 11];
	int o = 0;
	for (auto it = blocks.begin(); it != blocks.end(); ++it) {
		it->second->write(bin + o, it->first);
		o += 11;
	}
	std::ofstream stream(path, std::ios::out | std::ios::binary);

	if (!stream)
		return 0;

	stream.write(bin, blocks.size() * 11);
	stream.close();
	delete[] bin;

	return stream.good();
}

bool BlockManager::load(const char* path)
{
	std::ifstream stream(path, std::ios::out | std::ios::binary);
	if (!stream)
		return 0;
	int size = (int)std::filesystem::file_size(path);
	char* bin = new char[size];
	stream.read(bin, size);
	stream.close();

	blocks.clear();

	long long pos = 0LL;
	for (int i = 0; i < size / 11; i++) {
		Block* block = new Block(bin + i * 11L, types, &pos);
		blocks[pos] = block;
	}

	return 1;
}

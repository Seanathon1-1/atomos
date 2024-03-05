#pragma once

#include <glm.hpp>
#include <stdint.h>


template <typename NodeType>
class GridContainer {
protected:
	static uint32_t nextID;
	uint16_t width;
	uint16_t height;
	std::template vector<NodeType*> gridSquares;

	NodeType* getCell(int x, int y) { return gridSquares.at(y * width + x); }
	NodeType* getCell(glm::u8vec2 pos) { return getCell(pos.x, pos.y); };
public:
	GridContainer(uint16_t m, uint16_t n) {
		width = m;
		height = n;
		
		for (int i = 0; i < m * n; i++) {
			gridSquares.push_back(new NodeType());
		}
	}
	~GridContainer() {	for (NodeType* v : gridSquares) delete v; }
	template <typename NodeObject>bool insert(NodeObject* object, glm::vec2 position) {
		if (position.x < 0 || position.x >= width || position.y < 0 || position.y >= height) return 0;
		assert(getCell(position)->insert(object));
		return 1;
	}
	void clear() { for (NodeType* v : gridSquares) v->clear(); }
};
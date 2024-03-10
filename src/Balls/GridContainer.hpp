#pragma once

#include <glm.hpp>
#include <stdint.h>
#include <concepts>


template <typename T>
concept Placeable = requires (T t) { t.position; };

template<typename T>
concept Boundable = requires (T t) { t.minimumBound; t.maximumBound; };

template <Boundable NodeType>
class GridContainer {
protected:
	static uint32_t nextID;
	uint16_t width;
	uint16_t height;
	uint16_t nodeSize;
	std::template vector<NodeType*> gridSquares; 

	NodeType* getCell(int x, int y) { return gridSquares.at(y * width + x); }
	NodeType* getCell(glm::u16vec2 pos) { return getCell(pos.x, pos.y); };
	NodeType* getCellFromPosition(float x, float y) { return gridSquares.at(floor(y / nodeSize) * width + floor(x / nodeSize)); }
	NodeType* getCellFromPosition(glm::vec2 pos) { return getCellFromPosition(pos.x, pos.y); }
public:
	GridContainer(uint16_t m, uint16_t n, uint16_t size) {
		width = m;
		height = n;
		nodeSize = size;
		
		for (int i = 0; i < m * n; i++) {
			gridSquares.push_back(new NodeType(i % width, i / width, size));
		}
	}
	~GridContainer() {	for (NodeType* v : gridSquares) delete v; }
	glm::u16vec2 getGridIndex(glm::vec2 position) {
		uint16_t x = floor(position.x / nodeSize);
		uint16_t y = floor(position.y / nodeSize);
		return glm::u16vec2(x, y);
	}
	template <Placeable NodeObject>bool insert(NodeObject* object) {
		glm::u16vec2 gridIndex = getGridIndex(object->position);
		if (gridIndex.x < 0 || gridIndex.x >= width || gridIndex.y < 0 || gridIndex.y >= height) return 0;
		
		assert(getCell(gridIndex)->insert(object));
		return 1;
	}
	void clear() { for (NodeType* v : gridSquares) v->clear(); }
};
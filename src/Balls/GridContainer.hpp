#pragma once

#include <glm.hpp>
#include <stdint.h>


template <typename T>
class GridContainer {
protected:
	static uint32_t nextID;
	uint8_t width;
	uint8_t height;
	std::template vector<T*> gridSquares;

	T* getCell(int x, int y) { return gridSquares.at(y * width + x); }
	T* getCell(glm::u8vec2 pos) { return getCell(pos.x, pos.y); };
public:
	GridContainer(uint16_t m, uint16_t n) {
		width = m;
		height = n;

		for (int i = 0; i < m * n; i++) {
			gridSquares.push_back(new T());
		}
	}
	~GridContainer() {	for (T* v : gridSquares) delete v; }
	template <typename TObject>bool insert(TObject* b, glm::u8vec2 position) {
		if (position.x < 0 || position.x >= width || position.y < 0 || position.y >= height) return 0;
		getCell(position)->insert(b);
		return 1;
	}
	void clear() { for (T* v : gridSquares) v->clear(); }
};
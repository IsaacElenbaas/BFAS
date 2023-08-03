#ifndef SHAPES_H
#define SHAPES_H
#include <forward_list>
#include <list>
#include <tuple>
#include <unordered_map>
#include <vector>
#include "../types.h"

// should be a highly composite number
// TODO: change to 60 after testing
#define SHAPE_COLLECTION_BEZIERS 6

// TODO: PFAS.cpp checks the map, unstales or calls add
class Shape {
public:
	std::forward_list<std::tuple<decltype(point().used_by.begin()), bool, decltype(point().used_by.end())>> shape;
private:
	std::forward_list<bezier*> shape_data;
	bool update;
	void _add(decltype(Shape::shape) shape);
public:
	bool stale;
	bool fresh;
	size_t size;
	point tl, br;
	size_t depth;
	std::unordered_map<bezier*, bool> beziers;
	std::forward_list<point*> color_coords;
	std::forward_list<double> colors;
	size_t color_count;
	bool color_update;
	friend class OpenGLShapeCollection;
	static void add(decltype(Shape::shape) shape);
	static void apply_adds();
	void release();
};

class OpenGLShapeCollection {
	std::list<Shape*> shapes;
	unsigned long vbos[3];
	unsigned long ssbos[2];
	void* data;
	size_t used = 0;
	size_t capacity = SHAPE_COLLECTION_BEZIERS;
	bool update = false;
public:
	friend void Shape::_add(decltype(shape) shape);
	OpenGLShapeCollection();
	~OpenGLShapeCollection();
	void draw();
	void pack(OpenGLShapeCollection* other);
};

class ShapeHasher {
public:
	size_t operator()(decltype(Shape::shape)* const& shape) const;
};
class ShapeComparator {
public:
	bool operator()(decltype(Shape::shape)* const& a, decltype(Shape::shape)* const& b) const;
};

extern std::unordered_map<decltype(Shape::shape)*, Shape*, ShapeHasher, ShapeComparator> shapes;
extern std::forward_list<OpenGLShapeCollection*> shape_collections;
#endif

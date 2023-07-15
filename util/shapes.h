#ifndef SHAPES_H
#define SHAPES_H
#include <forward_list>
#include <tuple>
#include <unordered_map>
#include <vector>
#include "../types.h"

// should be a highly composite number
// TODO: change to 60 after testing
#define SHAPE_COLLECTION_BEZIERS 6

// TODO: PFAS.cpp checks the map, unstales or calls add
class Shape {
	std::forward_list<bezier*> shape_data;
	bool update;
public:
	bool stale;
	size_t size;
	point tl, br;
	size_t depth;
	friend class OpenGLShapeCollection;
	std::forward_list<std::tuple<decltype(point().used_by.begin()), bool, decltype(point().used_by.end())>> shape;
	static void add(typeof(Shape::shape) shape);
	void release();
};

class OpenGLShapeCollection {
	std::forward_list<Shape*> shapes;
	unsigned long vbos[3];
	void* data;
	size_t used = 0;
	size_t capacity = SHAPE_COLLECTION_BEZIERS;
	bool update = false;
public:
	friend void Shape::add(typeof(shape) shape);
	OpenGLShapeCollection();
	~OpenGLShapeCollection();
	void draw();
	void pack(OpenGLShapeCollection* other);
};

class ShapeHasher {
public:
	size_t operator()(typeof(Shape::shape)* const& shape) const;
};
class ShapeComparator {
public:
	bool operator()(typeof(Shape::shape)* const& a, typeof(Shape::shape)* const& b) const;
};

extern std::unordered_map<typeof(Shape::shape)*, Shape*, ShapeHasher, ShapeComparator> shapes;
extern std::forward_list<OpenGLShapeCollection*> shape_collections;
#endif

#ifndef QUADTREE_H
#define QUADTREE_H
#include <forward_list>
#include "types.h"

template<typename T>
class QuadTree {
	size_t capacity;
	size_t size;
	point tl = {0, 0}, br = {cmax, cmax};
	std::forward_list<T*> contents;
	QuadTree<T>* parent = NULL;
	QuadTree<T>* nw = NULL;
	QuadTree<T>* ne = NULL;
	QuadTree<T>* sw = NULL;
	QuadTree<T>* se = NULL;
	QuadTree(size_t capacity, QuadTree<T>* parent, point tl, point br);
	static bool contains(point qtl, point qbr, point* p);
	inline bool contains(point* p) { return contains(tl, br, p); }
	static bool contains(point qtl, point qbr, bezier* b);
	inline bool contains(bezier* b) { return contains(tl, br, b); }
	static bool contains(point qtl, point qbr, point tl, point br);
	inline bool contains(point tl, point br) { return contains(QuadTree::tl, QuadTree::br, tl, br); }
	void check_delete(bool recurse);
public:
	QuadTree(size_t capacity);
	~QuadTree() noexcept(false);
	T* insert(T* e);
	T* remove(T* e);
	class Region {
		point tl, br;
		typename std::forward_list<T*>::iterator checking;
		QuadTree<T>* quad_checking;
		QuadTree<T>* last = NULL;
		bool removed = false;
		friend Region QuadTree::region(point tl, point br);
		Region(QuadTree<T>* parent, point tl, point br);
		T* _next(bool remove, bool peek);
	public:
		inline T* next(bool remove) { return _next(remove, false); }
		inline T* peek() { return _next(false, true); }
	};
	Region region(point tl, point br);
};
#endif

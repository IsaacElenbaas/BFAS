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
	QuadTree<T>* parent;
	QuadTree<T>* nw = NULL;
	QuadTree<T>* ne;
	QuadTree<T>* sw;
	QuadTree<T>* se;
	QuadTree(size_t capacity, QuadTree<T>* parent, const point& tl, const point& br);
	static bool contains(const point& qtl, const point& qbr, const point& p);
	inline bool contains(const point& p) { return contains(tl, br, p); }
	static bool contains(const point& qtl, const point& qbr, const bezier& b);
	inline bool contains(const bezier& b) { return contains(tl, br, b); }
	static bool contains(const point& qtl, const point& qbr, const point& tl, const point& br);
	inline bool contains(const point& tl, const point& br) { return contains(QuadTree::tl, QuadTree::br, tl, br); }
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
		friend Region QuadTree::region(const point& tl, const point& br);
		Region(QuadTree<T>* parent, const point& tl, const point& br);
		T* _next(bool remove, bool peek);
	public:
		Region();
		inline T* next(bool remove) { return _next(remove, false); }
		inline T* peek() { return _next(false, true); }
	};
	Region region(const point& tl, const point& br);
};
#endif

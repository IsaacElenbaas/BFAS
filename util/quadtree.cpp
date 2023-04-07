#include <algorithm>
#include <stdexcept>
#include "quadtree.h"

/*{{{ QuadTree*/
template <class T> QuadTree<T>::QuadTree(size_t capacity, QuadTree<T>* parent, point tl, point br) : capacity(capacity), tl(tl), br(br), parent(parent) {}
template <class T> QuadTree<T>::QuadTree(size_t capacity) : capacity(capacity) {}
template <class T> QuadTree<T>::~QuadTree() noexcept(false) {
	if(nw != NULL || !contents.empty()) {
		throw new std::logic_error("not empty at deconstruction");
	}
}
template <class T> typename::QuadTree<T>::Region QuadTree<T>::region(point tl, point br) { return Region(this, tl, br); }

	/*{{{ contains*/
template <class T> bool QuadTree<T>::contains(point qtl, point qbr, point* p) {
	return qtl.x <= p->x && p->x <= qbr.x &&
	       qtl.y <= p->y && p->y <= qbr.y;
}

// TODO: implement less lazily, actually intersect quadrilateral and QuadTree rect
template <class T> bool QuadTree<T>::contains(point qtl, point qbr, bezier* b) {
	return contains(qtl, qbr,
		{std::min({b->a1->x, b->h1->x, b->a2->x, b->h2->x}), std::min({b->a1->y, b->h1->y, b->a2->y, b->h2->y})},
		{std::max({b->a1->x, b->h1->x, b->a2->x, b->h2->x}), std::max({b->a1->y, b->h1->y, b->a2->y, b->h2->y})}
	);
}

template <class T> bool QuadTree<T>::contains(point qtl, point qbr, point tl, point br) {
	return  br.x >= qtl.x &&  br.y >= qtl.y &&
	       qbr.x >=  tl.x && qbr.y >=  tl.y;
}
	/*}}}*/

	/*{{{ T* insert(T* e)*/
template <class T> T* QuadTree<T>::insert(T* e) {
	// remove may unnecessarily decrement
	if(size < capacity) size = distance(contents.begin(), contents.end());
	if(size >= capacity) {
		if(nw == NULL) {
			point ce = (tl+br)/2;
			nw = new QuadTree(capacity, this, {tl.x,tl.y}, {ce.x,ce.y});
			ne = new QuadTree(capacity, this, {ce.x,tl.y}, {br.x,ce.y});
			sw = new QuadTree(capacity, this, {tl.x,ce.y}, {ce.x,br.y});
			se = new QuadTree(capacity, this, {ce.x,ce.y}, {br.x,br.y});
		}
		if(nw->contains(e)) return nw->insert(e);
		if(ne->contains(e)) return ne->insert(e);
		if(sw->contains(e)) return sw->insert(e);
		if(se->contains(e)) return se->insert(e);
	}
	size++;
	contents.push_front(e);
	return e;
}
	/*}}}*/

	/*{{{ T* remove(T* e)*/
template <class T> T* QuadTree<T>::remove(T* e) {
	contents.remove(e);
	size--;
	check_delete(true);
	if(nw != NULL) {
		if(nw->contains(e)) return nw->remove(e);
		if(ne->contains(e)) return ne->remove(e);
		if(sw->contains(e)) return sw->remove(e);
		if(se->contains(e)) return se->remove(e);
	}
	return e;
}
	/*}}}*/

	/*{{{ void check_delete()*/
template <class T> void QuadTree<T>::check_delete(bool recurse) {
	if(nw != NULL) {
		if(nw->nw != NULL || !nw->contents.empty()) return;
		if(ne->nw != NULL || !ne->contents.empty()) return;
		if(sw->nw != NULL || !sw->contents.empty()) return;
		if(se->nw != NULL || !se->contents.empty()) return;
		delete nw; nw = NULL;
		delete ne; ne = NULL;
		delete sw; sw = NULL;
		delete se; se = NULL;
	}
	if(recurse && parent != NULL) parent->check_delete(recurse);
}
	/*}}}*/
/*}}}*/

/*{{{ Region*/
template <class T> QuadTree<T>::Region::Region(QuadTree<T>* parent, point tl, point br) : tl(tl), br(br), quad_checking(parent) {
	checking = quad_checking->contents.begin();
}
template <class T> T* QuadTree<T>::Region::_next(bool remove, bool peek) {
	if(
		last == quad_checking->parent &&
		checking != quad_checking->contents.end()
	) {
		T* ret = *checking;
		if(!QuadTree<T>::contains(tl, br, ret)) {
			++checking;
			return _next(remove, peek);
		}
		if(!peek) ++checking;
		if(remove) {
			quad_checking->contents.remove(ret);
			quad_checking->size--;
			removed = true;
		}
		return ret;
	}
	else {
		while(true) {
			QuadTree<T>* new_last = quad_checking;
			if(last == quad_checking->parent)
				quad_checking = quad_checking->nw;
			else if(last == quad_checking->nw)
				quad_checking = quad_checking->ne;
			else if(last == quad_checking->ne)
				quad_checking = quad_checking->sw;
			else if(last == quad_checking->sw)
				quad_checking = quad_checking->se;
			if(quad_checking == NULL || last == quad_checking->se) {
				quad_checking = new_last->parent;
				last = new_last;
				if(quad_checking == NULL) return NULL;
				if(removed) {
					quad_checking->check_delete(false);
					if(quad_checking->nw == NULL) last = NULL; // will match quad_checking->se again
					else                          removed = false;
				}
				continue;
			}
			if(quad_checking->contains(tl, br)) {
				last = new_last;
				break;
			}
			last = quad_checking;
			quad_checking = quad_checking->parent;
			if(removed) {
				quad_checking->check_delete(false);
				if(quad_checking->nw == NULL) last = NULL; // will match quad_checking->se
				else                          removed = false;
			}
		}
		checking = quad_checking->contents.begin();
		return _next(remove, peek);
	}
}
/*}}}*/

template class QuadTree<point>;
template class QuadTree<bezier>;

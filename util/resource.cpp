#include "resource.h"
#include "shapes.h"

template <class T> T* Resource<T>::get() {
	if(!available.empty()) {
		T* ret = available.front();
		available.pop_front();
		return ret;
	}
	else {
		batches.push_front(new T[RESOURCE_BATCH]());
		for(int i = 1; i < RESOURCE_BATCH; i++) {
			available.push_back(&batches.front()[i]);
		}
		return &batches.front()[0];
	}
}

// TODO: free memory here eventually. . . but has to be at a good time, don't want comparisons with stale shapes to segfault
template <class T> void Resource<T>::release(T* p) { available.push_front(p); }

template <class T> Resource<T>::~Resource() {
	for(typename std::forward_list<T*>::iterator i = batches.begin(); i != batches.end(); ++i) {
		delete *i;
	}
}

#include "types.h"

template class Resource<point>;
template class Resource<bezier>;
template class Resource<Shape>;

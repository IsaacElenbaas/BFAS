#include "resource.h"

template <class T> T* Resource<T>::get() {
	if(!available.empty()) {
		T* ret = available.front();
		available.pop_front();
		return ret;
	}
	else {
		batches.push_front(new T[batch]);
		for(int i = 1; i < batch; i++) {
			available.push_back(&batches.front()[i]);
		}
		return &batches.front()[0];
	}
}

template <class T> void Resource<T>::release(T* p) { available.push_front(p); }

template <class T> Resource<T>::~Resource() {
	for(typename std::forward_list<T*>::iterator i = batches.begin(); i != batches.end(); ++i) {
		delete *i;
	}
}

template class Resource<point>;
template class Resource<bezier>;

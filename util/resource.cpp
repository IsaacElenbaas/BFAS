#include "resource.h"

template <class T> T* Resource<T>::get() {
	if(!available.empty()) {
		T* ret = available.front();
		available.pop_front();
		return ret;
	}
	else {
		ResourceBatch* new_head = new ResourceBatch;
		new_head->next = head;
		head = new_head;
		for(int i = 1; i < batch; i++) {
			available.push_back(&head->Ts[i]);
		}
		return &head->Ts[0];
	}
}

template <class T> void Resource<T>::release(T* p) { available.push_front(p); }

template class Resource<point>;
template class Resource<bezier>;

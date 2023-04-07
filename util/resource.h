#include <deque>
#include "types.h"

#define batch 10

template<typename T>
class Resource {
	std::deque<T*> available;
	struct ResourceBatch {
		T Ts[batch];
		ResourceBatch* next = NULL;
	};
	ResourceBatch* head = NULL;
public:
	T* get();
	void release(T* p);
};

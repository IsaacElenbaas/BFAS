#include <deque>
#include <forward_list>
#include "types.h"

#define batch 10

template<typename T>
class Resource {
	std::deque<T*> available;
	std::forward_list<T*> batches;
public:
	T* get();
	void release(T* p);
	~Resource();
};

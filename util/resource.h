#ifndef RESOURCE_H
#define RESOURCE_H
#include <deque>
#include <forward_list>

#define RESOURCE_BATCH 10

template<typename T>
class Resource {
	std::deque<T*> available;
	std::forward_list<T*> batches;
public:
	T* get();
	void release(T* p);
	~Resource();
};
#endif

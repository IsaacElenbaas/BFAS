#include <QOpenGLFunctions>
#include <iterator>
#include "resource.h"
#include "shapes.h"

/* TODO
Multiple OpenGL object groups of shapes
	Each shape is represented by a class
	Mark all shapes as non-present then when finding shapes hash by points, mark as present again or create new shape (if actually new shape or was modified, can store tl and br)
	Remove non-present shapes, every once in a while pack down
		Only pack if not changed in 10 changes or something - mark as new when made, only pack those not marked as new, mark all as old when packing
Three shape VBOs
	First gives:
		position x y depth
	Second gives:
		number of beziers in the shape
		index in the uniform SSBO of the first bezier in the shape
	Third gives:
		number of color points in the shape
		index in the second uniform VBO of the first color in the shape
One uniform SSBO for beziers
One uniform SSBO for colors
*/
std::unordered_map<typeof(Shape::shape)*, Shape*, ShapeHasher, ShapeComparator> shapes;
std::forward_list<OpenGLShapeCollection*> shape_collections;
Resource<Shape> shape_RS;

void Shape::add(typeof(Shape::shape) shape) {
	Shape* new_shape = shape_RS.get();
	new_shape->update = true;
	new_shape->stale = false;
	// need to copy data because moving a point would change the hash of the old shape and we can't find it anymore
	// or, worse, the hash *doesn't* change and because the data is equal (pointers to same things) the old entry is overwritten but its being stale not handled
	auto last_data = new_shape->shape_data.before_begin();
	auto last = new_shape->shape.before_begin();
	new_shape->tl = {cmax, cmax};
	new_shape->br = {0, 0};
	for(auto i = shape.begin(); i != shape.end(); ++i ) {
		bezier* b = (*std::get<0>(*i));
		new_shape->tl.x = std::min({new_shape->tl.x, b->a1->x, b->h1->x, b->a2->x, b->h2->x});
		new_shape->tl.y = std::min({new_shape->tl.y, b->a1->y, b->h1->y, b->a2->y, b->h2->y});
		new_shape->br.x = std::max({new_shape->br.x, b->a1->x, b->h1->x, b->a2->x, b->h2->x});
		new_shape->br.y = std::max({new_shape->br.y, b->a1->y, b->h1->y, b->a2->y, b->h2->y});
		bezier* temp = b->clone();
		last_data = new_shape->shape_data.insert_after(last_data, temp);
		last = new_shape->shape.insert_after(last, {last_data, std::get<1>(*i), std::tuple_element<2, typeof(*i)>::type()});
	}
	new_shape->depth = 0;
	shapes.insert({&new_shape->shape, new_shape});
	OpenGLShapeCollection* collection = shape_collections.front();
	new_shape->size = distance(shape.begin(), shape.end());
	if(new_shape->size > collection->capacity-collection->used) {
		if(collection->used != 0) {
			shape_collections.push_front(new OpenGLShapeCollection());
			collection = shape_collections.front();
		}
		if(new_shape->size > collection->capacity-collection->used) {
			delete[] static_cast<GLfloat*>(collection->data);
			collection->capacity = new_shape->size;
			collection->data = new GLfloat[2*4*collection->capacity];
			// TODO: make SSBO?
		}
	}
	collection->shapes.push_front(new_shape);
	collection->used += new_shape->size;
}

void Shape::release() {
	shapes.erase(&shape);
	for(auto i = shape_data.begin(); i != shape_data.end(); ++i ) { (*i)->release(); }
	shape_data.clear();
	shape.clear();
	shape_RS.release(this);
}

OpenGLShapeCollection::OpenGLShapeCollection() {
	data = new GLfloat[2*4*capacity];
	QOpenGLFunctions* f = QOpenGLContext::currentContext()->functions();
	GLuint vbos[3];
	f->glGenBuffers(3, vbos);
	this->vbos[0] = vbos[0];
	this->vbos[1] = vbos[1];
	this->vbos[2] = vbos[2];
	// TODO: make SSBO?
}

void OpenGLShapeCollection::draw() {
	if(used == 0) return;
	bool update = this->update;
	size_t index = 0;
	size_t length = 0;
	for(auto i = shapes.begin(), last = shapes.before_begin(); i != shapes.end(); ) {
		if((*i)->stale) {
			used -= (*i)->size;
			(*i)->release();
			update = true;
			i = shapes.erase_after(last);
		}
		else {
			if(update || (*i)->update) {
				// TODO: fill SSBO
				update = true;
				(*i)->update = false;
			}
			(*i)->stale = true;
			index += (*i)->size;
			length++;
			last = i;
			++i;
		}
	}
	QOpenGLFunctions* f = QOpenGLContext::currentContext()->functions();
	if(update) {
		// position x y depth
		// number of beziers in the shape
		// index in the uniform VBO of the first bezier in the shape
		GLfloat p[6*3*length], b[6*2*length];
		// TODO: remove
		GLfloat c[6*3*length];
		size_t p_i = 0, b_i = 0;
		index = 0;
		for(auto i = shapes.begin(); i != shapes.end(); ++i) {
			p[p_i+ 0] = (*i)->tl.x/(double)cmax; p[p_i+ 1] = (*i)->tl.y/(double)cmax; p[p_i+ 2] = (*i)->depth; b[b_i+ 3] = (*i)->size; b[b_i+ 4] = index;
			p[p_i+ 3] = (*i)->br.x/(double)cmax; p[p_i+ 4] = (*i)->tl.y/(double)cmax; p[p_i+ 5] = (*i)->depth; b[b_i+ 8] = (*i)->size; b[b_i+ 9] = index;
			p[p_i+ 6] = (*i)->tl.x/(double)cmax; p[p_i+ 7] = (*i)->br.y/(double)cmax; p[p_i+ 8] = (*i)->depth; b[b_i+13] = (*i)->size; b[b_i+14] = index;
			p[p_i+ 9] = (*i)->tl.x/(double)cmax; p[p_i+10] = (*i)->br.y/(double)cmax; p[p_i+11] = (*i)->depth; b[b_i+18] = (*i)->size; b[b_i+19] = index;
			p[p_i+12] = (*i)->br.x/(double)cmax; p[p_i+13] = (*i)->tl.y/(double)cmax; p[p_i+14] = (*i)->depth; b[b_i+23] = (*i)->size; b[b_i+24] = index;
			p[p_i+15] = (*i)->br.x/(double)cmax; p[p_i+16] = (*i)->br.y/(double)cmax; p[p_i+17] = (*i)->depth; b[b_i+28] = (*i)->size; b[b_i+29] = index;
			// TODO: remove
			c[p_i+ 0] = 0.25; c[p_i+ 1] = 0; c[p_i+ 2] = 0;
			c[p_i+ 3] = 0.25; c[p_i+ 4] = 0; c[p_i+ 5] = 0;
			c[p_i+ 6] = 0.25; c[p_i+ 7] = 0; c[p_i+ 8] = 0;
			c[p_i+ 9] = 0.25; c[p_i+10] = 0; c[p_i+11] = 0;
			c[p_i+12] = 0.25; c[p_i+13] = 0; c[p_i+14] = 0;
			c[p_i+15] = 0.25; c[p_i+16] = 0; c[p_i+17] = 0;
			p_i += 6*3;
			b_i += 6*2;
			index += (*i)->size;
		}
		f->glBindBuffer(GL_ARRAY_BUFFER, vbos[0]);
		f->glBufferData(GL_ARRAY_BUFFER, sizeof(p), p, GL_STATIC_DRAW);
		f->glBindBuffer(GL_ARRAY_BUFFER, vbos[1]);
		f->glBufferData(GL_ARRAY_BUFFER, sizeof(b), b, GL_STATIC_DRAW);
		f->glBindBuffer(GL_ARRAY_BUFFER, vbos[2]);
		f->glBufferData(GL_ARRAY_BUFFER, sizeof(c), c, GL_STATIC_DRAW);
		// TODO: send SSBO
		this->update = false;
	}
	f->glBindBuffer(GL_ARRAY_BUFFER, vbos[0]);
	f->glEnableVertexAttribArray(0);
	f->glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, NULL);
	f->glBindBuffer(GL_ARRAY_BUFFER, vbos[1]);
	f->glEnableVertexAttribArray(1);
	f->glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 0, NULL);
	f->glBindBuffer(GL_ARRAY_BUFFER, vbos[2]);
	f->glEnableVertexAttribArray(2);
	f->glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, 0, NULL);
	glDrawArrays(GL_TRIANGLES, 0, 6*length);
}

void OpenGLShapeCollection::pack(OpenGLShapeCollection* other) {
	// TODO: if self is empty and capacity != SHAPE_COLLECTION_BEZIERS, remember to reset to that size before packing
	// TODO: set update to true
}

size_t ShapeHasher::operator()(typeof(Shape::shape)* const& shape) const {
	std::hash<decltype(point().x)> hasher;
	size_t hash_value = 0;
	for(auto i = shape->begin(); i != shape->end(); ++i) {
		bezier* b = *std::get<0>(*i);
		if(!std::get<1>(*i)) {
			hash_value ^= hasher(b->a1->x^b->a1->y^b->h1->x^b->h1->y)+0x9e3779b9+(hash_value << 6)+(hash_value >> 2);
			hash_value ^= hasher(b->a2->x^b->a2->y^b->h2->x^b->h2->y)+0x9e3779b9+(hash_value << 6)+(hash_value >> 2);
		}
		else {
			hash_value ^= hasher(b->a2->x^b->a2->y^b->h2->x^b->h2->y)+0x9e3779b9+(hash_value << 6)+(hash_value >> 2);
			hash_value ^= hasher(b->a1->x^b->a1->y^b->h1->x^b->h1->y)+0x9e3779b9+(hash_value << 6)+(hash_value >> 2);
		}
	}
	return hash_value;
}

bool ShapeComparator::operator()(typeof(Shape::shape)* const& a, typeof(Shape::shape)* const& b) const {
	auto i = a->begin(), j = b->begin();
	for(; i != a->end(); ++i, ++j) {
		if(j == b->end()) return false;
		if(std::get<1>(*i) != std::get<1>(*j)) return false;
		bezier* b_a = *std::get<0>(*i);
		bezier* b_b = *std::get<0>(*j);
		if(
			*(b_a->a1) != *(b_b->a1) ||
			*(b_a->h1) != *(b_b->h1) ||
			*(b_a->a2) != *(b_b->a2) ||
			*(b_a->h2) != *(b_b->h2)
		) return false;
	}
	if(j != b->end()) return false;
	return true;
}

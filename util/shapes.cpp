#include <QOpenGLExtraFunctions>
#include <QOpenGLFunctions>
#include <iterator>
#include <limits>
#include "resource.h"
#include "shapes.h"
// TODO: remove
#include <iostream>

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
			collection->data = new GLfloat[6*(3+2)*collection->capacity];
			QOpenGLFunctions* f = QOpenGLContext::currentContext()->functions();
			f->glBindBuffer(GL_ARRAY_BUFFER, collection->vbos[0]);
			f->glBufferData(GL_ARRAY_BUFFER, collection->capacity*6*3*sizeof(GLfloat), NULL, GL_DYNAMIC_DRAW);
			f->glBindBuffer(GL_ARRAY_BUFFER, collection->vbos[1]);
			f->glBufferData(GL_ARRAY_BUFFER, collection->capacity*6*2*sizeof(GLfloat), NULL, GL_DYNAMIC_DRAW);
		}
	}
	new_shape->color_count = 0;
	collection->shapes.push_front(new_shape);
	collection->used += new_shape->size;
}

void Shape::release() {
	shapes.erase(&shape);
	for(auto i = shape_data.begin(); i != shape_data.end(); ++i ) { (*i)->release(); }
	shape_data.clear();
	shape.clear();
	color_coords.clear();
	colors.clear();
	shape_RS.release(this);
}

OpenGLShapeCollection::OpenGLShapeCollection() {
	data = new GLfloat[6*(3+2)*capacity];
	QOpenGLFunctions* f = QOpenGLContext::currentContext()->functions();
	GLuint vbos[3];
	f->glGenBuffers(3, vbos);
	this->vbos[0] = vbos[0];
	f->glBindBuffer(GL_ARRAY_BUFFER, vbos[0]);
	f->glBufferData(GL_ARRAY_BUFFER, capacity*6*3*sizeof(GLfloat), NULL, GL_DYNAMIC_DRAW);
	this->vbos[1] = vbos[1];
	f->glBindBuffer(GL_ARRAY_BUFFER, vbos[1]);
	f->glBufferData(GL_ARRAY_BUFFER, capacity*6*2*sizeof(GLfloat), NULL, GL_DYNAMIC_DRAW);
	this->vbos[2] = vbos[2];
	GLuint ssbos[2];
	f->glGenBuffers(2, ssbos);
	this->ssbos[0] = ssbos[0];
	this->ssbos[1] = ssbos[1];
}

void OpenGLShapeCollection::draw() {
	if(used == 0) return;
	bool update = this->update;
	bool color_update = false;
	size_t update_index = (update) ? 0 : std::numeric_limits<size_t>::max();
	std::vector<GLfloat> bezier_data;
	size_t index = 0;
	for(auto i = shapes.begin(), last = shapes.before_begin(); i != shapes.end(); ) {
		if((*i)->stale) {
			// TODO: not cleaning these up - should probably happen in pack though
			// TODO: leave one empty one
			used -= (*i)->size;
			(*i)->release();
			update = true;
			update_index = std::min(index, update_index);
			i = shapes.erase_after(last);
		}
		else {
			if(update || (*i)->update) {
				update = true;
				update_index = std::min(index, update_index);
				(*i)->update = false;
			}
			if((*i)->color_update) {
				color_update = true;
				(*i)->color_update = false;
			}
			(*i)->stale = true;
			index++;
			last = i;
			++i;
		}
	}
	QOpenGLFunctions* f = QOpenGLContext::currentContext()->functions();
	if(update) {
		GLfloat* p = &((GLfloat*)data)[0];
		GLfloat* b = &((GLfloat*)data)[6*3*index];
		size_t p_i = 0, b_i = 0;
		size_t bezier_index = 0;
		index = 0;
		for(auto i = shapes.begin(); i != shapes.end(); ++i) {
			if(index >= update_index) {
				p[p_i+ 0] = (*i)->tl.x/(double)cmax; p[p_i+ 1] = (*i)->tl.y/(double)cmax; p[p_i+ 2] = (*i)->depth;
				p[p_i+ 3] = (*i)->br.x/(double)cmax; p[p_i+ 4] = (*i)->tl.y/(double)cmax; p[p_i+ 5] = (*i)->depth;
				p[p_i+ 6] = (*i)->tl.x/(double)cmax; p[p_i+ 7] = (*i)->br.y/(double)cmax; p[p_i+ 8] = (*i)->depth;
				p[p_i+ 9] = (*i)->tl.x/(double)cmax; p[p_i+10] = (*i)->br.y/(double)cmax; p[p_i+11] = (*i)->depth;
				p[p_i+12] = (*i)->br.x/(double)cmax; p[p_i+13] = (*i)->tl.y/(double)cmax; p[p_i+14] = (*i)->depth;
				p[p_i+15] = (*i)->br.x/(double)cmax; p[p_i+16] = (*i)->br.y/(double)cmax; p[p_i+17] = (*i)->depth;
				b[b_i+ 0] = (*i)->size; b[b_i+ 1] = bezier_index;
				b[b_i+ 2] = (*i)->size; b[b_i+ 3] = bezier_index;
				b[b_i+ 4] = (*i)->size; b[b_i+ 5] = bezier_index;
				b[b_i+ 6] = (*i)->size; b[b_i+ 7] = bezier_index;
				b[b_i+ 8] = (*i)->size; b[b_i+ 9] = bezier_index;
				b[b_i+10] = (*i)->size; b[b_i+11] = bezier_index;
			}
			p_i += 6*3;
			b_i += 6*2;
			bezier_index += (*i)->size;
			for(auto j = (*i)->shape.begin(); j != (*i)->shape.end(); ++j ) {
				bezier* b = (*std::get<0>(*j));
				bezier_data.insert(bezier_data.end(), {
					(GLfloat)(b->a1->x/(double)cmax), (GLfloat)(b->a1->y/(double)cmax),
					(GLfloat)(b->h1->x/(double)cmax), (GLfloat)(b->h1->y/(double)cmax),
					(GLfloat)(b->a2->x/(double)cmax), (GLfloat)(b->a2->y/(double)cmax),
					(GLfloat)(b->h2->x/(double)cmax), (GLfloat)(b->h2->y/(double)cmax)
				});
			}
			index++;
		}
		// coordinates and bezier info
		f->glBindBuffer(GL_ARRAY_BUFFER, vbos[0]);
		f->glBufferSubData(GL_ARRAY_BUFFER, update_index*6*3*sizeof(GLfloat), (index-update_index)*6*3*sizeof(GLfloat), &p[update_index*6*3]);
		f->glBindBuffer(GL_ARRAY_BUFFER, vbos[1]);
		f->glBufferSubData(GL_ARRAY_BUFFER, update_index*6*2*sizeof(GLfloat), (index-update_index)*6*2*sizeof(GLfloat), &b[update_index*6*2]);
		// bezier SSBO
		f->glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssbos[0]);
		f->glBufferData(GL_SHADER_STORAGE_BUFFER, bezier_data.size()*sizeof(GLfloat), bezier_data.data(), GL_STATIC_DRAW);
		this->update = false;
	}
	if(update || color_update) {
		GLfloat c[6*2*index];
		std::vector<GLfloat> color_data;
		size_t c_i = 0;
		size_t color_index = 0;
		for(auto i = shapes.begin(); i != shapes.end(); ++i) {
			// color info
			c[c_i+ 0] = (*i)->color_count; c[c_i+ 1] = color_index;
			c[c_i+ 2] = (*i)->color_count; c[c_i+ 3] = color_index;
			c[c_i+ 4] = (*i)->color_count; c[c_i+ 5] = color_index;
			c[c_i+ 6] = (*i)->color_count; c[c_i+ 7] = color_index;
			c[c_i+ 8] = (*i)->color_count; c[c_i+ 9] = color_index;
			c[c_i+10] = (*i)->color_count; c[c_i+11] = color_index;
			// color SSBO
			auto k = (*i)->colors.begin();
			for(auto j = (*i)->color_coords.begin(); j != (*i)->color_coords.end(); ++j ) {
				color_data.insert(color_data.end(), {(GLfloat)((*j)->x/(double)cmax), (GLfloat)((*j)->y/(double)cmax)});
				color_data.insert(color_data.end(), *k); ++k;
				color_data.insert(color_data.end(), *k); ++k;
				color_data.insert(color_data.end(), *k); ++k;
				color_data.insert(color_data.end(), *k); ++k;
			}
			c_i += 6*2;
			color_index += (*i)->size;
		}
		// color info
		f->glBindBuffer(GL_ARRAY_BUFFER, vbos[2]);
		f->glBufferData(GL_ARRAY_BUFFER, sizeof(c), c, GL_STATIC_DRAW);
		// color SSBO
		f->glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssbos[1]);
		f->glBufferData(GL_SHADER_STORAGE_BUFFER, color_data.size()*sizeof(GLfloat), color_data.data(), GL_STATIC_DRAW);
		color_update = false;
	}
	// coordinates and bezier info
	f->glBindBuffer(GL_ARRAY_BUFFER, vbos[0]);
	f->glEnableVertexAttribArray(0);
	f->glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, NULL);
	f->glBindBuffer(GL_ARRAY_BUFFER, vbos[1]);
	f->glEnableVertexAttribArray(1);
	f->glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 0, NULL);
	// bezier SSBO
	f->glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssbos[0]);
	QOpenGLContext::currentContext()->extraFunctions()->glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, ssbos[0]);
	// color info
	f->glBindBuffer(GL_ARRAY_BUFFER, vbos[2]);
	f->glEnableVertexAttribArray(2);
	f->glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 0, NULL);
	// color SSBO
	f->glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssbos[1]);
	QOpenGLContext::currentContext()->extraFunctions()->glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, ssbos[1]);
	glDrawArrays(GL_TRIANGLES, 0, 6*index);
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

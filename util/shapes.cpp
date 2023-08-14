#include <QOpenGLExtraFunctions>
#include <QOpenGLFunctions>
#include <algorithm>
#include <iterator>
#include <limits>
#include "UI.h"
#include "quadtree.h"
#include "resource.h"
#include "shapes.h"
#include "state.h"

extern QuadTree<point> point_QT;
extern QuadTree<bezier> bezier_QT;

std::unordered_map<decltype(Shape::shape)*, Shape*, ShapeHasher, ShapeComparator> shapes;
static std::forward_list<decltype(Shape::shape)> add_shapes;
std::vector<OpenGLShapeCollection*> shape_collections;
Resource<Shape> shape_RS;

void Shape::add(decltype(Shape::shape) shape) { add_shapes.push_front(shape); }
void Shape::apply_adds() {
	// try to match up shapes with their old objects to preserve colors and active shape between quick disconnections
	size_t to_match = distance(add_shapes.begin(), add_shapes.end());
	for(auto i = shapes.begin(); i != shapes.end(); ++i) { if((*i).second->stale) to_match++; }
	if(to_match != 0) {
		Shape* new_state_s = NULL;
		Shape** adding_shapes = new Shape*[to_match];
		size_t i = 0;
		for(auto j = shapes.begin(); j != shapes.end(); ++j) {
			if((*j).second->stale) {
				adding_shapes[i] = (*j).second;
				i++;
			}
		}
		for( ; i < to_match; i++) {
			adding_shapes[i] = shape_RS.get();
			// no point in matching against fresh shapes
			if(adding_shapes[i]->beziers.empty()) {
				shape_RS.release(adding_shapes[i]);
				i--; to_match--;
			}
		}
		// checking in this order should be most recently released first
		for(size_t i = 0; i < to_match; i++) {
			auto before_best = add_shapes.before_begin();
			size_t best_count = 0;
			for(auto shape = add_shapes.begin(), last = before_best; shape != add_shapes.end(); last = shape++) {
				size_t count = 0;
				for(auto j = (*shape).begin(); j != (*shape).end(); ++j) {
					if(adding_shapes[i]->beziers.find(*std::get<0>(*j)) != adding_shapes[i]->beziers.end())
						count++;
				}
				if(count > best_count) {
					before_best = last;
					best_count = count;
				}
			}
			if(best_count == 0) {
				if(adding_shapes[i]->shape.empty())
					shape_RS.release(adding_shapes[i]);
				// collection needs to see it is stale and remove it
				else {
					shapes.erase(&adding_shapes[i]->shape);
					for(auto k = adding_shapes[i]->shape_data.begin(); k != adding_shapes[i]->shape_data.end(); ++k) { (**k).release(); }
					adding_shapes[i]->shape_data.clear();
					adding_shapes[i]->shape.clear();
				}
				if(state.s == adding_shapes[i]) state.s = NULL;
			}
			else {
				shapes.erase(&adding_shapes[i]->shape);
				for(auto k = adding_shapes[i]->shape_data.begin(); k != adding_shapes[i]->shape_data.end(); ++k) { (**k).release(); }
				adding_shapes[i]->shape_data.clear();
				adding_shapes[i]->shape.clear();
				adding_shapes[i]->_add(*(std::next(before_best)));
				// first instance in collection (new one) will see stale == false, second (old one) will see stale == true and remove it
				adding_shapes[i]->stale = false;
				add_shapes.erase_after(before_best);
				if(new_state_s == NULL || state.s == adding_shapes[i])
					new_state_s = adding_shapes[i];
			}
		}
		if(state.s == NULL && (new_state_s != NULL || distance(add_shapes.begin(), add_shapes.end()) > 0)) {
			// may be filled below, not here
			state.s = new_state_s;
			repaint(false);
		}
		// add shapes that didn't match with an old shape
		for(auto shape = add_shapes.begin(); shape != add_shapes.end(); ++shape) {
			Shape* new_shape = shape_RS.get();
			while(!new_shape->color_coords.empty()) {
				(*(new_shape->color_coords.front())).release();
				new_shape->color_coords.erase_after(new_shape->color_coords.before_begin());
			}
			new_shape->colors.clear();
			new_shape->color_count = 0;
			new_shape->depth = 0;
			new_shape->_add(*shape);
			// first instance in collection (new one) will see stale == false, second (old one) will see stale == true and remove it
			new_shape->stale = false;
			if(state.s == NULL)
				state.s = new_shape;
		}
		add_shapes.clear();
	}
	std::sort(shape_collections.begin(), shape_collections.end(), OpenGLShapeCollectionComparator());
	{
		QuadTree<point>::Region region = point_QT.region(state.tl, state.br);
		for(point* p = region.next(false); p != NULL; p = region.next(false)) {
			p->visible = false;
		}
	}
	{
		QuadTree<bezier>::Region region = bezier_QT.region(state.tl, state.br);
		for(bezier* b = region.next(false); b != NULL; b = region.next(false)) {
			if(!b->used || (state.s != NULL && state.s->beziers.find(b) != state.s->beziers.end())) {
				b->a1->visible = true; b->h1->visible = true;
				b->a2->visible = true; b->h2->visible = true;
			}
		}
	}
}
void Shape::_add(decltype(Shape::shape) shape) {
	beziers.clear();
	update = true;
	depth_update = false;
	// need to copy data because moving a point would change the hash of the old shape and we can't find it anymore
	// or, worse, the hash *doesn't* change and because the data is equal (pointers to same things) the old entry is overwritten but its being stale not handled
	auto last_data = shape_data.before_begin();
	auto last = this->shape.before_begin();
	tl = {cmax, cmax};
	br = {0, 0};
	for(auto i = shape.begin(); i != shape.end(); ++i) {
		bezier* b = *std::get<0>(*i);
		tl.x = std::min({tl.x, b->a1->x, b->h1->x, b->a2->x, b->h2->x});
		tl.y = std::min({tl.y, b->a1->y, b->h1->y, b->a2->y, b->h2->y});
		br.x = std::max({br.x, b->a1->x, b->h1->x, b->a2->x, b->h2->x});
		br.y = std::max({br.y, b->a1->y, b->h1->y, b->a2->y, b->h2->y});
		bezier* temp = (*b).clone();
		last_data = shape_data.insert_after(last_data, temp);
		// third component is unused
		last = this->shape.insert_after(last, {last_data, std::get<1>(*i), std::get<2>(*i)});
		beziers.insert({b, true});
	}
	shapes.insert({&this->shape, this});
	auto collection = std::lower_bound(
		shape_collections.begin(), shape_collections.end(),
		depth, [](OpenGLShapeCollection* const& collection, size_t depth) {
			if(collection->used == 0) return false;
			return collection->depth > depth;
		}
	);
	if(collection == shape_collections.end()) collection = --shape_collections.end();
	size = distance(shape.begin(), shape.end());
	while(
		size > (*collection)->capacity-(*collection)->used &&
		std::next(collection) != shape_collections.end() &&
		(*std::next(collection))->depth == depth
	)
		++collection;
	if((*collection)->depth != depth || size > (*collection)->capacity-(*collection)->used) {
		if((*collection)->used != 0) {
			collection = --shape_collections.end();
			if((*collection)->used != 0) {
				shape_collections.push_back(new OpenGLShapeCollection());
				collection = --shape_collections.end();
			}
		}
		if(size > (*collection)->capacity-(*collection)->used) {
			delete[] static_cast<GLfloat*>((*collection)->data);
			(*collection)->capacity = size;
			(*collection)->data = new GLfloat[6*(2+2)*(*collection)->capacity];
			QOpenGLFunctions* f = QOpenGLContext::currentContext()->functions();
			f->glBindBuffer(GL_ARRAY_BUFFER, (*collection)->vbos[0]);
			f->glBufferData(GL_ARRAY_BUFFER, (*collection)->capacity*6*2*sizeof(GLfloat), NULL, GL_DYNAMIC_DRAW);
			f->glBindBuffer(GL_ARRAY_BUFFER, (*collection)->vbos[1]);
			f->glBufferData(GL_ARRAY_BUFFER, (*collection)->capacity*6*2*sizeof(GLfloat), NULL, GL_DYNAMIC_DRAW);
		}
		(*collection)->depth = depth;
	}
	(*collection)->shapes.push_back(this);
	this->collection = *collection;
	(*collection)->used += size;
}

OpenGLShapeCollection::OpenGLShapeCollection() {
	data = new GLfloat[6*(2+2)*capacity];
	QOpenGLFunctions* f = QOpenGLContext::currentContext()->functions();
	GLuint vbos[3];
	f->glGenBuffers(3, vbos);
	this->vbos[0] = vbos[0];
	f->glBindBuffer(GL_ARRAY_BUFFER, vbos[0]);
	f->glBufferData(GL_ARRAY_BUFFER, capacity*6*2*sizeof(GLfloat), NULL, GL_DYNAMIC_DRAW);
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
	for(auto i = shapes.rbegin(); i != shapes.rend(); ) {
		if((*i)->stale || (*i)->collection != this) {
			// TODO: not cleaning these up - should probably happen in pack though
			// TODO: leave one empty one
			used -= (*i)->size;
			if((*i)->shape.empty()) shape_RS.release(*i);
			update = true;
			update_index = std::min(index, update_index);
			i = std::reverse_iterator(shapes.erase(std::next(i).base()));
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
			i++;
		}
	}
	QOpenGLFunctions* f = QOpenGLContext::currentContext()->functions();
	if(update) {
		GLfloat* p = &((GLfloat*)data)[0];
		GLfloat* b = &((GLfloat*)data)[6*2*index];
		size_t p_i = 0, b_i = 0;
		size_t bezier_index = 0;
		index = 0;
		for(auto i = shapes.rbegin(); i != shapes.rend(); ++i) {
			if(index >= update_index) {
				p[p_i+ 0] = (*i)->tl.x/(double)cmax; p[p_i+ 1] = (*i)->tl.y/(double)cmax;
				p[p_i+ 2] = (*i)->br.x/(double)cmax; p[p_i+ 3] = (*i)->tl.y/(double)cmax;
				p[p_i+ 4] = (*i)->tl.x/(double)cmax; p[p_i+ 5] = (*i)->br.y/(double)cmax;
				p[p_i+ 6] = (*i)->tl.x/(double)cmax; p[p_i+ 7] = (*i)->br.y/(double)cmax;
				p[p_i+ 8] = (*i)->br.x/(double)cmax; p[p_i+ 9] = (*i)->br.y/(double)cmax;
				p[p_i+10] = (*i)->br.x/(double)cmax; p[p_i+11] = (*i)->tl.y/(double)cmax;
				b[b_i+ 0] = (*i)->size; b[b_i+ 1] = bezier_index;
				b[b_i+ 2] = (*i)->size; b[b_i+ 3] = bezier_index;
				b[b_i+ 4] = (*i)->size; b[b_i+ 5] = bezier_index;
				b[b_i+ 6] = (*i)->size; b[b_i+ 7] = bezier_index;
				b[b_i+ 8] = (*i)->size; b[b_i+ 9] = bezier_index;
				b[b_i+10] = (*i)->size; b[b_i+11] = bezier_index;
			}
			p_i += 6*2;
			b_i += 6*2;
			bezier_index += (*i)->size;
			for(auto j = (*i)->shape.begin(); j != (*i)->shape.end(); ++j) {
				bezier* b = *std::get<0>(*j);
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
		f->glBufferSubData(GL_ARRAY_BUFFER, update_index*6*2*sizeof(GLfloat), (index-update_index)*6*2*sizeof(GLfloat), &p[update_index*6*2]);
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
		for(auto i = shapes.rbegin(); i != shapes.rend(); ++i) {
			// color info
			c[c_i+ 0] = (*i)->color_count; c[c_i+ 1] = color_index;
			c[c_i+ 2] = (*i)->color_count; c[c_i+ 3] = color_index;
			c[c_i+ 4] = (*i)->color_count; c[c_i+ 5] = color_index;
			c[c_i+ 6] = (*i)->color_count; c[c_i+ 7] = color_index;
			c[c_i+ 8] = (*i)->color_count; c[c_i+ 9] = color_index;
			c[c_i+10] = (*i)->color_count; c[c_i+11] = color_index;
			// color SSBO
			auto k = (*i)->colors.begin();
			for(auto j = (*i)->color_coords.begin(); j != (*i)->color_coords.end(); ++j) {
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
	}
	// coordinates and bezier info
	f->glBindBuffer(GL_ARRAY_BUFFER, vbos[0]);
	f->glEnableVertexAttribArray(0);
	f->glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, NULL);
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

size_t ShapeHasher::operator()(decltype(Shape::shape)* const& shape) const {
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

bool ShapeComparator::operator()(decltype(Shape::shape)* const& a, decltype(Shape::shape)* const& b) const {
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

bool OpenGLShapeCollectionComparator::operator()(OpenGLShapeCollection* const& a, OpenGLShapeCollection* const& b) const {
	if(a == NULL || a->used == 0) return false;
	if(b == NULL || b->used == 0) return true;
	if(a->depth > b->depth) return true;
	if(b->depth > a->depth) return false;
	return false;
}

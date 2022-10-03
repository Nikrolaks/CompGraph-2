#pragma once

#include <GL/glew.h>

#include <vector>
#include <set>
#include <map>
#include <queue>
#include <memory>
#include <algorithm>
#include <string>
#include <filesystem>
#include <cstddef>

#include "functions.hpp"

namespace fs = std::filesystem;

std::string load_shader(const std::filesystem::path& path);
GLuint create_shader(GLenum type, const char* source);
GLuint create_program(GLuint vertex_shader, GLuint fragment_shader);


class vertex
{
public:
	vec2 position;
	std::uint8_t color[4];

	vertex() = default;
	vertex(const vec2& pos, const std::vector<std::uint8_t> &rgba) : position(pos) {
		for (int i = 0; i < 4; ++i)
			color[i] = rgba[i];
	}
};

class series {
private:
	GLuint vao, ind;
	std::vector<GLuint> vbos;
	GLuint program;
	GLuint view_location, time_location;
	static float view[16];

protected:
	static float time;
	static int swidth, sheight;

	// 0 - vertex
	// 1 - fragment
	// to be continued...
	static GLuint make_program(const std::vector<std::string> &paths) {
		std::vector<GLuint> shaders(2);
		shaders[0] = create_shader(GL_VERTEX_SHADER, load_shader(paths[0]).c_str());
		shaders[1] = create_shader(GL_FRAGMENT_SHADER, load_shader(paths[1]).c_str());
		return create_program(shaders[0], shaders[1]);
	}

	virtual GLuint load_program() { return 0; }
	virtual void attrib_structure(GLuint ind) {
		glBindVertexArray(vao);
		glBindBuffer(GL_ARRAY_BUFFER, vbos[ind]);
	}
	void load_data(const std::vector<std::size_t> &to_be_upd,
		GLsizeiptr size, void *data) {
		glBindVertexArray(vao);
		std::size_t i = 0;
		for (auto ind : to_be_upd) {
			glBindBuffer(GL_ARRAY_BUFFER, vbos[ind]);
			glBufferData(GL_ARRAY_BUFFER, size, data, GL_DYNAMIC_DRAW);
			++i;
		}
	}

	void load_indexes(GLsizeiptr size, void *indexes) {
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ind);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, size, indexes, GL_DYNAMIC_DRAW);
	}

	void draw(std::size_t count, GLenum mode) {
		glUseProgram(program);
		glUniformMatrix4fv(view_location, 1, GL_TRUE, view);
		glUniform1f(time_location, time);
		glBindVertexArray(vao);
		glBindBuffer(GL_ARRAY_BUFFER, vbos[0]);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ind);
		glDrawElements(mode, count, GL_UNSIGNED_INT, nullptr);
	}

	void druw(std::size_t count, std::size_t first) {
		glUseProgram(program);
		glUniformMatrix4fv(view_location, 1, GL_TRUE, view);
		glUniform1f(time_location, time);
		glBindVertexArray(vao);
		//glBindBuffer(GL_ARRAY_BUFFER, vbos[0]);
		glDrawArrays(GL_LINE_STRIP, first, count);
	}

public:
	series(GLuint program, std::size_t count_of_vbo) : program(program), vbos(count_of_vbo) {
		glGenVertexArrays(1, &vao);
		glGenBuffers(count_of_vbo, vbos.data());
		glGenBuffers(1, &ind);
		glBindVertexArray(vao);
		view_location = glGetUniformLocation(program, "view");
		time_location = glGetUniformLocation(program, "time");
	}

	virtual ~series() {}

	// left button
	virtual void mouse_update(int mouse_x, int mouse_y) {}
	// right button
	virtual void mouse_update() {}
	// left-right arrow
	virtual void key_update(int dir) {}
	// up-down arrow
	virtual void key_update(int dir, bool dummy) {}

	// I need normal animation system aaaaaaaaa
	virtual void resize(int width, int height) {
		// 0  1  2  3
		// 4  5  6  7
		// 8  9  10 11
		// 12 13 14 15

		swidth = width;
		sheight = height;

		view[0] = 2.0 / width;
		view[3] = -1.f;
		view[5] = -2.0 / height;
		view[7] = 1.f;
	}

	// so so so so
	virtual void resize(int sparsity) {}

	static void timer(float new_time) {
		time = new_time;
	}

	virtual void draw() {}
};

class isolines : public series {
	std::shared_ptr<function> f;
	std::vector<vec2> points;
	std::vector<std::uint32_t> ind;
	std::vector<std::pair<std::uint32_t, std::uint32_t>> grid;

	int cx = 0,
		cy = 0,
		w = 0,
		h = 0;

	vec2 find_point(float x_1, float y_1, float x_2, float y_2, float v_1, float v_2, float c) {
		return vec2(x_1 * cx, y_1 * cy).interpolate(vec2(x_2 * cx, y_2 * cy), (c - v_1) / (v_2 - v_1));
	}

	typedef void (isolines:: *procedure_t)(int x, int y, int lu, float c, const std::vector<float> &values);

	void process_initial_above(int x, int y, int lu, float c, const std::vector<float> &values) {
		ind.push_back(points.size());
		points.push_back(find_point(x, 0, x + 1, 0, values[lu], values[lu + 1], c));
	}

	void process_initial_left(int x, int y, int lu, float c, const std::vector<float> &values) {
		ind.push_back(points.size());
		points.push_back(find_point(0, y, 0, y + 1, values[lu], values[lu + w + 1], c));
	}

	void process_simple_above(int x, int y, int lu, float c, const std::vector<float> &values) {
		ind.push_back(grid[lu - w - 1].second);
	}

	void process_simple_left(int x, int y, int lu, float c, const std::vector<float> &values) {
		ind.push_back(grid[lu - 1].first);
	}

	void process_right(int x, int y, int lu, float c, const std::vector<float> &values) {
		ind.push_back(points.size());
		grid[lu].first = points.size();
		points.push_back(find_point(x + 1, y, x + 1, y + 1, values[lu + 1], values[lu + w + 2], c));
	}

	void process_below(int x, int y, int lu, float c, const std::vector<float> &values) {
		ind.push_back(points.size());
		grid[lu].second = points.size();
		points.push_back(find_point(x, y + 1, x + 1, y + 1, values[lu + w + 1], values[lu + w + 2], c));
	}

	void process_diag(int x, int y, int lu, float c, const std::vector<float> &values) {
		ind.push_back(points.size());	
		ind.push_back(points.size());
		points.push_back(find_point(x, y + 1, x + 1, y, values[lu + w + 1], values[lu + 1], c));
	}

	procedure_t procedures [4];

	void process_ceil(float c, const std::vector<float> &values, int x, int y,
		int above_func, int left_func) {
		int lu = y * (w + 1) + x, ru = lu + 1, ld = lu + w + 1, rd = ld + 1;
		bool slu = values[lu] > c, sld = values[ld] > c,
			sru = values[ru] > c, srd = values[rd] > c;
		if (sru ^ sld) {
			// there is point on diag
			if (slu ^ sru)
				//above
				(this->*procedures[above_func])(x, y, lu, c, values);
			else
				// left
				(this->*procedures[left_func])(x, y, lu, c, values);

			// on diag
			process_diag(x, y, lu, c, values);

			if (srd ^ sru) {
				// right
				process_right(x, y, lu, c, values);
			}
			else {
				// below
				process_below(x, y, lu, c, values);
			}
		}
		else {
			// there is no point on diag
			if (slu ^ sru) {
				// there is line in upper triangle
				(this->*procedures[above_func])(x, y, lu, c, values);
				(this->*procedures[left_func])(x, y, lu, c, values);
			}
			if (srd ^ sru) {
				// there is line in lower triangle
				process_right(x, y, lu, c, values);
				process_below(x, y, lu, c, values);
			}
		}
	}

	void process_value(float c, const std::vector<float> &values) {
		// first ceil
		process_ceil(c, values, 0, 0, 0, 2);
		// upper and left border
		for (int x = 1; x < w; ++x)
			process_ceil(c, values, x, 0, 0, 3);
		for (int y = 1; y < h; ++y)
			process_ceil(c, values, 0, y, 1, 2);

		// inner ceils
		for (int x = 1; x < w; ++x)
			for (int y = 1; y < h; ++y)
				process_ceil(c, values, x, y, 1, 3);
	}

	void attrib_structure(GLuint dummy = 0) override {
		series::attrib_structure(0);
		glEnableVertexAttribArray(0);
		glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, ( void * )(0));
	}

public:
	isolines(std::shared_ptr<function> &func) : series(series::make_program({
			"shaders/std_vertex.glsl",
			"shaders/std_fragment.glsl"
		}), 1), f(func) {
		attrib_structure();
		procedures[0] = &isolines::process_initial_above;
		procedures[1] = &isolines::process_simple_above;
		procedures[2] = &isolines::process_initial_left;
		procedures[3] = &isolines::process_simple_left;
	}

	void resize(int ceil_width, int ceil_height, int width, int height) {
		cx = ceil_width;
		cy = ceil_height;
		w = width;
		h = height;
		grid.resize((w + 1) * (h + 1));
	}

	void build_isolines(std::vector<float> &values) {
		points.clear();
		ind.clear();

		for (auto c : f->consts)
			process_value(c, values);
		
		// loading
		series::load_data({ 0 }, sizeof(vec2) * points.size(), points.data());
		series::load_indexes(sizeof(std::uint32_t) * ind.size(), ind.data());
	}

	void draw() override {
		series::draw(ind.size(), GL_LINES);
	}
};

class canvas : public series {
private:
	isolines lines;
	std::shared_ptr<function> f;
	int wcount, hcount, sqsize;
	std::vector<vertex> grid;
	std::vector<std::uint32_t> indexes;

	const std::uint8_t top[3] = {
		255, 224, 47
	};
	const std::uint8_t tm[3] = {
		188, 255, 47
	};
	const std::uint8_t middle[3] = {
		47, 188, 255
	};
	const std::uint8_t mb[3] = {
		255, 47, 188
	};
	const std::uint8_t bottom[3] = {
		253, 94, 83
	};

	std::uint8_t interpolate(std::uint8_t left, std::uint8_t right, float t) {
		return left + (right - left) * t;
	}

	void color(std::uint8_t *to_change, float z) {
		z = 4 * (z - f->left_bound) / (f->right_bound - f->left_bound) - 2;
		if (z <= -1) {
			to_change[0] = interpolate(bottom[0], mb[0], 2 + z);
			to_change[1] = interpolate(bottom[1], mb[1], 2 + z);
			to_change[2] = interpolate(bottom[2], mb[2], 2 + z);
		}
		else if (z <= 0) {
			to_change[0] = interpolate(mb[0], middle[0], 1 + z);
			to_change[1] = interpolate(mb[1], middle[1], 1 + z);
			to_change[2] = interpolate(mb[2], middle[2], 1 + z);
		}
		else if (z <= 1) {
			to_change[0] = interpolate(middle[0], tm[0], z);
			to_change[1] = interpolate(middle[1], tm[1], z);
			to_change[2] = interpolate(middle[2], tm[2], z);
		}
		else {
			to_change[0] = interpolate(tm[0], top[0], z - 1);
			to_change[1] = interpolate(tm[1], top[1], z - 1);
			to_change[2] = interpolate(tm[2], top[2], z - 1);
		}
		to_change[3] = 1;
	}

	void build_grid() {
		lines.resize(sqsize, sqsize, wcount - 1, hcount  - 1);
		grid.resize(wcount * hcount);
		int i = 0, j = 0;
		for (auto it = grid.begin(); it != grid.end(); ++it) {
			(*it).position.y = i * sqsize;
			(*it).position.x = j * sqsize;
			i += j == wcount - 1;
			j = (j + 1) % wcount;
		}
		series::load_data({ 0 }, { GLsizeiptr(grid.size() * sizeof(vertex)) }, { grid.data() });

		indexes.resize((wcount - 1) * (hcount - 1) * 6);

		std::uint32_t cur_h = 0, cur_w = 0;
		for (std::size_t i = 0; i < indexes.size(); i += 6) {
			std::uint32_t left = cur_h * wcount + cur_w;
			indexes[i] = left;
			indexes[i + 1] = left + 1;
			indexes[i + 2] = left + wcount;
			indexes[i + 3] = left + wcount;
			indexes[i + 4] = left + 1;
			indexes[i + 5] = left + wcount + 1;
			cur_h += cur_w == wcount - 2;
			cur_w = (cur_w + 1) % (wcount - 1);
		}

		series::load_indexes(sizeof(std::uint32_t) * indexes.size(), indexes.data());
	}

	void attrib_structure(GLuint dummy = 0) override {
		series::attrib_structure(0);
		glEnableVertexAttribArray(0);
		glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(vertex), (void*)(0));

		series::attrib_structure(1);
		glEnableVertexAttribArray(1);
		glVertexAttribPointer(1, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(vertex),
			(void*)(offsetof(vertex, color)));
	}
public:
	canvas(std::shared_ptr<function> Func) : series(series::make_program({
			"shaders/canvas_vertex.glsl",
			"shaders/canvas_fragment.glsl"
		}), 2), f(Func), lines(Func) {
		sqsize = 15;
		attrib_structure();
	}

	void draw() override {
		std::vector<float> to_lines;
		f->update(series::time);
		for (auto it = grid.begin(); it != grid.end(); ++it) {
			to_lines.push_back(f->calc((*it).position.x,
				(*it).position.y, series::time));
			color((*it).color, *to_lines.rbegin());
		}
		series::load_data({ 1 }, { GLsizeiptr(grid.size() * sizeof(vertex)) }, { grid.data() });
		series::draw(indexes.size(), GL_TRIANGLES);

		lines.build_isolines(to_lines);
		lines.draw();
	}

	void resize(int width, int height) override {
		series::resize(width, height);
		wcount = width / sqsize + 2;
		hcount = height / sqsize + 2;
		build_grid();
	}

	void key_update(int dir) override {
		if (sqsize + dir > 0) {
			sqsize += dir;
			wcount = series::swidth / sqsize + 2;
			hcount = series::sheight / sqsize + 2;
			build_grid();
		}
	}

	void key_update(int dir, bool dummy) {
		f->update(dir);
	}
};

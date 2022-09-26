#pragma once

#include <GL/glew.h>

#include <vector>
#include <string>
#include <filesystem>
#include <cstddef>

namespace fs = std::filesystem;

std::string load_shader(const std::filesystem::path& path);
GLuint create_shader(GLenum type, const char* source);
GLuint create_program(GLuint vertex_shader, GLuint fragment_shader);

class vec2 {
public:
	float x;
	float y;

	vec2() = default;
	vec2(float x, float y) : x(x), y(y) {}
	vec2(const vec2 &oth) : x(oth.x), y(oth.y) {}
};

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
		glBindBuffer(GL_ARRAY_BUFFER, vbos[ind]);
	}
	void load_data(const std::vector<std::size_t> &to_be_upd,
		const std::vector<GLsizeiptr> &sizes, const std::vector<void *> &data) {
		glBindVertexArray(vao);
		std::size_t i = 0;
		for (auto ind : to_be_upd) {
			glBindBuffer(GL_ARRAY_BUFFER, vbos[ind]);
			glBufferSubData(GL_ARRAY_BUFFER, 0, sizes[i], data[i]);
			++i;
		}
		std::vector<float> a(250 * 4);
		std::vector<int> color(250 * 4);
	}

	void load_indexes(GLsizeiptr size, void *indexes) {
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ind);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, size, indexes, GL_DYNAMIC_DRAW);
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
	// left arrow
	virtual void key_update() {}
	// right arrow
	virtual void key_update(bool dummy) {}

	static void resize(int width, int height) {
		// 0  1  2  3
		// 4  5  6  7
		// 8  9  10 11
		// 12 13 14 15

		view[0] = 2.0 / width;
		view[3] = -1.f;
		view[5] = -2.0 / height;
		view[7] = 1.f;
	}

	static void timer(float new_time) {
		time = new_time;
	}

	virtual void draw(std::size_t count=0) {
		glUseProgram(program);
		glUniformMatrix4fv(view_location, 1, GL_TRUE, view);
		glUniform1f(time_location, time);
		glBindVertexArray(vao);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ind);
		// glBindBuffer(GL_ARRAY_BUFFER, vbo); ?
		glDrawArrays(GL_TRIANGLES, 0, count);
		glPointSize(10.f);
		//glDrawArrays(GL_POINTS, 0, count);
	}
};

class seething : public series {
private:
	int wcount, hcount, sqsize;
	std::vector<vertex> grid;
	std::vector<int> indexes;

	void attrib_structure(GLuint dummy = 0) override {
		series::attrib_structure(0);
		glEnableVertexAttribArray(0);
		glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE,
			12, (void*)(0));

		series::attrib_structure(1);
		glEnableVertexAttribArray(1);
		glVertexAttribPointer(1, 4, GL_UNSIGNED_BYTE, GL_TRUE,
			12, (void*)(0));
	}

	std::vector<std::uint8_t> calc_color(float x, float y) {
		static vec2 center(200, 100);
		static float scale = 3 * 3;
		float res = 255 * std::sin((x - center.x) * (x - center.x) + (y - center.y) * (y - center.y));
		if (res <= 0)
			return { std::uint8_t(255 - res), 0, 0, 1 };
		else
			return { 255, std::uint8_t(res), std::uint8_t(res), 1 };
	}
public:
	seething() : series(series::make_program({
			"shaders/seething_vertex.glsl",
			"shaders/seething_fragment.glsl"
		}), 2) {
		wcount = 25;
		hcount = 10;
		sqsize = 50;
		grid.resize(wcount * hcount);
		int i = 0, j = 0;
		for (auto it = grid.begin(); it != grid.end(); ++it) {
			(*it).position.y = i * sqsize;
			(*it).position.x = j * sqsize;
			std::vector<std::uint8_t> ret(calc_color((*it).position.x,
				(*it).position.y));
			(*it).color[0] = ret[0];
			(*it).color[1] = ret[1];
			(*it).color[2] = ret[2];
			(*it).color[3] = ret[3];
			i += j == wcount - 1;
			j = (j + 1) % wcount;
		}
		attrib_structure();
		series::load_data({ 0, 1 },
			{ GLsizeiptr(grid.size() * sizeof(vertex)),
			GLsizeiptr(grid.size() * sizeof(vertex)) }, { grid.data(),
			(char *)(grid.data()) + sizeof(vec2)});

		indexes.resize((wcount - 1) * (hcount - 1) * 6);

		int cur_h = 0, cur_w = 0;
		for (std::size_t i = 0; i < indexes.size(); i += 6) {
			int left = cur_h * wcount + cur_w;
			indexes[i] = left;
			indexes[i + 1] = left + wcount;
			indexes[i + 2] = left + wcount + 1;
			indexes[i + 3] = left;
			indexes[i + 4] = left + 1;
			indexes[i + 5] = left + wcount + 1;
			cur_h += cur_w == wcount - 1;
			cur_w = (cur_w + 1) % wcount;
		}

		series::load_indexes(sizeof(int) * indexes.size(), indexes.data());
	}

	void draw(std::size_t dummy = 0) override {
		series::draw(indexes.size());
	}
};

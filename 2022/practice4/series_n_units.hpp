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

#include "obj_parser.hpp"

namespace fs = std::filesystem;

std::string load_shader(const std::filesystem::path& path);
GLuint create_shader(GLenum type, const char* source);
GLuint create_program(GLuint vertex_shader, GLuint fragment_shader);


class vec2 {
public:
	float x, y;
	vec2(float x, float y) : x(x), y(y) {}
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
	GLuint view_location, transform_location;
	static float view[16];

protected:
	static float time;
	static int swidth, sheight;
	float transform[16] = {
		1.f, 0.f, 0.f, 0.f,
		0.f, 1.f, 0.f, 0.f,
		0.f, 0.f, 1.f, 0.f,
		0.f, 0.f, 0.f, 1.f
	};

	// 0 - vertex
	// 1 - fragment
	// to be continued...
	static GLuint make_program(const std::vector<std::string> &paths) {
		std::vector<GLuint> shaders(2);
		shaders[0] = create_shader(GL_VERTEX_SHADER, load_shader(paths[0]).c_str());
		shaders[1] = create_shader(GL_FRAGMENT_SHADER, load_shader(paths[1]).c_str());
		return create_program(shaders[0], shaders[1]);
	}

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
		glUniformMatrix4fv(transform_location, 1, GL_TRUE, transform);
		glBindVertexArray(vao);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ind);
		glDrawElements(mode, count, GL_UNSIGNED_INT, nullptr);
	}

public:
	series(GLuint program, std::size_t count_of_vbo) : program(program), vbos(count_of_vbo) {
		glGenVertexArrays(1, &vao);
		glGenBuffers(count_of_vbo, vbos.data());
		glGenBuffers(1, &ind);
		glBindVertexArray(vao);
		view_location = glGetUniformLocation(program, "view");
		transform_location = glGetUniformLocation(program, "transform");
	}

	virtual ~series() {}

	// left button
	virtual void mouse_update(int mouse_x, int mouse_y) {}
	// right button
	virtual void mouse_update() {}
	// left-right arrow
	virtual void key_update(int dir, float dt) {}
	// up-down arrow
	virtual void key_update(int dir, float dt, bool dummy) {}

	static void resize(int width, int height) {
		// 0  1  2  3
		// 4  5  6  7
		// 8  9  10 11
		// 12 13 14 15

		swidth = width;
		sheight = height;

		float R = float(width) / height;

		float near = 0.01, far = 1000, theta = std::acos(-1) / 2;

		float right = near * std::tan(theta / 2);

		float top = right / R;

		//view[0]  = 1;
		//view[5]	 = 1;
		//view[10] = 1;
		//view[15] = 1;

		view[0] = near / right;
		view[5] = near / top;
		view[10] = -(far + near) / (far - near);
		view[11] = -(2 * far * near) / (far - near);
		view[14] = -1;
		view[15] = 0;
	}

	static void timer(float new_time) {
		time = new_time;
	}

	virtual void draw() {}
};

class bunny : public series {
private:
	float bunny_x, bunny_y;
	const std::vector<int> rot;
	float speed = 5;
	std::string project_root = PROJECT_ROOT;
	obj_data obj = parse_obj(project_root + "/bunny_lowres.obj");

	void attrib_structure() {
		series::attrib_structure(0);
		glEnableVertexAttribArray(0);
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(obj_data::vertex),
			( void * )(offsetof(obj_data::vertex, position)));

		glEnableVertexAttribArray(1);
		glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(obj_data::vertex),
			( void * )(offsetof(obj_data::vertex, normal)));

		glEnableVertexAttribArray(2);
		glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(obj_data::vertex),
			( void * )(offsetof(obj_data::vertex, texcoord)));
	}
public:
	bunny(float x, float y, const std::vector<int> &rotation) : series(series::make_program({
		"shaders/vertex.glsl",
		"shaders/fragment.glsl"
		}), 1), bunny_x(x), bunny_y(y), rot(rotation) {
		attrib_structure();
		series::load_data({ 0 }, sizeof(obj_data::vertex) * obj.vertices.size(), obj.vertices.data());
		series::load_indexes(obj.indices.size() * sizeof(std::uint32_t), obj.indices.data());
	}

	void draw() override {
		// 0  1  2  3
		// 4  5  6  7
		// 8  9  10 11
		// 12 13 14 15

		transform[0] = 0.5;
		transform[5] = 0.5;
		transform[10] = 0.5;
		transform[3] = 0.5;
		transform[7] = 0.5;
		transform[11] = 0.5;
		transform[rot[0]] = 0.5;
		transform[rot[1]] = 0.5;
		transform[rot[2]] = 0.5;
		transform[rot[3]] = 0.5;

		// move
		transform[11] *= -5;
		transform[3] *= bunny_x;
		transform[7] *= bunny_y;

		// rotate
		transform[rot[0]] *= std::cos(time);
		transform[rot[1]] *= -std::sin(time);
		transform[rot[2]] *= std::sin(time);
		transform[rot[3]] *= std::cos(time);
		series::draw(obj.indices.size(), GL_TRIANGLES);
	}

	// left-right arrow
	void key_update(int dir, float dt) override {
		bunny_x += dir * speed * dt;
	}
	// up-down arrow
	void key_update(int dir, float dt, bool dummy) override {
		bunny_y += dir * speed * dt;
	}
};
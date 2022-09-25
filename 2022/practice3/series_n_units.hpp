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

struct vec2
{
	float x;
	float y;
};

struct vertex
{
	vec2 position;
	std::uint8_t color[4];
};

class series {
private:
	GLuint vao, vbo;
	GLuint program;
	GLuint view_location, time_location;
	static float view[16];
	static float time;

protected:
	virtual GLuint load_program() { return 0; }
	virtual void attrib_structure() {}
	void load_data(GLsizeiptr size, void *data) {
		glBindVertexArray(vao);
		glBindBuffer(GL_ARRAY_BUFFER, vbo);
		glBufferData(GL_ARRAY_BUFFER, size, data, GL_DYNAMIC_DRAW);
	}
public:
	series(GLuint program) : program(program) {
		glGenVertexArrays(1, &vao);
		glGenBuffers(1, &vbo);
		glBindVertexArray(vao);
		glBindBuffer(GL_ARRAY_BUFFER, vbo);
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
		glBindBuffer(GL_ARRAY_BUFFER, vbo);
		glDrawArrays(GL_LINE_STRIP, 0, count);
	}
};

class broken_line : private series {
private:
	bool update = false;
protected:
	std::vector<vertex> vertices;

	void attrib_structure() override {
		glEnableVertexAttribArray(0);
		glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE,
			sizeof(vertex), (void *)(0));

		glEnableVertexAttribArray(1);
		glVertexAttribPointer(1, 4, GL_UNSIGNED_BYTE, GL_TRUE,
			sizeof(vertex), (void *)(8));
	}

	GLuint load_program() override {
		auto vertex_shader = create_shader(GL_VERTEX_SHADER,
			load_shader(fs::absolute(
				"shaders/broken_line_vertex.glsl")).c_str());
		auto fragment_shader = create_shader(GL_FRAGMENT_SHADER,
			load_shader(fs::absolute(
				"shaders/broken_line_fragment.glsl")).c_str());
		return create_program(vertex_shader, fragment_shader);
	}

	void draw(std::size_t count=0) override {
		if (update) {
			series::load_data(
				sizeof(vertex) * vertices.size(), vertices.data());
			update = false;
		}
		series::draw(vertices.size());
		glDrawArrays(GL_POINTS, 0, vertices.size());
	}
public:
	broken_line() : series(load_program()) {
		attrib_structure();
		glLineWidth(5.f);
		glPointSize(10.f);
	}

	virtual ~broken_line() {}

	virtual void mouse_update(int mouse_x, int mouse_y) override {
		vertex to_push = { {mouse_x, mouse_y}, {
			rand() % 255,
			rand() % 255,
			rand() % 255,
			255
		}};
		vertices.push_back(to_push);
		update = true;
	}

	virtual void mouse_update() override {
		if (vertices.size())
			vertices.pop_back();
	}
};

class bezier : private broken_line, public series {
private:
	struct bezier_vertex {
		float dist;
		vertex point;
	};

	std::vector<bezier_vertex> vertices;

	vec2 one_point(float t)
	{
		std::vector<vec2> points(broken_line::vertices.size());

		for (std::size_t i = 0; i < broken_line::vertices.size(); ++i)
			points[i] = broken_line::vertices[i].position;

		// De Casteljau's algorithm
		for (std::size_t k = 0; k + 1 < points.size(); ++k) {
			for (std::size_t i = 0; i + k + 1 < points.size(); ++i) {
				points[i].x = points[i].x * (1.f - t) + points[i + 1].x * t;
				points[i].y = points[i].y * (1.f - t) + points[i + 1].y * t;
			}
		}

		return points[0];
	}
private:
	bool update = false;
	int quality = 4;

	void update_curve() {
		// calculating color
		std::uint8_t mean_color[4] = { broken_line::vertices[0].color[0],
									   broken_line::vertices[0].color[1],
									   broken_line::vertices[0].color[2], 255 };
		std::size_t step = 2;
		for (auto it = broken_line::vertices.cbegin() + 1;
			it != broken_line::vertices.cend(); ++it) {
			mean_color[0] = ((mean_color[0] * step) + (*it).color[0]) / step;
			mean_color[1] = ((mean_color[1] * step) + (*it).color[1]) / step;
			mean_color[2] = ((mean_color[2] * step) + (*it).color[2]) / step;
		}

		// calculating curve
		std::size_t segments = (broken_line::vertices.size() - 1) * quality;
		vertices.resize((broken_line::vertices.size() - 1)* quality + 1);
		for (std::size_t i = 0; i < vertices.size(); ++i) {
			vertices[i].point.position = one_point(float(i) / segments);
			vertices[i].point.color[0] = mean_color[0];
			vertices[i].point.color[1] = mean_color[1];
			vertices[i].point.color[2] = mean_color[2];
			vertices[i].point.color[3] = mean_color[3];
		}

		// fill dist
		vertices[0].dist = 0.f;
		for (std::size_t i = 1; i < vertices.size(); ++i)
			vertices[i].dist = vertices[i - 1].dist + std::hypot(
				vertices[i].point.position.x - 
					vertices[i - 1].point.position.x,
				vertices[i].point.position.y -
					vertices[i - 1].point.position.y);
	}

	GLuint load_program() override {
		auto vertex_shader = create_shader(GL_VERTEX_SHADER,
			load_shader(fs::absolute(
				"shaders/bezier_vertex.glsl")).c_str());
		auto fragment_shader = create_shader(GL_FRAGMENT_SHADER,
			load_shader(fs::absolute(
				"shaders/bezier_fragment.glsl")).c_str());
		return create_program(vertex_shader, fragment_shader);
	}

	void attrib_structure() override {
		glEnableVertexAttribArray(0);
		glVertexAttribPointer(0, 1, GL_FLOAT, GL_FALSE,
			sizeof(bezier_vertex), (void *)(0));

		glEnableVertexAttribArray(1);
		glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE,
			sizeof(bezier_vertex), (void *)(4));

		glEnableVertexAttribArray(2);
		glVertexAttribPointer(2, 4, GL_UNSIGNED_BYTE, GL_TRUE,
			sizeof(bezier_vertex), (void *)(12));
	}
public:
	bezier() : broken_line(), series(load_program()) {
		attrib_structure();
	}

	virtual ~bezier() {}

	// left button
	void mouse_update(int mouse_x, int mouse_y) override {
		broken_line::mouse_update(mouse_x, mouse_y);
		update_curve();
		update = true;
	}

	// right button
	void mouse_update() override {
		broken_line::mouse_update();
		if (broken_line::vertices.size()) {
			update_curve();
			update = true;
		}
	}

	// left arrow
	void key_update() override {
		if (quality > 1) {
			--quality;
			update_curve();
			update = true;
		}
	}

	// right arrow
	void key_update(bool dummy) override {
		++quality;
		update_curve();
		update = true;
	}

	void draw(std::size_t count = 0) override {
		broken_line::draw();
		if (update) {
			series::load_data(
				sizeof(bezier_vertex) * vertices.size(),
				vertices.data());
			update = false;
		}
		series::draw(vertices.size());
	}
};

#pragma once
#ifdef WIN32
#include <SDL.h>
#undef main
#else
#include <SDL2/SDL.h>
#endif

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
#include <iostream>

#include <glm/vec3.hpp>
#include <glm/mat4x4.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/scalar_constants.hpp>
#include <glm/gtx/string_cast.hpp>

#include "nikropars.hpp"
#include "stb_image.h"

namespace fs = std::filesystem;

std::string load_shader(const std::filesystem::path &path);
GLuint create_shader(GLenum type, const char *source);
GLuint create_program(GLuint vertex_shader, GLuint fragment_shader);


class animation_stats {
private:
	static animation_stats self;

	float near = 0.1f;
	float far = 100.f;

	float camera_distance = 3.f;
	float camera_x = 0.f;
	float camera_y = 0.f;
	float camera_angle = 0.f;

	animation_stats() {
		
	}
public:

	int width, height;

	float time;

	glm::mat4 projection, view;

	static auto &anim() {
		return self;
	}

	void update_state(std::map<SDL_Keycode, bool> &buttons, float dt) {
		if (buttons[SDLK_UP])
			camera_distance -= 4.f * dt;
		if (buttons[SDLK_DOWN])
			camera_distance += 4.f * dt;

		if (buttons[SDLK_LEFT])
			camera_angle += 2.f * dt;
		if (buttons[SDLK_RIGHT])
			camera_angle -= 2.f * dt;

		view = glm::mat4(1.f);
		view = glm::translate(view, { 0.f, 0.f, -camera_distance });
		view = glm::rotate(view, glm::pi<float>() / 6.f, { 1.f, 0.f, 0.f });
		view = glm::rotate(view, camera_angle, { 0.f, 1.f, 0.f });
		view = glm::translate(view, { 0.f, -0.5f, 0.f });

		float aspect = ( float )height / ( float )width;
		projection = glm::perspective(glm::pi<float>() / 3.f, (width * 1.f) / height, near, far);
	}
};

#define ANIM animation_stats::anim()

class series {
private:
	GLuint vao;
	std::vector<GLuint> vbos, idos;
	GLuint program;
	GLuint projection_location, view_location, time_location;
	GLuint texture_sampler_location, model_location;

protected:
	glm::mat4 model;

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

	void load_indexes(GLsizeiptr size, void *indexes, std::size_t idx) {
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, idos[idx]);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, size, indexes, GL_DYNAMIC_DRAW);
	}

	void draw(std::size_t count, std::size_t idx, GLuint texture) {
		glUseProgram(program);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		glEnable(GL_DEPTH_TEST);
		glEnable(GL_CULL_FACE);
		glViewport(0, 0, ANIM.width, ANIM.height);

		glUniformMatrix4fv(projection_location, 1, GL_TRUE, reinterpret_cast< float * >(&ANIM.projection));
		glUniformMatrix4fv(view_location, 1, GL_TRUE, reinterpret_cast<float *>(&ANIM.view));
		glUniform1f(time_location, ANIM.time);

		glUniformMatrix4fv(model_location, 1, GL_TRUE, reinterpret_cast< float * >(&model));
		glUniform1i(texture_sampler_location, idx);

		glBindVertexArray(vao);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, idos[idx]);
		glActiveTexture(GL_TEXTURE0 + idx);
		glBindTexture(GL_TEXTURE_2D, texture);

		glDrawElements(GL_TRIANGLES, count, GL_UNSIGNED_INT, nullptr);
	}

	void druw(std::size_t count, std::size_t idx) {
		glUseProgram(program);
		glUniformMatrix4fv(view_location, 1, GL_TRUE, reinterpret_cast< float * >(&ANIM.view));
		glUniform1f(time_location, ANIM.time);
		glBindVertexArray(vao);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, idos[idx]);
		glDrawElements(GL_LINE_STRIP, count, GL_UNSIGNED_INT, nullptr);
	}

public:
	series(GLuint program, std::size_t count_of_vbo, std::size_t count_of_idos) : program(program), vbos(count_of_vbo), idos(count_of_idos), model(1.f) {
		glGenVertexArrays(1, &vao);
		glGenBuffers(count_of_vbo, vbos.data());
		glGenBuffers(count_of_idos, idos.data());
		glBindVertexArray(vao);
		projection_location = glGetUniformLocation(program, "projection");
		view_location = glGetUniformLocation(program, "view");
		time_location = glGetUniformLocation(program, "time");
		texture_sampler_location = glGetUniformLocation(program, "texture_sampler");
		model_location = glGetUniformLocation(program, "model");
	}

	virtual ~series() {}

	virtual void draw() {}
};

class scene_drawer : public series {
private:
	std::size_t count_of_objects;
	std::vector<std::size_t> counts;
	std::vector<GLuint> textures;

private:
	void attrib_structure() {
		// attrib_t::vertices
		series::attrib_structure(0);
		glEnableVertexAttribArray(0);
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(scene::vertex), ( void * )(0));

		// attrib_t::normals
		series::attrib_structure(1);
		glEnableVertexAttribArray(1);
		glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(scene::vertex), ( void * )(offsetof(scene::vertex, normal)));

		// attrib_t::texcoords
		series::attrib_structure(2);
		glEnableVertexAttribArray(2);
		glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(scene::vertex), ( void * )(offsetof(scene::vertex, texture_coord)));
	}

	static void stupid_parser(const wchar_t *str, char **to) {
		int len = 0;
		for (; *str != 0; ++str, ++len)
			;
		str -= len;

		*to = new char[len + 1];
		for (int i = 0; i < len; ++i)
			(*to)[i] = str[i];
		(*to)[len] = 0;
	}

public:
	

	scene_drawer(const scene &obj) : series(series::make_program({
			"shaders/object_vertex.glsl",
			"shaders/object_fragment.glsl"
		}), 3, obj.objects.size()), count_of_objects(obj.objects.size()), counts(obj.objects.size()) {
		attrib_structure();

		load_data({ 0, 1, 2 }, sizeof(scene::vertex) * obj.verticies.size(), ( void * )(obj.verticies.data()));

		std::size_t cur_idx = 0;
		GLuint tmp;
		for (auto &object : obj.objects) {
			std::cout << "load " << object.first << " data to gpu" << std::endl;

			load_indexes(object.second.indices.size(), ( void * )(object.second.indices.data()), cur_idx);
			counts[cur_idx] = object.second.indices.size();

			glGenTextures(1, &tmp);
			textures.push_back(tmp);
			glActiveTexture(GL_TEXTURE0 + cur_idx);
			glBindTexture(GL_TEXTURE_2D, tmp);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

			int x, y, n;
			auto u = object.second.mtl.albedo.native();
			char *o;
			stupid_parser(u.c_str(), &o);
			unsigned char *texture_data = stbi_load(o, &x, &y, &n, 4);
			delete o;
			if (!texture_data)
				throw std::exception("can't load texture((");

			if (n == 4)
				glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, x, y, 0, GL_RGBA, GL_UNSIGNED_BYTE, texture_data);
			else if (n == 3)
				glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, x, y, 0, GL_RGBA, GL_UNSIGNED_BYTE, texture_data);

			glGenerateMipmap(GL_TEXTURE_2D);
			stbi_image_free(texture_data);

			++cur_idx;
		}
	}

	void draw() {
		for (int i = 0; i < count_of_objects; ++i)
			series::draw(counts[i], i, textures[i]);
	}
};

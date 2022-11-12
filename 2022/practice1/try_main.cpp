#ifdef WIN32
#include <SDL.h>
#undef main
#else

#include <SDL2/SDL.h>

#endif

#include <GL/glew.h>

#include <string_view>
#include <stdexcept>
#include <iostream>
#include <chrono>
#include <vector>
#include <map>
#include <cmath>
#include <fstream>
#include <sstream>
#include <cstring>
#include <cstdlib>

#define GLM_FORCE_SWIZZLE
#define GLM_ENABLE_EXPERIMENTAL

#include <glm/vec3.hpp>
#include <glm/mat4x4.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/scalar_constants.hpp>
#include <glm/gtx/string_cast.hpp>

// #define TINYOBJLOADER_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#include "nikropars.hpp"
#include "stb_image.h"

std::string to_string(std::string_view str) {
    return std::string(str.begin(), str.end());
}

void sdl2_fail(std::string_view message) {
    throw std::runtime_error(to_string(message) + SDL_GetError());
}

void glew_fail(std::string_view message, GLenum error) {
    throw std::runtime_error(to_string(message) + reinterpret_cast< const char * >(glewGetErrorString(error)));
}

const char vertex_shader_source[] =
R"(#version 330 core

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

layout (location = 0) in vec3 in_position;
layout (location = 1) in vec3 in_normal;
layout (location = 2) in vec2 in_texcoord;

out vec3 position;
out vec3 normal;
out vec2 texcoord;

void main()
{
    position = (model * vec4(in_position, 1.0)).xyz;
    gl_Position = projection * view * vec4(position, 1.0);
    normal = normalize(mat3(model) * in_normal);
    texcoord = in_texcoord;
}
)";

const char fragment_shader_source[] =
R"(#version 330 core

uniform vec3 camera_position;

uniform sampler2D albedo_samp;
uniform vec3 ambient_light;

uniform vec3 sun_direction;
uniform vec3 sun_color;

uniform vec3 point_light_position;
uniform vec3 point_light_color;
uniform vec3 point_light_attenuation;

uniform vec3 glossiness;
uniform float roughness;

in vec3 position;
in vec3 normal;
in vec2 texcoord;

layout (location = 0) out vec4 out_color;

vec3 adamara(vec3 cofs, vec3 v) {
    return vec3(cofs.r * v.r, cofs.g * v.g, cofs.b * v.b);
}

vec3 specular(vec3 direction, vec3 albedo) {
    float cosine = dot(normal, direction);
    vec3 reflected = 2.0 * normal * cosine - direction;
    float power = 1 / (roughness * roughness) - 1;
    vec3 view_direction = normalize(camera_position - position);
    return adamara(glossiness, albedo * pow(max(0.0,
        dot(reflected, view_direction)), power));
}

vec3 diffuse(vec3 direction, vec3 albedo) {
    return albedo * max(0.0, dot(normal, direction)) + specular(direction, albedo);
}

float attenuation(float r) {
    return 1.0 / dot(point_light_attenuation, vec3(1.0, r, r * r));
}

void main()
{
    vec3 albedo = texture(albedo_samp, texcoord).rgb;
    // vec3 ambient = albedo * ambient_light;
    // vec3 sun_light = diffuse(sun_direction, albedo) * sun_color;
    // vec3 dir = point_light_position - position;
    // float len = length(dir);
    // vec3 point_light = diffuse(dir / len, albedo) * point_light_color * attenuation(len);
    // vec3 color = ambient + sun_light + point_light;
    out_color = vec4(albedo, 1.0);
}
)";

GLuint create_shader(GLenum type, const char *source) {
    GLuint result = glCreateShader(type);
    glShaderSource(result, 1, &source, nullptr);
    glCompileShader(result);
    GLint status;
    glGetShaderiv(result, GL_COMPILE_STATUS, &status);
    if (status != GL_TRUE) {
        GLint info_log_length;
        glGetShaderiv(result, GL_INFO_LOG_LENGTH, &info_log_length);
        std::string info_log(info_log_length, '\0');
        glGetShaderInfoLog(result, info_log.size(), nullptr, info_log.data());
        throw std::runtime_error("Shader compilation failed: " + info_log);
    }
    return result;
}

GLuint create_program(GLuint vertex_shader, GLuint fragment_shader) {
    GLuint result = glCreateProgram();
    glAttachShader(result, vertex_shader);
    glAttachShader(result, fragment_shader);
    glLinkProgram(result);

    GLint status;
    glGetProgramiv(result, GL_LINK_STATUS, &status);
    if (status != GL_TRUE) {
        GLint info_log_length;
        glGetProgramiv(result, GL_INFO_LOG_LENGTH, &info_log_length);
        std::string info_log(info_log_length, '\0');
        glGetProgramInfoLog(result, info_log.size(), nullptr, info_log.data());
        throw std::runtime_error("Program linkage failed: " + info_log);
    }

    return result;
}

std::size_t long_strlen(const wchar_t *str) {
    std::size_t len = 0;
    while (*str != 0) {
        ++len;
        ++str;
    }
    return len;
}

void long_strcpy(const wchar_t *src, char *dst) {
    while (*src != 0) {
        *dst = *src;
        ++src;
        ++dst;
    }
    *dst = 0;
}

int main() try {
    if (SDL_Init(SDL_INIT_VIDEO) != 0)
        sdl2_fail("SDL_Init: ");

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);

    SDL_Window *window = SDL_CreateWindow("Graphics course practice 7",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        800, 600,
        SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_MAXIMIZED);

    if (!window)
        sdl2_fail("SDL_CreateWindow: ");

    int width, height;
    SDL_GetWindowSize(window, &width, &height);

    SDL_GLContext gl_context = SDL_GL_CreateContext(window);
    if (!gl_context)
        sdl2_fail("SDL_GL_CreateContext: ");

    if (auto result = glewInit(); result != GLEW_NO_ERROR)
        glew_fail("glewInit: ", result);

    if (!GLEW_VERSION_3_3)
        throw std::runtime_error("OpenGL 3.3 is not supported");

    glClearColor(0.8f, 0.8f, 1.f, 0.f);

    auto vertex_shader = create_shader(GL_VERTEX_SHADER, vertex_shader_source);
    auto fragment_shader = create_shader(GL_FRAGMENT_SHADER, fragment_shader_source);
    auto program = create_program(vertex_shader, fragment_shader);

    GLuint model_location = glGetUniformLocation(program, "model");
    GLuint view_location = glGetUniformLocation(program, "view");
    GLuint projection_location = glGetUniformLocation(program, "projection");
    GLuint camera_position_location = glGetUniformLocation(program, "camera_position");
    GLuint albedo_location = glGetUniformLocation(program, "albedo_samp");
    GLuint ambient_light_location = glGetUniformLocation(program, "ambient_light");
    GLuint sun_dir_location = glGetUniformLocation(program, "sun_direction");
    GLuint sun_color_location = glGetUniformLocation(program, "sun_color");
    GLuint pl_pos_location = glGetUniformLocation(program, "point_light_position");
    GLuint pl_color_location = glGetUniformLocation(program, "point_light_color");
    GLuint pl_attenuation_location = glGetUniformLocation(program, "point_light_attenuation");
    GLuint glossiness_location = glGetUniformLocation(program, "glossiness");
    GLuint roughness_location = glGetUniformLocation(program, "roughness");

    std::string project_root = "";
    std::string suzanne_model_path = project_root + "building/building.obj";
    scene suzanne(suzanne_model_path);

    GLuint suzanne_vao, suzanne_vbo;
    glGenVertexArrays(1, &suzanne_vao);
    glBindVertexArray(suzanne_vao);

    glGenBuffers(1, &suzanne_vbo);
    glBindBuffer(GL_ARRAY_BUFFER, suzanne_vbo);
    glBufferData(GL_ARRAY_BUFFER, suzanne.verticies.size() * sizeof(suzanne.verticies[0]), suzanne.verticies.data(),
        GL_STATIC_DRAW);

    std::size_t count_of_shapes = 0;
    for (auto &par : suzanne.objects)
        count_of_shapes += par.second.shapes.size();

    std::vector<GLuint> suzanne_ebos(count_of_shapes);

    glGenBuffers(count_of_shapes, suzanne_ebos.data());

    std::size_t idx = 0;
    for (auto &par : suzanne.objects) {
        for (auto &shape : par.second.shapes) {
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, suzanne_ebos[idx]);
            glBufferData(GL_ELEMENT_ARRAY_BUFFER, shape.indices.size() * sizeof(shape.indices[0]), shape.indices.data(),
                GL_STATIC_DRAW);
            ++idx;
        }
    }

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(scene::vertex), ( void * )(0));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(scene::vertex), ( void * )(12));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(scene::vertex), ( void * )(24));

    std::map<std::string, GLuint> texture_mtl_map;

    for (auto &par : suzanne.material_map) {
        GLuint texture;
        glGenTextures(1, &texture);
        texture_mtl_map[par.first] = texture;

        glBindTexture(GL_TEXTURE_2D, texture);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);

        int w, h, ch;
        auto long_pointer = par.second.albedo.native().c_str();
        char *norm_pointer = new char[long_strlen(long_pointer) + 1];
        long_strcpy(long_pointer, norm_pointer);

        auto albedo_texture = stbi_load(norm_pointer, &w, &h, &ch, 4);

        delete[] norm_pointer;

        if (ch == 4)
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, albedo_texture);
        else
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, albedo_texture);

        stbi_image_free(albedo_texture);

        glGenerateMipmap(GL_TEXTURE_2D);
    }

    auto last_frame_start = std::chrono::high_resolution_clock::now();

    float time = 0.f;

    std::map<SDL_Keycode, bool> button_down;

    bool transparent = false;

    float camera_distance = 3.f;
    float camera_x = 0.f;
    float camera_y = 0.f;
    float camera_angle = 0.f;

    bool running = true;
    while (running) {
        for (SDL_Event event; SDL_PollEvent(&event);)
            switch (event.type) {
            case SDL_QUIT:
                running = false;
                break;
            case SDL_WINDOWEVENT:
                switch (event.window.event) {
                case SDL_WINDOWEVENT_RESIZED:
                    width = event.window.data1;
                    height = event.window.data2;
                    break;
                }
                break;
            case SDL_KEYDOWN:
                button_down[event.key.keysym.sym] = true;
                // std::cout << event.key.keysym.sym << std::endl;
                break;
            case SDL_KEYUP:
                button_down[event.key.keysym.sym] = false;
                break;
            }

        if (!running)
            break;

        auto now = std::chrono::high_resolution_clock::now();
        float dt = std::chrono::duration_cast< std::chrono::duration<float> >(now - last_frame_start).count();
        last_frame_start = now;
        time += dt;

        if (button_down[SDLK_UP])
            camera_distance -= 4.f * dt;
        if (button_down[SDLK_DOWN])
            camera_distance += 4.f * dt;

        if (button_down[SDLK_LEFT])
            camera_angle += 1.f * dt;
        if (button_down[SDLK_RIGHT])
            camera_angle -= 1.f * dt;

        if (button_down[97])
            camera_x -= 4.f * dt;
        if (button_down[100])
            camera_x += 4.f * dt;
        if (button_down[119])
            camera_y -= 4.f * dt;
        if (button_down[115])
            camera_y += 4.f * dt;
        

        glViewport(0, 0, width, height);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glClearColor(0.8f, 0.8f, 1.f, 0.f);

        glEnable(GL_DEPTH_TEST);
        glEnable(GL_CULL_FACE);

        float near = 0.1f;
        float far = 100.f;

        glm::mat4 model(1.f);

        glm::mat4 view(1.f);
        view = glm::translate(view, { 0.f, 0.f, -camera_distance });
        view = glm::rotate(view, camera_angle, { 0.f, 1.f, 0.f });
        view = glm::translate(view, { -camera_x, -camera_y, 0.f });

        float aspect = ( float )height / ( float )width;
        glm::mat4 projection = glm::perspective(glm::pi<float>() / 3.f, (width * 1.f) / height, near, far);

        glm::vec3 camera_position = (glm::inverse(view) * glm::vec4(0.f, 0.f, 0.f, 1.f)).xyz();

        idx = 0;
        for (auto &par : suzanne.objects) {
            for (auto &surf : par.second.shapes) {
                glUseProgram(program);
                glUniformMatrix4fv(view_location, 1, GL_FALSE, reinterpret_cast< float * >(&view));
                glUniformMatrix4fv(projection_location, 1, GL_FALSE, reinterpret_cast< float * >(&projection));
                glUniform3fv(camera_position_location, 1, ( float * )(&camera_position));

                glUniform1i(albedo_location, 1);
                glUniform3f(ambient_light_location, 0.2f, 0.2f, 0.2f);
                glUniform3f(sun_dir_location, 0.f, std::sin(glm::pi<float>() / 3), std::cos(glm::pi<float>() / 3));
                glUniform3f(sun_color_location, 1.0f, 1.0f, 1.0f);
                glUniform3f(pl_pos_location, 2 * std::cos(time * 2), 0.f, 2 * std::sin(time * 2));
                glUniform3f(pl_color_location, 1.f, 0.f, 0.f);
                glUniform3f(pl_attenuation_location, 1.0f, 0.0f, 0.01f);

                glUniformMatrix4fv(model_location, 1, GL_FALSE, reinterpret_cast< float * >(&model));

                auto &material = suzanne.material_map[surf.mtl];

                glUniform3f(glossiness_location, material.glossiness[0],
                    material.glossiness[1], material.glossiness[2]);
                glUniform1f(roughness_location, material.roughness);

                glActiveTexture(GL_TEXTURE0 + 1);
                glBindTexture(GL_TEXTURE_2D, texture_mtl_map[surf.mtl]);

                glBindVertexArray(suzanne_vao);
                glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, suzanne_ebos[idx]);
                glDrawElements(GL_TRIANGLES, surf.indices.size(), GL_UNSIGNED_INT, nullptr);
                ++idx;
            }
        }

        SDL_GL_SwapWindow(window);
    }

    for (auto &par : texture_mtl_map)
        glDeleteTextures(1, &par.second);

    SDL_GL_DeleteContext(gl_context);
    SDL_DestroyWindow(window);
}
catch (std::exception const &e) {
    std::cerr << e.what() << std::endl;
    return EXIT_FAILURE;
}
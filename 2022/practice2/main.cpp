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
#include <cmath>

std::string to_string(std::string_view str)
{
    return std::string(str.begin(), str.end());
}

void sdl2_fail(std::string_view message)
{
    throw std::runtime_error(to_string(message) + SDL_GetError());
}

void glew_fail(std::string_view message, GLenum error)
{
    throw std::runtime_error(to_string(message) + reinterpret_cast<const char *>(glewGetErrorString(error)));
}

const char vertex_shader_source[] =
R"(#version 330 core

uniform mat4 transform;
uniform mat4 view;
uniform mat4 shift;

const vec2 PS[6] = vec2[6](
    vec2(cos(radians(30.0  )), sin(radians(30.0)  )),
    vec2(cos(radians(90.0 )), sin(radians(90.0) )),
    vec2(cos(radians(150.0)), sin(radians(150.0))),
    vec2(cos(radians(210.0)), sin(radians(210.0))),
    vec2(cos(radians(270.0)), sin(radians(270.0))),
    vec2(cos(radians(330.0)), sin(radians(330.0)))
);

const vec2 VERTICES[18] = vec2[18](
    vec2(0.0, 0.0), PS[0], PS[1],
    vec2(0.0, 0.0), PS[1], PS[2],
    vec2(0.0, 0.0), PS[2], PS[3],
    vec2(0.0, 0.0), PS[3], PS[4],
    vec2(0.0, 0.0), PS[4], PS[5],
    vec2(0.0, 0.0), PS[5], PS[0]
);

const vec3 PCOLORS[7] = vec3[7](
    vec3(0.25, 0.5, 0.75),
    vec3(0.25, 0.75, 0.5),
    vec3(0.5, 0.25, 0.75),
    vec3(0.5, 0.75, 0.25),
    vec3(0.75, 0.25, 0.5),
    vec3(0.75, 0.5, 0.25),
    vec3(1.0, 1.0, 1.0)
);

const vec3 COLORS[18] = vec3[18](
    PCOLORS[6], PCOLORS[0], PCOLORS[1],
    PCOLORS[6], PCOLORS[1], PCOLORS[2],
    PCOLORS[6], PCOLORS[2], PCOLORS[3],
    PCOLORS[6], PCOLORS[3], PCOLORS[4],
    PCOLORS[6], PCOLORS[4], PCOLORS[5],
    PCOLORS[6], PCOLORS[5], PCOLORS[0]
);

out vec3 color;

void main()
{
    vec2 pos = VERTICES[gl_VertexID];
    gl_Position = (view * shift * transform) * vec4(pos, 0.0, 1.0);
    color = COLORS[gl_VertexID];
}
)";

const char fragment_shader_source[] =
R"(#version 330 core

in vec3 color;

layout (location = 0) out vec4 out_color;

void main()
{
    out_color = vec4(color, 1.0);
}
)";

GLuint create_shader(GLenum type, const char * source)
{
    GLuint result = glCreateShader(type);
    glShaderSource(result, 1, &source, nullptr);
    glCompileShader(result);
    GLint status;
    glGetShaderiv(result, GL_COMPILE_STATUS, &status);
    if (status != GL_TRUE)
    {
        GLint info_log_length;
        glGetShaderiv(result, GL_INFO_LOG_LENGTH, &info_log_length);
        std::string info_log(info_log_length, '\0');
        glGetShaderInfoLog(result, info_log.size(), nullptr, info_log.data());
        throw std::runtime_error("Shader compilation failed: " + info_log);
    }
    return result;
}

GLuint create_program(GLuint vertex_shader, GLuint fragment_shader)
{
    GLuint result = glCreateProgram();
    glAttachShader(result, vertex_shader);
    glAttachShader(result, fragment_shader);
    glLinkProgram(result);

    GLint status;
    glGetProgramiv(result, GL_LINK_STATUS, &status);
    if (status != GL_TRUE)
    {
        GLint info_log_length;
        glGetProgramiv(result, GL_INFO_LOG_LENGTH, &info_log_length);
        std::string info_log(info_log_length, '\0');
        glGetProgramInfoLog(result, info_log.size(), nullptr, info_log.data());
        throw std::runtime_error("Program linkage failed: " + info_log);
    }

    return result;
}

int main() try
{
    if (SDL_Init(SDL_INIT_VIDEO) != 0)
        sdl2_fail("SDL_Init: ");

    SDL_Window * window = SDL_CreateWindow("Graphics course practice 2",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        800, 600,
        SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_MAXIMIZED);

    if (!window)
        sdl2_fail("SDL_CreateWindow: ");

    int width, height;
    SDL_GetWindowSize(window, &width, &height);

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

    SDL_GLContext gl_context = SDL_GL_CreateContext(window);
    if (!gl_context)
        sdl2_fail("SDL_GL_CreateContext: ");

    SDL_GL_SetSwapInterval(0);

    if (auto result = glewInit(); result != GLEW_NO_ERROR)
        glew_fail("glewInit: ", result);

    if (!GLEW_VERSION_3_3)
        throw std::runtime_error("OpenGL 3.3 is not supported");

    glClearColor(0.8f, 0.8f, 1.f, 0.f);

    GLuint vertex_shader = create_shader(GL_VERTEX_SHADER, vertex_shader_source);
    GLuint fragment_shader = create_shader(GL_FRAGMENT_SHADER, fragment_shader_source);

    GLuint program = create_program(vertex_shader, fragment_shader);

    GLuint vao;
    glGenVertexArrays(1, &vao);

    auto last_frame_start = std::chrono::high_resolution_clock::now();

    glUseProgram(program);

    GLint
        transform_loc = glGetUniformLocation(program, "transform"),
        view_loc = glGetUniformLocation(program, "view"),
        shift_loc = glGetUniformLocation(program, "shift");
    float scale = 0.5, time = 0.0;

    glBindVertexArray(vao);

    bool running = true;
    while (running)
    {
        for (SDL_Event event; SDL_PollEvent(&event);) switch (event.type)
        {
        case SDL_QUIT:
            running = false;
            break;
        case SDL_WINDOWEVENT: switch (event.window.event)
            {
            case SDL_WINDOWEVENT_RESIZED:
                width = event.window.data1;
                height = event.window.data2;
                glViewport(0, 0, width, height);
                break;
            }
            break;
        }

        if (!running)
            break;

        auto now = std::chrono::high_resolution_clock::now();
        float dt = std::chrono::duration_cast<std::chrono::duration<float>>(now - last_frame_start).count();
        last_frame_start = now;

        //dt = 0.016f;

        //std::cout << dt << std::endl;

        time += dt;

        scale = (1.0) / 2;

        float
            angle = time * 0.75,
            x = -0.5 + 0.5 * cos(angle),
            y = -0.5 + 0.5 * sin(angle),
            aspect_ratio = float(width) / height;

        float transform[16] = {
            cos(angle), -sin(angle),  0, 0,
            sin(angle), cos(angle),  0, 0,
            0, 0, 1, 0,
            0, 0, 0, 1
        };

        float shift[16] = {
            scale, 0, 0, x,
            0, scale, 0, y,
            0, 0, 1, 0,
            0, 0, 0, 1
        };

        float view[16] = {
            1 / aspect_ratio, 0, 0, 0,
            0,                1, 0, 0,
            0,                0, 1, 0,
            0,                0, 0, 1
        };

        glUniformMatrix4fv(transform_loc, 1, GL_TRUE, transform);
        glUniformMatrix4fv(view_loc, 1, GL_TRUE, view);
        glUniformMatrix4fv(shift_loc, 1, GL_TRUE, shift);

        glClear(GL_COLOR_BUFFER_BIT);
        glDrawArrays(GL_TRIANGLES, 0, 18);

        SDL_GL_SwapWindow(window);
    }

    SDL_GL_DeleteContext(gl_context);
    SDL_DestroyWindow(window);
}
catch (std::exception const & e)
{
    std::cerr << e.what() << std::endl;
    return EXIT_FAILURE;
}

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

#include "series_n_units.hpp"

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
    throw std::runtime_error(to_string(message) +
        reinterpret_cast<const char *>(glewGetErrorString(error)));
}

int main() try
{
    if (SDL_Init(SDL_INIT_VIDEO) != 0)
        sdl2_fail("SDL_Init: ");

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 1);
    SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, 4);

    SDL_Window * window = SDL_CreateWindow("Graphics course practice 3",
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

    SDL_GL_SetSwapInterval(0);

    if (auto result = glewInit(); result != GLEW_NO_ERROR)
        glew_fail("glewInit: ", result);

    if (!GLEW_VERSION_3_3)
        throw std::runtime_error("OpenGL 3.3 is not supported");

    glClearColor(0.8f, 0.8f, 1.f, 0.f);

    auto last_frame_start = std::chrono::high_resolution_clock::now();

    float time = 0.f;

    bool running = true;
    series *obj = new canvas(std::shared_ptr<function>(new metaballs({
        // metaball(std::shared_ptr<traectory>(new circle(20, 100, 20, 0, 1, 1)), 100, 2, 1) // debug ball
        metaball(std::shared_ptr<traectory>(new circle(50, 600, 500, 0, 1, 2)), 70, 2, 1),
        metaball(std::shared_ptr<traectory>(new circle(125, 700, 600, PI / 2, -1, 1.5)), 95, 0.5, -1),
        metaball(std::shared_ptr<traectory>(new circle(130, 800, 300, 5 * PI / 6, 1, 0.6)), 120, 4, 1),
        metaball(std::shared_ptr<traectory>(new circle(100, 1000, 300, 0, 1, 3)), 100, 3, 1),
        metaball(std::shared_ptr<traectory>(new parabola(1000, 400, 400, 400, PI / 6, 1.5)), 40, 2, 1),
        metaball(std::shared_ptr<traectory>(new circle(60, 1200, 500, 0, -1, 0.5)), 500, 5, 1),
        metaball(std::shared_ptr<traectory>(new segment(100, 100, 1700, 900, PI / 2, 2.5)), 150, 2, -1),
        metaball(std::shared_ptr<traectory>(new parabola(1300, 800, 700, -700, PI / 3, 0.75)), 140, 2, -1),
        metaball(std::shared_ptr<traectory>(new circle(300, 550, 700, 0, -1, 1.5)), 200, 2.5, 1),
        metaball(std::shared_ptr<traectory>(new segment(100, 600, 1700, 300, PI, 2.2)), 300, 2, 1),
        metaball(std::shared_ptr<traectory>(new segment(1200, 200, 1200, 700, 0, 1.2)), 150, 3, -1),
    })));
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
                obj->resize(width, height);
                break;
            }
            break;
        case SDL_MOUSEBUTTONDOWN:
            if (event.button.button == SDL_BUTTON_LEFT)
            {
                int mouse_x = event.button.x;
                int mouse_y = event.button.y;
                obj->mouse_update(mouse_x, mouse_y);
            }
            else if (event.button.button == SDL_BUTTON_RIGHT)
            {
                obj->mouse_update();
            }
            break;
        case SDL_KEYDOWN:
            if (event.key.keysym.sym == SDLK_LEFT)
            {
                obj->key_update(-1);
            }
            else if (event.key.keysym.sym == SDLK_RIGHT)
            {
                obj->key_update(+1);
            }
            else if (event.key.keysym.sym == SDLK_DOWN) {
                obj->key_update(-1, true);
            }
            else if (event.key.keysym.sym == SDLK_UP) {
                obj->key_update(+1, true);
            }
            break;
        }

        if (!running)
            break;

        auto now = std::chrono::high_resolution_clock::now();
        float dt = std::chrono::duration_cast<std::chrono::duration<float>>(now - last_frame_start).count();
        last_frame_start = now;
        time += dt;

        series::timer(time);

        glClear(GL_COLOR_BUFFER_BIT);

        obj->draw();

        SDL_GL_SwapWindow(window);
    }
    delete obj;
    SDL_GL_DeleteContext(gl_context);
    SDL_DestroyWindow(window);
}
catch (std::exception const & e)
{
    std::cerr << e.what() << std::endl;
    return EXIT_FAILURE;
}

#include <math.h>

#if defined(_WIN32)
#include <Windows.h>
#include <gl/GL.h>
#else
#include <OpenGL/gl.h>
#endif
#include <api.h>

extern void render_next_gl(uint64_t time_micros)
{
        double const phase = 6.30 * time_micros / 1e6 / 6.0;
        float sincos[2] = {
                0.49f * sinf(phase),
                0.49f * cosf(phase),
        };
        float const argb[4] = {
                0.0f, 0.31f + 0.09f * sincos[0], 0.27f + 0.09f * sincos[1], 0.29f
        };
        glClearColor (argb[1], argb[2], argb[3], argb[0]);
        glClear (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

int main (int argc, char** argv)
{
        (void) argc;
        (void) argv;

        runtime_init();

        return 0;
}

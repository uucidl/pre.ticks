#include <micros/api.h>
#include <micros/gl3.h>

// Movie Playing
// ------------
//
// Our objective is to play small movie loops in realtime. The loops
// must be converted into series of frames directly exploitable using
// OpenGL, for mapping and general post-processing purpose.
//
// Once used, a frame may be discarded.
//
// We ideally want to align to a timeline.
//

extern
void render_next_gl3(uint64_t now_micros, Display)
{
        glClearColor(0.14f, 0.15f, 0.134f, 0.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        // TODO(nil): prototype API here
}

extern
void render_next_2chn_48khz_audio(uint64_t now_micros, int, double*, double*)
{
        // TODO(nil): prototype API here
}

extern int
main (int argc, char** argv)
{
        runtime_init();
        return 0;
}

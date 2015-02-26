#include "../compile.hpp"

#include <micros/api.h>

#include <GL/glew.h>

#include <cmath>
#include <cstdint>

// aka 2*PI
#define TAU (6.2831853071795864769252867665590057683943387987502116419498891846156328125724179972560696506842341359)

static float float32Square(float x)
{
        return x*x;
};

BEGIN_NAMELESS_STRUCT_DEF_BLOCK
struct Float32Vector3 {
        union {
                float values[3];
                struct {
                        float x, y, z;
                };
                struct {
                        float r, g, b;
                };
        };
};
END_NAMELESS_STRUCT_DEF_BLOCK

static inline struct Float32Vector3 V3(float x, float y, float z)
{
        return Float32Vector3 { { { x, y, z } } };
}

static inline struct Float32Vector3 operator * (float scalar,
                struct Float32Vector3 v)
{
        return V3(scalar * v.x, scalar * v.y, scalar * v.z);
}

void render_next_gl3(uint64_t micros, struct Display)
{
        static auto origin = micros;

        double const seconds = (micros - origin) / 1e6;
        {
                auto modulation = 1.0f + 0.25f*float32Square(sinf(TAU*seconds / 8.0f));
                auto backgroundColor = modulation * V3(0.16f, 0.23f, 0.38f);
                glClearColor(backgroundColor.r, backgroundColor.g, backgroundColor.b, 0.0f);
                glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        }

        // NOTE(nicolas) we want to describe the camera. It is a solid with a position
        // and some free axes. Every camera needs not to follow the same description:
        //
        // We can have 3, 2, 1 axes camera
        //
        // For each of these axes there are tracking forces at
        // play. These represent the action of the camera operator on
        // the camera to maintain it in one direction.
        //
        // These tracking forces are the ones who are subject to _noise_
        //
        // The noise itself has to be modeled somehow. For instance
        // the noise does not need to be aligned with the tracking
        // force itself. i.e. there are also certain free axes for the
        // noise forces.
}

void render_next_2chn_48khz_audio(uint64_t, int, double*, double*)
{}

int main (int argc, char **argv)
{
        runtime_init();
        return 0;
}

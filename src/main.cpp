#include <math.h>
#include <stdio.h> // for printf

#include <GL/glew.h>

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wmissing-field-initializers"

#include "stb_image.c"
#include "stb_image_write.h"
#include "stb_perlin.h"
#include "stb_truetype.h"
#include "ujdecode.h"

#pragma clang diagnostic pop

#include <micros/api.h>

#include <string>
using std::string;

#include "shader_types.h"

static int get_beat(uint64_t micros)
{
        return (int) (micros / (uint64_t) 1e6);
}

extern void render_next_2chn_48khz_audio(uint64_t time_micros,
                int const sample_count, double left[/*sample_count*/],
                double right[/*sample_count*/])
{
        static double sc_phase = 0.0;
        static double l_phase = 0.0;
        static double r_phase = 0.0;

        int const beat = get_beat(time_micros);

        double sequence_hz[] = {
                110.0,
                440.0,
                110.0 * (1 + 1/sqrt(2.0)),
                440.0 * (1 + 1/sqrt(2.0)),
        };

        double const pert = cos(6.30 * time_micros / 1e6 / 11.0) + sin(
                                    6.30 * time_micros / 1e6 / 37.0);
        double const drone_hz = sequence_hz[beat % (sizeof sequence_hz / sizeof
                                            sequence_hz[0])] + 20.0 * pert*pert;

        for (int i = 0; i < sample_count; i++) {
                double sincos[2] = {
                        0.49 * sin(sc_phase) * pert,
                        0.49 * cos(sc_phase) * pert
                };

                left[i] = 0.5 * (sin(l_phase) +
                                 0.25 * (sin(2.0 * l_phase) +
                                         0.25 * pert * sin(4.0 * r_phase)));
                right[i] = 0.5 * (sin(r_phase) +
                                  0.25 * (sin(2.0 * r_phase) +
                                          0.25 * pert * sin(4.0 * l_phase)));

                double rm = left[i] * right[i];

                left[i] += 0.35 * pert * pert * rm;
                right[i] += 0.35 * pert * pert * rm;

                left[i] *= 0.3;
                right[i] *= 0.3;

                l_phase += (1.0f + 0.12f * sincos[0]) * drone_hz * 6.30 / 48000.0;
                r_phase += (1.0f + 0.12f * sincos[1]) * drone_hz * 6.30 / 48000.0;
                sc_phase += 6.30 / 48000.0 / 7.0;
        }
}

static void test_json()
{
        UJObject obj;
        void *state;
        const char input[] =
                "{\"name\": \"John Doe\", \"age\": 31, \"number\": 1337.37, \"negative\": -9223372036854775808, \"address\": { \"city\": \"Uppsala\", \"population\": 9223372036854775807 } }";
        size_t cbInput = sizeof(input) - 1;

        const wchar_t *personKeys[] = { L"name", L"age", L"number", L"negative", L"address"};
        UJObject oName, oAge, oNumber, oNegative, oAddress;

        obj = UJDecode(input, cbInput, NULL, &state);

        if (obj &&
            UJObjectUnpack
            (obj, 5, "SNNNO",
             personKeys, &oName, &oAge, &oNumber, &oNegative, &oAddress) == 5) {
                const wchar_t *addressKeys[] = { L"city", L"population" };
                UJObject oCity, oPopulation;

                const wchar_t *name = UJReadString(oName, NULL);
                int age = UJNumericInt(oAge);
                double number = UJNumericFloat(oNumber);
                long long negative = UJNumericLongLong(oNegative);

                if (UJObjectUnpack(oAddress, 2, "SN", addressKeys, &oCity,
                                   &oPopulation) == 2) {
                        const wchar_t *city;
                        long long population;
                        city = UJReadString(oCity, NULL);
                        population = UJNumericLongLong(oPopulation);
                }
                printf("name: %ls, age: %d, number: %f, negative: %lld\n", name, age, number,
                       negative);
        } else {
                printf("could not parse JSON stream\n");
        }

        UJFree(state);
}

string const VERTEX_SHADER =
        "#version 150\n"
        "void main() {\n"
        "}";
string const FRAGMENT_SHADER =
        "#version 150\n"
        "void main() {\n"
        "}";

extern void render_next_gl(uint64_t time_micros)
{
        static class DoOnce
        {
        public:
                DoOnce()
                {
                        printf("OpenGL version %s\n", glGetString(GL_VERSION));
                        test_json();

                        shaders = ShaderProgram::create(VERTEX_SHADER, FRAGMENT_SHADER);
                }

                ShaderProgram shaders;
        } init;

        typedef struct Rgb {
                float x;
                float y;
                float z;

                Rgb(float x, float y, float z) :
                        x(x), y(y), z(z) {}

                Rgb operator * (double f)
                {
                        return Rgb((float) f*x, (float) f*y, (float) f*z);
                }
        } rgb;

        rgb sequence_rgb[] = {
                rgb(0.31f, 0.27f, 0.29f),
                rgb(0.62f, 0.54f, 0.58f),
                rgb(
                        0.31f, 0.27f, 0.29f
                ) * (1 + 1/sqrtf(2.0)),
                rgb(
                        0.62f, 0.54f, 0.58f
                ) * (1 + 1/sqrtf(2.0)),
        };

        int const beat = get_beat(time_micros);
        rgb current_rgb = sequence_rgb[beat % (sizeof sequence_rgb / sizeof
                                               sequence_rgb[0])];

        double const phase = 6.30 * time_micros / 1e6 / 11.0;
        float sincos[2] = {
                static_cast<float>(0.49 * sin(phase)),
                static_cast<float>(0.49 * cos(phase)),
        };
        float const argb[4] = {
                0.0f, current_rgb.x + 0.39f * sincos[0], current_rgb.y + 0.39f * sincos[1], current_rgb.z,
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

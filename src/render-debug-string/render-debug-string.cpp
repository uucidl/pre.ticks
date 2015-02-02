#include "../compile.hpp"
#include <micros/api.h>

#include <GL/glew.h>

BEGIN_NOWARN_BLOCK
#include "../../modules/stb/stb_easy_font.h"
END_NOWARN_BLOCK

#include <cassert>
#include <memory>
#include <vector>

/*
  Draw a message at a pixel position.

  @param scalePower 0,1,2 ... 0 shows the original font, 1 doubles it etc...
*/
static void draw_debug_string(float pixelX, float pixelY, char const* message,
                              int scalePower)
{
        struct StbEasyFontVertex {
                float x;
                float y;
                float unused_z;
                uint8_t unused_color[4];
        };

        static struct Resources {
                GLuint shaders[2] = {};
                GLuint shaderProgram = 0;
                GLuint quadBuffers[2] = {};
                GLuint quadVertexArray = 0;

                // dynamic data
                std::unique_ptr<struct StbEasyFontVertex, void (*)(void*)>
                stbEasyFontVertexBuffer = { nullptr, nullptr };
                size_t stbEasyFontVertexBufferSize = 0;
        } all;

        glGetError(); // do not let error spill over from previous frame

        static bool mustInit = true;
        if (mustInit) {
                mustInit = false;

                auto const MAX_CHAR_N = 64;
                auto const MAX_QUAD_N = 270 * MAX_CHAR_N;

                all.stbEasyFontVertexBufferSize = 4*MAX_QUAD_N*(sizeof
                                                  *all.stbEasyFontVertexBuffer);
                all.stbEasyFontVertexBuffer = { (struct StbEasyFontVertex*) malloc(
                                                        all.stbEasyFontVertexBufferSize), free
                                              };

                // DATA -> GPU

                GLuint baseQuadIndices[] = {
                        0, 1, 2, 2, 3, 0,
                };

                auto const stbVertexIndicesSize = 6*MAX_QUAD_N;
                GLuint* stbVertexIndices = new GLuint[stbVertexIndicesSize];
                for (size_t i = 0; i < MAX_QUAD_N; i++) {
                        auto base = 4*i;
                        for (size_t ii = 0; ii < sizeof baseQuadIndices / sizeof *baseQuadIndices;
                             ii++) {
                                stbVertexIndices[6*i + ii] = base + baseQuadIndices[ii];
                        }
                }

                char const* vertexShaderStrings[] = {
                        "#version 150\n",
                        "uniform vec3 iResolution;\n",
                        "uniform int iFontPixelSize;\n",
                        "in vec2 position;\n",
                        "void main()\n",
                        "{\n",
                        "    vec2 pixelEdge = position;\n",
                        "    vec2 pixel00 = vec2(-1.0, 1.0);\n",
                        "    vec2 pixelEdgeToVertexPosition = vec2(2.0, -2.0)/iResolution.xy;",
                        "    float scale = iFontPixelSize == 0.0 ? 1.0 : iFontPixelSize / 7.0;\n",
                        "    gl_Position = vec4(pixel00 + scale*pixelEdgeToVertexPosition * pixelEdge, 0.0, 1.0);\n",
                        "}\n",
                        nullptr,
                };
                char const* vertexShaderSource = __FILE__;

                char const* fragmentShaderStrings[] = {
                        "#version 150\n"
                        "out vec4 oColor;\n"
                        "void main()\n",
                        "{\n",
                        "    oColor = vec4(1.0, 1.0, 1.0, 1.0);\n"
                        "}\n",
                        nullptr
                };
                char const* fragmentShaderSource = __FILE__;

                auto countStrings = [](char const* lineArray[]) -> GLint {
                        auto count = 0;
                        while (*lineArray++)
                        {
                                count++;
                        }
                        return count;
                };

                struct ShaderDef {
                        GLenum type;
                        char const** lines;
                        GLint lineCount;
                        char const* source;
                } shaderDefs[2] = {
                        { GL_VERTEX_SHADER, vertexShaderStrings, countStrings(vertexShaderStrings), vertexShaderSource },
                        { GL_FRAGMENT_SHADER, fragmentShaderStrings, countStrings(fragmentShaderStrings), fragmentShaderSource },
                };
                {
                        auto i = 0;
                        all.shaderProgram  = glCreateProgram();

                        for (auto def : shaderDefs) {
                                GLuint shader = glCreateShader(def.type);
                                glShaderSource(shader, def.lineCount, def.lines, NULL);
                                glCompileShader(shader);
                                GLint status;
                                glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
                                if (status == GL_FALSE) {
                                        GLint length;
                                        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &length);
                                        auto output = std::vector<char> {};
                                        output.reserve(length + 1);
                                        glGetShaderInfoLog(shader, length, &length, &output.front());
                                        fprintf(stderr, "error:%s:0:%s while compiling shader #%d\n", def.source,
                                                &output.front(), 1+i);
                                }
                                glAttachShader(all.shaderProgram, shader);
                                all.shaders[i++] = shader;
                        }
                        glLinkProgram(all.shaderProgram);
                        {
                                auto program = all.shaderProgram;
                                GLint status;
                                glGetProgramiv(program, GL_LINK_STATUS, &status);
                                if (status == GL_FALSE) {
                                        GLint length;
                                        glGetProgramiv(program, GL_INFO_LOG_LENGTH, &length);
                                        auto output = std::vector<char> {};
                                        output.reserve(length + 1);
                                        glGetProgramInfoLog(program, length, &length, &output.front());
                                        fprintf(stderr, "error: %s while compiling program #1\n", &output.front());
                                }
                        }

                }

                struct BufferDef {
                        GLenum target;
                        GLenum usage;
                        GLvoid const* data;
                        GLsizeiptr count;
                        GLsizeiptr elementSize;
                        GLint componentCount;
                        GLint shaderAttrib;
                } bufferDefs[] = {
                        { GL_ELEMENT_ARRAY_BUFFER, GL_STATIC_DRAW, stbVertexIndices, stbVertexIndicesSize, sizeof *stbVertexIndices, 0, 0 },
                        { GL_ARRAY_BUFFER, GL_DYNAMIC_DRAW, NULL, 0, sizeof *all.stbEasyFontVertexBuffer, 2, glGetAttribLocation(all.shaderProgram, "position") },
                };

                glGenBuffers(sizeof bufferDefs / sizeof bufferDefs[0],
                             all.quadBuffers);
                {
                        auto i = 0;
                        for (auto def : bufferDefs) {
                                auto id = all.quadBuffers[i++];
                                glBindBuffer(def.target, id);
                                glBufferData(def.target, def.count * def.elementSize, def.data, def.usage);
                                glBindBuffer(def.target, 0);
                        }
                }

                glGenVertexArrays(1, &all.quadVertexArray);
                glBindVertexArray(all.quadVertexArray);
                {
                        auto i = 0;
                        for (auto def : bufferDefs) {
                                auto id = all.quadBuffers[i++];
                                glBindBuffer(def.target, id);

                                if (def.target != GL_ARRAY_BUFFER) {
                                        continue;
                                }
                                glEnableVertexAttribArray(def.shaderAttrib);
                                glVertexAttribPointer(def.shaderAttrib, def.componentCount, GL_FLOAT,
                                                      GL_FALSE, def.elementSize, 0);
                        }
                }
                glBindVertexArray(0);
                delete[] stbVertexIndices;
                assert(GL_NO_ERROR == glGetError());
        }

        // DYNAMIC DATA -> GPU
        int indicesCount;
        {
                auto glBufferId = all.quadBuffers[1];
                auto vertexBuffer = all.stbEasyFontVertexBuffer.get();
                auto vertexBufferSize = all.stbEasyFontVertexBufferSize;

                auto quadCount = stb_easy_font_print(pixelX, pixelY,
                                                     const_cast<char*>(message),
                                                     NULL, vertexBuffer, vertexBufferSize);

                glBindBuffer(GL_ARRAY_BUFFER, glBufferId);
                glBufferData(GL_ARRAY_BUFFER,
                             4*quadCount * (sizeof *vertexBuffer),
                             vertexBuffer, GL_DYNAMIC_DRAW);
                glBindBuffer(GL_ARRAY_BUFFER, 0);

                indicesCount = 6*quadCount;
        }
        // Drawing code

        glUseProgram(all.shaderProgram);
        {
                GLint viewport[4];
                glGetIntegerv(GL_VIEWPORT, viewport);
                GLfloat resolution[] = {
                        static_cast<GLfloat> (viewport[2]),
                        static_cast<GLfloat> (viewport[3]),
                        0.0,
                };
                glUniform3fv(glGetUniformLocation(all.shaderProgram, "iResolution"), 1,
                             resolution);
                glUniform1i(glGetUniformLocation(all.shaderProgram, "iFontPixelSize"),
                            (7 << scalePower));
        }

        glBindVertexArray(all.quadVertexArray);
        glDrawElements(GL_TRIANGLES, indicesCount, GL_UNSIGNED_INT, 0);
        glBindVertexArray(0);
        glUseProgram(0);
}

extern void render_next_gl3(uint64_t time_micros)
{
        float const argb[4] = {
                0.00f, 0.49f, 0.39f, 0.12f,
        };
        glClearColor (argb[1], argb[2], argb[3], argb[0]);
        glClear (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        static uint64_t firstFrameMicros = time_micros;

        char const *someLines[] = {
                "twinkle, twinkle little star",
                "don't tell me you've gone too far",
                "I miss you and Johann Sfar",
                "might take another dip in tar!"
        };

        auto seconds = (int) ((time_micros - firstFrameMicros) / 2.0 / 1e6);

        auto indexOfLineToShow = (seconds) % (sizeof someLines /
                                              sizeof *someLines);

        draw_debug_string(0.0f, 0.0f, someLines[indexOfLineToShow], 2);
}

extern void render_next_2chn_48khz_audio(uint64_t time_micros,
                int const sample_count, double left[/*sample_count*/],
                double right[/*sample_count*/])
{
        // silence
}

int main (int argc, char** argv)
{
        runtime_init();

        return 0;
}

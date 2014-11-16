#include <GL/glew.h>
#include <micros/api.h>

#include "../common/main_types.h"
#include "../common/shader_types.h"
#include "../common/buffer_types.h"
#include "../common/fragops_types.h"

#include <fstream>
#include <future>
#include <sstream>
#include <string>
#include <vector>
#include <math.h>
#include <stdio.h> // for printf

std::string dirname(std::string path)
{
        return path.substr(0, path.find_last_of("/\\"));
}

extern void render_next_gl3(uint64_t time_micros)
{
        float const argb[4] = {
                0.0f, 0.39f, 0.19f, 0.29f,
        };
        glClearColor (argb[1], argb[2], argb[3], argb[0]);
        glClear (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        static struct Resources {
                GLuint shaders[2]      = {};
                GLuint shaderProgram   = 0;
                GLuint quadBuffers[2]  = {};
                GLuint quadVertexArray = 0;
                GLint indicesCount = 0;
        } all;

        static bool mustInit = true;
        if (mustInit) {
                mustInit = false;

                char const* vertexShaderLines[] = {
                        "#version 150\n",
                        "in vec4 position;\n",
                        "void main()\n",
                        "{\n",
                        "gl_Position = position;\n",
                        "}\n",
                        NULL
                };
                char const* fragmentShaderLines[] = {
                        "#version 150\n",
                        "out vec4 color;\n",
                        "void main()\n",
                        "{\n",
                        "    float g = gl_FragCoord.y/64.0 * (1.0f + 0.2 * sin(gl_FragCoord.x / 64.0));\n",
                        "    color = vec4(1.0, 0.5 * g, 0.0, 0.90);\n",
                        "}\n",
                        NULL
                };

                auto countLines = [](char const* lineArray[]) -> GLint {
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
                } shaderDefs[2] = {
                        { GL_VERTEX_SHADER, vertexShaderLines, countLines(vertexShaderLines) },
                        { GL_FRAGMENT_SHADER, fragmentShaderLines, countLines(fragmentShaderLines) },
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
                                        fprintf(stderr, "ERROR compiling shader: %s\n", &output.front());
                                }
                                glAttachShader(all.shaderProgram, shader);
                                all.shaders[i++] = shader;
                        }
                        glLinkProgram(all.shaderProgram);
                }

                GLuint quadIndices[] = {
                        0, 1, 2, 2, 3, 0,
                };
                GLfloat quadVertices[] = {
                        -1.0, -1.0,
                        -1.0, +1.0,
                        +1.0, +1.0,
                        +1.0, -1.0,
                };

                struct BufferDef {
                        GLenum target;
                        GLenum usage;
                        GLvoid const* data;
                        GLsizeiptr size;
                        GLint componentCount;
                        GLint shaderAttrib;
                } bufferDefs[] = {
                        { GL_ELEMENT_ARRAY_BUFFER, GL_STATIC_DRAW, quadIndices, sizeof quadIndices, 0, 0 },
                        { GL_ARRAY_BUFFER, GL_STATIC_DRAW, quadVertices, sizeof quadVertices, 2, glGetAttribLocation(all.shaderProgram, "position") },
                };

                glGenBuffers(sizeof bufferDefs / sizeof bufferDefs[0],
                             all.quadBuffers);
                {
                        auto i = 0;
                        for (auto def : bufferDefs) {
                                auto id = all.quadBuffers[i++];
                                glBindBuffer(def.target, id);
                                glBufferData(def.target, def.size, def.data, def.usage);
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
                                glVertexAttribPointer(def.shaderAttrib, def.componentCount, GL_FLOAT,
                                                      GL_FALSE, 0, 0);
                                glEnableVertexAttribArray(def.shaderAttrib);
                        }
                }
                glBindVertexArray(0);
                all.indicesCount = sizeof quadIndices / sizeof quadIndices[0];
        }

        glUseProgram(all.shaderProgram);
        glBindVertexArray(all.quadVertexArray);
        glDrawElements(GL_TRIANGLES, all.indicesCount, GL_UNSIGNED_INT, 0);
        glBindVertexArray(0);
        glUseProgram(0);

}

extern void render_next_2chn_48khz_audio(uint64_t time_micros,
                int const sample_count, double left[/*sample_count*/],
                double right[/*sample_count*/])
{
        // silence
}

int main (int argc, char** argv)
{
        (void) argc;
        (void) argv;

        runtime_init();

        return 0;
}

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
        static class DoOnce : public DisplayThreadTasks, public FileSystem
        {
        public:
                void add_task(std::function<bool()>&& task)
                {
                        std::lock_guard<std::mutex> lock(tasks_mtx);
                        tasks.emplace_back(task);
                }


                DoOnce() : base_path(dirname(__FILE__)), shader_loader(*this,
                                        *this)
                {
                        shader_loader.load_shader("main.vs", "main.fs", [=](ShaderProgram&& input) {
                                shader = std::move(input);
                                position_attr = glGetAttribLocation(shader.ref(), "position");
                        });
                        float vertices[] = {
                                -1.0f, -1.0f,
                                -1.0f, +1.0f,
                                +1.0f, +1.0f,
                                +1.0f, -1.0f,
                        };

                        GLuint indices[] = {
                                0, 1, 2, 2, 3, 0
                        };

                        {
                                WithArrayBufferScope scope(vbo_vertices);
                                glBufferData(GL_ARRAY_BUFFER, sizeof vertices, vertices, GL_STREAM_DRAW);
                        }

                        {
                                WithElementArrayBufferScope scope(vbo_indices);
                                glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof indices, indices,
                                             GL_STREAM_DRAW);
                        }

                        printf("defining vertex array %d\n", vao_quad.ref);
                        {
                                WithVertexArrayScope vascope(vao_quad);

                                glEnableVertexAttribArray(position_attr);

                                glBindBuffer(GL_ARRAY_BUFFER, vbo_vertices.ref);
                                glVertexAttribPointer(position_attr, 2, GL_FLOAT, GL_FALSE, 0, 0);

                                glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, vbo_indices.ref);
                        }
                }

                std::ifstream open_file(std::string relpath) const
                {
                        auto stream = std::ifstream(base_path + "/" + relpath);

                        if (stream.fail()) {
                                throw std::runtime_error("could not load file at " + relpath);
                        }

                        return stream;
                }

                void run()
                {
                        std::lock_guard<std::mutex> lock(tasks_mtx);
                        for (auto& task : tasks) {
                                std::future<bool> future = task.get_future();
                                task();
                                future.get();
                        }
                        tasks.clear();
                }

                std::string base_path;
                ShaderProgram shader;
                Buffer vbo_vertices;
                Buffer vbo_indices;
                VertexArray vao_quad;
                GLuint position_attr;

                std::mutex tasks_mtx;
                std::vector<std::packaged_task<bool()>> tasks;
                ShaderLoader shader_loader;
        } resources;

        resources.run();

        float const argb[4] = {
                0.0f, 0.39f, 0.19f, 0.29f,
        };
        glClearColor (argb[1], argb[2], argb[3], argb[0]);
        glClear (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        {
                WithVertexArrayScope vascope(resources.vao_quad);
                WithBlendEnabledScope blend(GL_SRC_COLOR, GL_DST_COLOR);
                WithShaderProgramScope with_shader(resources.shader);
                glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
                resources.shader.validate();
        }

        static struct ShaderResources {
                GLuint shaders[2]      = { 0, 0 };
                GLuint shaderProgram   = 0;
                GLuint quadBuffers[3]  = { 0, 0, 0, };
                GLuint quadVertexArray = 0;
        } all;

        static bool mustInit = true;
        if (mustInit) {
                mustInit = false;
                char const* vertexShaderLines[] = {
                        "#version 150",
                        "in vec4 position;",
                        "void main()",
                        "{",
                        "gl_Position = position;",
                        "}",
                        NULL
                };
                char const* fragmentShaderLines[] = {
                        "#version 150",
                        "out vec4 color;",
                        "void main()",
                        "{",
                        "    float g = gl_FragCoord.y/512.0 * (1.0f + 0.2 * sin(gl_FragCoord.x / 64.0));",
                        "    color = vec4(1.0, 0.5 * g, 0.0, 0.90);",
                        "}",
                        NULL
                };
                struct ShaderDef {
                        GLenum type;
                        char const** lines;
                } shaderDefs[2] = {
                        { GL_VERTEX_SHADER, vertexShaderLines },
                        { GL_FRAGMENT_SHADER, fragmentShaderLines },
                };
                auto countLines = [](char const* lineArray[]) {
                        size_t count = 0;
                        while (*lineArray++) {
                                count++;
                        }
                        return count;
                };
                auto i = 0;
                for (auto def : shaderDefs) {
                        GLuint shader = glCreateShader(def.type);
                        glShaderSource(shader, countLines(def.lines), def.lines, NULL);
                        glCompileShader(shader);
                        all.shaders[i++] = shader;
                }

                all.shaderProgram  = glCreateProgram();
                glGenBuffers(3, all.quadBuffers);
                glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, all.quadBuffers[0]);
                glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
                glBindBuffer(GL_ARRAY_BUFFER, all.quadBuffers[1]);
                glBindBuffer(GL_ARRAY_BUFFER, all.quadBuffers[2]);
                glBindBuffer(GL_ARRAY_BUFFER, 0);
                glGenVertexArrays(1, &all.quadVertexArray);
                glBindVertexArray(all.quadVertexArray);
                glBindVertexArray(0);
        }


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

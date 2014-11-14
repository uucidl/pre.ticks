#include <fstream>
#include <future>
#include <sstream>
#include <string>
#include <vector>

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

#include "../common/main_types.h"
#include "../common/shader_types.h"
#include "../common/buffer_types.h"
#include "../common/fragops_types.h"

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
                                0.0f, 0.0f,
                                0.0f, 1.0f,
                                1.0f, 1.0f,
                                1.0f, 0.0f,
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

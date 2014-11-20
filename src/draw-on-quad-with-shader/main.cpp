#include <micros/api.h>

#include <GL/glew.h>

#include <cstdio> // for printf, read/seek etc..
#include <string>
#include <vector>

static std::string gbl_PROG;

static void draw_shader_on_quad(uint64_t time_micros)
{
        static struct Resources {
                GLuint shaders[2]      = {};
                GLuint shaderProgram   = 0;
                GLuint quadBuffers[2]  = {};
                GLuint quadVertexArray = 0;
                GLint indicesCount = 0;
        } all;

        // this incoming section initializes the static resources
        // necessary for drawing. It is executed only once!
        //
        // read the drawing code (after it) before checking the
        // initialization code.

        static bool mustInit = true;
        if (mustInit) {
                mustInit = false;

                // DATA

                const char* fragmentShaderPath = "shader.fs";

                // we will load content of datafiles either next to executable
                // or at its original source location, whichever contains a file.
                //
                // this allows changing the source file easily without extra copying
                //
                const char* dataFileSources[] = {
                        __FILE__, gbl_PROG.c_str(),
                };

                char const* vertexShaderStrings[] = {
                        "#version 150\n",
                        "in vec4 position;\n",
                        "void main()\n",
                        "{\n",
                        "    gl_Position = position;\n",
                        "}\n",
                        nullptr,
                };

                GLuint quadIndices[] = {
                        0, 1, 2, 2, 3, 0,
                };
                GLfloat quadVertices[] = {
                        -1.0, -1.0,
                        -1.0, +1.0,
                        +1.0, +1.0,
                        +1.0, -1.0,
                };

                // DATA -> OpenGL

                auto file_content = [](std::string const& base_path,
                std::string const& relpath) {
                        auto file = std::fopen((base_path + "/" + relpath).c_str(), "rb");
                        if (!file) {
                                throw std::runtime_error("could not load file at " + relpath);
                        }

                        std::string content;
                        std::fseek(file, 0, SEEK_END);
                        content.reserve(std::ftell(file));
                        std::fseek(file, 0, SEEK_SET);

                        char buffer[64 * 1024];
                        long n;
                        while (n = std::fread(buffer, 1, sizeof buffer, file), n != 0) {
                                content.append(buffer, n);
                        }
                        std::fclose(file);

                        return content;
                };

                auto datafile_content = [dataFileSources,
                file_content](std::string const& relpath) {
                        auto dirname = [](std::string filepath) {
                                return filepath.substr(0, filepath.find_last_of("/\\"));
                        };

                        for (auto base : dataFileSources) {
                                try {
                                        return file_content(dirname(base), relpath);
                                } catch (...) {
                                        continue;
                                }
                        }
                        return std::string("");
                };

                std::string fragmentShaderContent = datafile_content(fragmentShaderPath);
                char const* fragmentShaderStrings[] = {
                        fragmentShaderContent.c_str(),
                        nullptr,
                };

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
                } shaderDefs[2] = {
                        { GL_VERTEX_SHADER, vertexShaderStrings, countStrings(vertexShaderStrings) },
                        { GL_FRAGMENT_SHADER, fragmentShaderStrings, countStrings(fragmentShaderStrings) },
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
                                        fprintf(stderr, "ERROR compiling shader #%d: %s\n", 1+i, &output.front());
                                }
                                glAttachShader(all.shaderProgram, shader);
                                all.shaders[i++] = shader;
                        }
                        glLinkProgram(all.shaderProgram);
                }

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
                                glEnableVertexAttribArray(def.shaderAttrib);
                                glVertexAttribPointer(def.shaderAttrib, def.componentCount, GL_FLOAT,
                                                      GL_FALSE, 0, 0);
                        }
                }
                glBindVertexArray(0);
                all.indicesCount = sizeof quadIndices / sizeof quadIndices[0];
        }

        // Drawing code

        float const argb[4] = {
                0.0f, 0.39f, 0.19f, 0.29f,
        };
        glClearColor (argb[1], argb[2], argb[3], argb[0]);
        glClear (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);


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

                auto globalTimeInSeconds = static_cast<GLfloat> ((double) time_micros / 1e6);
                glUniform1fv(glGetUniformLocation(all.shaderProgram, "iGlobalTime"), 1,
                             &globalTimeInSeconds);
        }
        glBindVertexArray(all.quadVertexArray);
        glDrawElements(GL_TRIANGLES, all.indicesCount, GL_UNSIGNED_INT, 0);
        glBindVertexArray(0);
        glUseProgram(0);
}

extern void render_next_gl3(uint64_t time_micros)
{
        draw_shader_on_quad(time_micros);
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
        gbl_PROG = argv[0];

        runtime_init();

        return 0;
}

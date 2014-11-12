#include <GL/glew.h>

#include <sstream>
#include <memory>
#include <vector>
#include <string>

using std::vector;
using std::string;
using std::stringstream;

#include "shader_types.h"
#include "main_types.h"

class Program
{
public:
        Program() : ref(glCreateProgram()) {}
        Program(GLuint ref) : ref(ref) {}
        Program(Program&& other) : ref(0)
        {
                *this = std::move(other);
        }

        Program& operator=(Program&& other)
        {
                if (ref != other.ref) {
                        Program old (std::move(*this));
                }
                ref = other.ref;
                other.ref = 0;
                return *this;
        }

        ~Program()
        {
                glDeleteProgram(ref);
                ref = 0;
        }


        GLuint ref;

        Program(Program const& other) = delete;
        Program& operator= (Program const& other) = delete;
};

class Shader
{
public:
        Shader(GLuint const type) : ref(glCreateShader(type))
        {
        }

        Shader(Shader&& other) : ref(0)
        {
                *this = std::move(other);
        }

        Shader& operator=(Shader&& other)
        {
                if (ref != other.ref) {
                        Shader old (std::move(*this));
                }
                ref = other.ref;
                other.ref = 0;
                return *this;
        }

        ~Shader()
        {
                glDeleteShader(ref);
                ref = 0;
        }

        GLuint ref;

        Shader(Shader const& other) = delete;
        Shader& operator= (Shader const& other) = delete;
};

class ShaderProgram::Impl
{
public:
        Impl() {}
        Impl(GLuint program_ref) : program(program_ref) {}
        Impl(ShaderProgram::Impl&& other) :
                program(std::move(other.program)),
                shaders(std::move(other.shaders)) {}
        Impl& operator=(ShaderProgram::Impl&& other)
        {
                shaders = std::move(other.shaders);
                program = std::move(other.program);
                return *this;
        }

        Program program;
        vector<Shader> shaders;

        Impl(ShaderProgram::Impl const& other) = delete;
        Impl& operator= (ShaderProgram::Impl const& other) = delete;
};

class Lines
{
public:
        Lines(string const& source)
        {
                stringstream ss {source};
                string item;
                while (getline(ss, item, '\n')) {
                        lines.push_back(item + "\n");
                }
                for (string const& str : lines) {
                        cstrs.emplace_back(str.c_str());
                }
        }

        vector<string const> lines;
        vector<char const*> cstrs;
};

class ShaderProgramBuilder
{
private:
        void compile(Shader const& shader, string const& source) const
        {
                Lines lines (source);
                glShaderSource(shader.ref, lines.cstrs.size(), &lines.cstrs.front(), NULL);
                glCompileShader(shader.ref);

                GLint status;
                glGetShaderiv (shader.ref, GL_COMPILE_STATUS, &status);
                if (status == GL_FALSE) {
                        GLint length;
                        glGetShaderiv (shader.ref, GL_INFO_LOG_LENGTH, &length);

                        vector<char> sinfo;
                        sinfo.reserve(length + 1);
                        glGetShaderInfoLog(shader.ref, length, &length, &sinfo.front());

                        printf ("ERROR compiling shader [%s] with source [\n", &sinfo.front());
                        printf ("%s", source.c_str());
                        printf ("]\n");
                }
        }

        void attach(GLint type, string const& source)
        {
                Shader shader (type);
                compile(shader, source);
                glAttachShader(content.program.ref, shader.ref);
                content.shaders.push_back(std::move(shader));
        }
public:
        ShaderProgramBuilder& addVertexShaderCode(string const& source)
        {
                attach(GL_VERTEX_SHADER, source);
                return *this;
        }
        ShaderProgramBuilder& addFragmentShaderCode(string const& source)
        {
                attach(GL_FRAGMENT_SHADER, source);
                return *this;
        }
        ShaderProgram::Impl&& link()
        {
                glLinkProgram(content.program.ref);
                return std::move(content);
        }

        ShaderProgram::Impl content;
};

ShaderProgram ShaderProgram::create(std::string const& vertex_shader_code,
                                    std::string const& fragment_shader_code)
{
        ShaderProgram shader_program;
        shader_program.impl = std::unique_ptr<ShaderProgram::Impl>
                              (new ShaderProgram::Impl
                               (ShaderProgramBuilder()
                                .addVertexShaderCode(vertex_shader_code)
                                .addFragmentShaderCode(fragment_shader_code).link()));
        return shader_program;
}

void ShaderProgram::validate() const
{
        glValidateProgram (impl->program.ref);
        GLint status;
        glGetProgramiv (impl->program.ref, GL_VALIDATE_STATUS, &status);
        if (status == GL_FALSE) {
                GLint length;
                glGetProgramiv (impl->program.ref, GL_INFO_LOG_LENGTH, &length);

                vector<char> pinfo;
                pinfo.reserve(length + 1);

                glGetProgramInfoLog (impl->program.ref, length, &length, &pinfo.front());

                printf ("ERROR: validating program [%s]\n", &pinfo.front());
        }
}

GLuint ShaderProgram::ref() const
{
        return impl->program.ref;
}

ShaderProgram::ShaderProgram() : impl(new ShaderProgram::Impl()) {}
ShaderProgram::~ShaderProgram() = default;
ShaderProgram::ShaderProgram(ShaderProgram&& other) :
        impl(std::move(other.impl)) {}
ShaderProgram& ShaderProgram::operator=(ShaderProgram&& other)
{
        impl = std::move(other.impl);
        return *this;
}

void ShaderLoader::load_shader(std::string vs_path,
                               std::string fs_path,
                               std::function<void(ShaderProgram&&)> bind_shader)
{
        auto future_shader = std::async(std::launch::async, [=]() {
                try {
                        auto vs = file_system.open_file(vs_path);
                        auto fs = file_system.open_file(fs_path);
                        std::string vs_content {
                                std::istreambuf_iterator<char>(vs),
                                std::istreambuf_iterator<char>()
                        };
                        std::string fs_content {
                                std::istreambuf_iterator<char>(fs),
                                std::istreambuf_iterator<char>()
                        };

                        display_tasks.add_task([=] () {
                                auto shader = ShaderProgram::create(vs_content, fs_content);
                                auto success = shader.ref() != 0;
                                bind_shader(std::move(shader));

                                return success;
                        });
                } catch (std::exception& e) {
                        // pass any exception to display thread so it can be treated
                        display_tasks.add_task([=] () -> bool {
                                throw e;
                        });
                }

        });

        std::lock_guard<std::mutex> lock(futures_mtx);
        futures.push_back(std::move(future_shader));
}

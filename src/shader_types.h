#include <memory>
#include <string>

class ShaderProgram
{
public:
        static ShaderProgram create(std::string const& vertex_shader_code,
                                    std::string const& fragment_shader_code);

        void validate() const;

        ShaderProgram();
        ~ShaderProgram();
        ShaderProgram(ShaderProgram&& other);
        ShaderProgram& operator=(ShaderProgram&& other);
        ShaderProgram(ShaderProgram& other) = delete;
        ShaderProgram& operator=(ShaderProgram& other) = delete;

        class Impl;
        std::unique_ptr<Impl> impl;
};

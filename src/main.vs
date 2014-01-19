#version 150

in vec4 position;

void main()
{
        gl_Position = position - vec4(0.05, 0.0, 0.0, 0.0);
}

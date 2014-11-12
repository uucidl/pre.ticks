#version 150

out vec4 color;

void main()
{
        float g = gl_FragCoord.y/512.0 * (1.0f + 0.2 * sin(gl_FragCoord.x / 64.0));
        color = vec4(1.0, 0.5 * g, 0.0, 0.90);
}


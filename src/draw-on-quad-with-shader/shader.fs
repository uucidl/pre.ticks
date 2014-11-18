#version 150

uniform vec3 iResolution; // viewport resolution in pixels
uniform float iGlobalTime; // shader playback time in seconds
out vec4 color;

void main()
{
        vec2 uv = gl_FragCoord.xy/iResolution.xy;
        float g = uv.y * (1.0f + 0.2 * sin(8.0*3.141592*(iGlobalTime/4.0 + uv.x)));
        color = vec4(uv.x, g, uv.y, 1.00);
}

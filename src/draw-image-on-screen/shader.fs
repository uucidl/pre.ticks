// -*- c -*-
#version 150

uniform vec3 iResolution; // viewport resolution in pixels
uniform float iGlobalTime; // shader playback time in seconds
uniform sampler2D iChannel0; // first texture
out vec4 oColor;

void main()
{
        vec2 photoResolution = textureSize(iChannel0, 0);
        // scale photo and position it to be at the center
        vec2 scales = vec2(iResolution.x/photoResolution.x,
                           iResolution.y/photoResolution.y);
        photoResolution = min(scales.x, scales.y) * photoResolution;
        vec2 translation;
        translation.y = max(0,(iResolution.y - photoResolution.y)/2.0);
        translation.x = max(0,(iResolution.x - photoResolution.x)/2.0);
        vec2 uv = (gl_FragCoord.xy + vec2(0.5,
                                          0.5) - translation) / photoResolution.xy;
        vec2 photouv = vec2(uv.x, 1.0-uv.y);
        oColor = texture(iChannel0, photouv);
}

// -*- c -*-

#version 150

// inputs & uniforms
uniform vec3 iResolution; // viewport resolution in pixels
uniform float iGlobalTime; // shader playback time in seconds
uniform sampler2D iChannel0; // first texture
uniform int iInterpolationMethod; // whether to interpolate or not

// outputs
out vec4 oColor;

// program
const bool mustScaleToFit = true;

const float TAU = 6.28318530717958647692528676655900576839433879875021;

const int BILINEAR_RESAMPLING = 1;
const int MITCHELL_NETRAVALLI_RESAMPLING = 2;
const int LANCZOS3_RESAMPLING = 3;

float mitchellNetravali(float x)
{
        float ax = abs(x);
        if (ax < 1) {
                return 7.0*ax*ax*ax
                       - 12*ax*ax
                       + 16.0/3.0;
        } else if (ax >= 1 && ax < 2) {
                return -7.0/3.0 * ax*ax*ax
                       + 12.0 * ax*ax
                       + -20.0 * ax
                       + 32.0/3.0;
        }

        return 0;
}

float lanczos3(float x)
{
        const float radius = 3.0;

        float ax = abs(x);
        if (x == 0.0) {
                return 1.0;
        }

        if (ax > radius) {
                return 0.0;
        }

        float pix = TAU * ax / 2.0;


        return sin(pix) * sin(pix / radius) / (pix * pix);
}

// MITCHELL - NETRAVALI
// -------------------
//
// General form:
//
// k(x) = 1/6 times
//    { (12 - 9B -6C)*abs(x)^3 + (-18 +12B + 6C)*abs(x)^2 + (6 - 2B) } if abs(x) < 1
//    { (-B-6C)abs(x)^3 + (6B+30C)abs(x)^2 + (-12B-48C)abs(x) + (8B+24C) } if abs(x) in [1..2(
//    { 0 } otherwise
//
// and with B = C = 1/3
//
// k(x) = 1/6 times
//    { 7*|x|^3 + -20*|x|^2 + 16/3} if |x| < 1
//    { -7/3*|x|^3 + 12*|x|^2 + -20*|x| + 32/3 | if |x| in [1..2[
//    { 0 } otherwise
//
vec4 sampleWithMitchellNetravali(sampler2D sampler, vec2 uv, vec2 stepxy)
{
        vec2 samplerSize = textureSize(sampler, 0);
        vec2 texel = 1.0 / samplerSize;

        vec2 texelPos = samplerSize * uv;
        vec2 bottomLeftTexelPos = floor(texelPos - vec2(0.5)) + vec2(0.5);

        vec4 xpos = vec4(
                            (bottomLeftTexelPos.x - 1.0) * texel.x,
                            (bottomLeftTexelPos.x + 0.0) * texel.x,
                            (bottomLeftTexelPos.x + 1.0) * texel.x,
                            (bottomLeftTexelPos.x + 2.0) * texel.x
                    );

        vec4 ypos = vec4(
                            (bottomLeftTexelPos.y - 1.0) * texel.y,
                            (bottomLeftTexelPos.y + 0.0) * texel.y,
                            (bottomLeftTexelPos.y + 1.0) * texel.y,
                            (bottomLeftTexelPos.y + 2.0) * texel.y
                    );

        vec2 f = texelPos - bottomLeftTexelPos;
        if (f.x >= 1.0 || f.y >= 1.0 || f.x < 0.0 || f.y < 0.0) {
                return vec4(1.0, 0.0, 0.0, 0.0);
        }

        vec2 speed = min(vec2(1.0), texel / stepxy);
        vec4 linetaps = vec4(mitchellNetravali(speed.x*(-1 - f.x)),
                             mitchellNetravali(speed.x*(0-f.x)),
                             mitchellNetravali(speed.x*(1-f.x)),
                             mitchellNetravali(speed.x*(2-f.x))
                            );
        linetaps /= dot(linetaps, vec4(1.0));
        vec4 columntaps = vec4(mitchellNetravali(speed.y*(-1 - f.y)),
                               mitchellNetravali(speed.y*(0-f.y)),
                               mitchellNetravali(speed.y*(1-f.y)),
                               mitchellNetravali(speed.y*(2-f.y))
                              );
        columntaps /= dot(columntaps, vec4(1.0));

        return columntaps.r * (texture(sampler, vec2(xpos.r, ypos.r)) * linetaps.r +
                               texture(sampler, vec2(xpos.g, ypos.r)) * linetaps.g +
                               texture(sampler, vec2(xpos.b, ypos.r)) * linetaps.b +
                               texture(sampler, vec2(xpos.a, ypos.r)) * linetaps.a) +
               columntaps.g * (texture(sampler, vec2(xpos.r, ypos.g)) * linetaps.r +
                               texture(sampler, vec2(xpos.g, ypos.g)) * linetaps.g +
                               texture(sampler, vec2(xpos.b, ypos.g)) * linetaps.b +
                               texture(sampler, vec2(xpos.a, ypos.g)) * linetaps.a) +
               columntaps.b * (texture(sampler, vec2(xpos.r, ypos.b)) * linetaps.r +
                               texture(sampler, vec2(xpos.g, ypos.b)) * linetaps.g +
                               texture(sampler, vec2(xpos.b, ypos.b)) * linetaps.b +
                               texture(sampler, vec2(xpos.a, ypos.b)) * linetaps.a) +
               columntaps.a * (texture(sampler, vec2(xpos.r, ypos.a)) * linetaps.r +
                               texture(sampler, vec2(xpos.g, ypos.a)) * linetaps.g +
                               texture(sampler, vec2(xpos.b, ypos.a)) * linetaps.b +
                               texture(sampler, vec2(xpos.a, ypos.a)) * linetaps.a)
               ;
}

// LANCZOS 3 INTERPOLATION
// ---------------------
//
vec4 sampleWithLanczos3Interpolation(sampler2D sampler, vec2 uv, vec2 stepxy)
{
        vec2 samplerSize = textureSize(sampler, 0);
        vec2 texel = 1.0 / samplerSize;
        vec2 texelPos = uv / texel;
        vec2 bottomLeftTexelPos = floor(texelPos - vec2(0.5)) + vec2(0.5);

        float xpos[6] = float[6](
                                (bottomLeftTexelPos.x - 2.0) * texel.x,
                                (bottomLeftTexelPos.x - 1.0) * texel.x,
                                (bottomLeftTexelPos.x + 0.0) * texel.x,
                                (bottomLeftTexelPos.x + 1.0) * texel.x,
                                (bottomLeftTexelPos.x + 2.0) * texel.x,
                                (bottomLeftTexelPos.x + 3.0) * texel.x
                        );

        float ypos[6] = float[6](
                                (bottomLeftTexelPos.y - 2.0) * texel.y,
                                (bottomLeftTexelPos.y - 1.0) * texel.y,
                                (bottomLeftTexelPos.y + 0.0) * texel.y,
                                (bottomLeftTexelPos.y + 1.0) * texel.y,
                                (bottomLeftTexelPos.y + 2.0) * texel.y,
                                (bottomLeftTexelPos.y + 3.0) * texel.y
                        );

        vec2 f = texelPos - bottomLeftTexelPos;
        vec2 speed = min(vec2(1.0), texel / stepxy);
        vec3 ltapsLeft = vec3(
                                 lanczos3(speed.x*(-2 - f.x)),
                                 lanczos3(speed.x*(-1 - f.x)),
                                 lanczos3(speed.x*(0 - f.x))
                         );
        vec3 ltapsRight = vec3(
                                  lanczos3(speed.x*(1 - f.x)),
                                  lanczos3(speed.x*(2 - f.x)),
                                  lanczos3(speed.x*(3 - f.x))
                          );
        float lsum = dot(ltapsLeft, vec3(1)) + dot(ltapsRight, vec3(1));

        ltapsRight /= lsum;
        ltapsLeft /= lsum;

        vec3 coltapsUp = vec3(
                                 lanczos3(speed.y*(-2 - f.y)),
                                 lanczos3(speed.y*(-1 - f.y)),
                                 lanczos3(speed.y*( 0 - f.y))
                         );
        vec3 coltapsDown = vec3(
                                   lanczos3(speed.y*(1 - f.y)),
                                   lanczos3(speed.y*(2 - f.y)),
                                   lanczos3(speed.y*(3 - f.y))
                           );
        float csum = dot(coltapsUp, vec3(1)) + dot(coltapsDown, vec3(1));

        coltapsUp /= csum;
        coltapsDown /= csum;

        vec4 color = vec4(0);
        for (int linei = 0; linei < 3; linei++) {
                float ctap = coltapsUp[linei];
                vec4 lineColor = vec4(0);
                for (int coli = 0; coli < 3; coli++) {
                        float ltap = ltapsLeft[coli];
                        lineColor += ltap * texture(sampler, vec2(xpos[coli], ypos[linei]));
                }
                for (int coli = 3; coli < 6; coli++) {
                        float ltap = ltapsRight[coli - 3];
                        lineColor += ltap * texture(sampler, vec2(xpos[coli], ypos[linei]));
                }
                color += ctap * lineColor;
        }
        for (int linei = 3; linei < 6; linei++) {
                float ctap = coltapsDown[linei-3];
                vec4 lineColor = vec4(0);
                for (int coli = 0; coli < 3; coli++) {
                        float ltap = ltapsLeft[coli];
                        lineColor += ltap * texture(sampler, vec2(xpos[coli], ypos[linei]));
                }
                for (int coli = 3; coli < 6; coli++) {
                        float ltap = ltapsRight[coli - 3];
                        lineColor += ltap * texture(sampler, vec2(xpos[coli], ypos[linei]));
                }
                color += ctap * lineColor;
        }

        return color;
}

// LINEAR INTERPOLATION
// -------------------
//
//
// This is done by hand here to demonstrate the algorithm and
// highlight the coordinate system used by OpenGL by default.
//
// You would normally just configure the texture to GL_LINEAR
// interpolation
//
vec4 sampleWithBilinearInterpolation(sampler2D sampler, vec2 uv)
{
        vec2 samplerSize = textureSize(sampler, 0);
        vec2 texel = 1.0 / samplerSize;
        vec2 texelPos = samplerSize * uv;

        // we get the position of the texel. Watch out that
        // texels start at the center of a position (hence the 0.5)
        vec2 bottomLeftTexelPos = floor(texelPos - vec2(0.5)) + vec2(0.5);

        vec4 bl = texture(sampler, (bottomLeftTexelPos + vec2(0.0, 0.0)) * texel);
        vec4 br = texture(sampler, (bottomLeftTexelPos + vec2(1.0, 0.0)) * texel);
        vec4 tl = texture(sampler, (bottomLeftTexelPos + vec2(0.0, 1.0)) * texel);
        vec4 tr = texture(sampler, (bottomLeftTexelPos + vec2(1.0, 1.0)) * texel);

        vec2 fractFromBottomLeftTexelPos = texelPos - bottomLeftTexelPos;
        if (fractFromBottomLeftTexelPos.x > 1.0) {
                return vec4(1.0, 0.0, 0.0, 0.0);
        }
        if (fractFromBottomLeftTexelPos.y > 1.0) {
                return vec4(1.0, 0.0, 0.0, 0.0);
        }

        vec4 tA = mix(bl, br, fractFromBottomLeftTexelPos.x);
        vec4 tB = mix(tl, tr, fractFromBottomLeftTexelPos.x);
        return mix(tA, tB, fractFromBottomLeftTexelPos.y);
}

// NEAREST NEIGHBOR LOOKUP
// ----------------------
//
// Normally you would just look up the nearest sample in the texture
//
// return texture(sampler, uv);
//
// We here show what OpenGL does internally
//
vec4 sampleWithNearestNeighbor(sampler2D sampler, vec2 uv)
{
        vec2 samplerSize = textureSize(sampler, 0);
        vec2 nearestTexelPos = round(samplerSize * uv - vec2(0.5)) + vec2(0.5);
        return texture(sampler, nearestTexelPos / samplerSize);
}

// draw a square between r0 and r1 in the given fillColor over the
// fragment at pixelPos and whose current color is fragmentColor
vec4 drawSquare(float r0, float r1, vec4 fillColor, vec2 pixelPos,
                vec4 fragmentColor)
{
        if (step(pixelPos, r0*vec2(1.0)) == vec2(0.0)
            && step(pixelPos, r1*vec2(1.0)) == vec2(1.0)) {
                return fillColor;
        }
        return fragmentColor;
}

void main()
{
        // NOTE(nicolas) we are assuming below that the input
        // photo/picture has a pixel aspect ratio equals to that of
        // our display.
        vec2 screenCenterFragCoord = vec2(iResolution.x/2.0,
                                          iResolution.y/2.0);
        vec2 fragCoordFromCenter = gl_FragCoord.xy - screenCenterFragCoord;

        vec2 iChannel0Size = textureSize(iChannel0, 0);
        vec2 uvPerFragCoord = 1.0 / iChannel0Size;

        int interpolationMethod = LANCZOS3_RESAMPLING;
        if (mustScaleToFit) {
                // scale photo so that at least one of its dimensions occupies the screen
                float xs = iChannel0Size.x / iResolution.x;
                float ys = iChannel0Size.y / iResolution.y;

                float speed = max(xs, ys);
                if (speed == 1.0) {
                        interpolationMethod = LANCZOS3_RESAMPLING;
                } else if (speed > 1.0) {
                        // use mitchell netravalli when downsampling, as it softens a bit more
                        interpolationMethod = MITCHELL_NETRAVALLI_RESAMPLING;
                } else if (speed < 1.0) {
                        interpolationMethod = LANCZOS3_RESAMPLING;
                }

                uvPerFragCoord = speed / iChannel0Size;
        }

        // compute uv so the photo is centered
        vec2 uvAtCenter = vec2(0.5, 0.5);
        vec2 uv = uvAtCenter + vec2(1.0, -1.0) * uvPerFragCoord * fragCoordFromCenter;

        if (interpolationMethod == MITCHELL_NETRAVALLI_RESAMPLING) {
                oColor = sampleWithMitchellNetravali(iChannel0, uv, uvPerFragCoord);
                oColor = drawSquare(8, 32, vec4(1.0, 0.4, 0.2, 0.0), gl_FragCoord.xy, oColor);
        } else if (interpolationMethod == LANCZOS3_RESAMPLING) {
                oColor = sampleWithLanczos3Interpolation(iChannel0, uv, uvPerFragCoord);
                oColor = drawSquare(8, 32, vec4(0.2, 0.7, 0.2, 0.0), gl_FragCoord.xy, oColor);
        } else if (interpolationMethod == BILINEAR_RESAMPLING) {
                oColor = sampleWithBilinearInterpolation(iChannel0, uv);
                oColor = drawSquare(8, 32, vec4(0.0, 0.8, 0.72, 0.0), gl_FragCoord.xy,
                                    oColor);
        } else {
                oColor = sampleWithNearestNeighbor(iChannel0, uv);
        }
}

#ifndef SHADERS_H
#define SHADERS_H

/* GLSL Shader Code */

char vShaderStr[] = R"shadercode(
uniform mat3 axisTransform;
uniform vec2 axisBase;
uniform bool tstrip;
attribute float time;
attribute float value;
attribute float rendertstrip;
varying float render;
void main()
{
    if (value < 0.0 || value == 0.0 || value > 0.0)
    {
        vec3 transformed = axisTransform * vec3(vec2(time, value) - axisBase, 1.0);
        gl_Position = vec4(transformed.xy, 0.0, 1.0);
        /* I want to read the sign bit of 'rendertstrip' and xor it with
         * tstrip. This isn't easy, because if rendertstrip is 0.0, then
         * it's difficult to distinguish if its +0.0 or -0.0. The solution
         * I came up with is to take infinity an divide it by rendertstrip.
         * The result will be positive infinity or negative infinity
         * depending on the sign of rendertstrip.
         */
        render = (tstrip ^^ (((1.0 / 0.0) / rendertstrip) < 0.0)) ? 1.0 : 0.0;
        gl_PointSize = 4.0;
    }
    else
    {
        render = 0.0;
    }
})shadercode";

char fShaderStr[] = R"shadercode(
uniform vec3 color;
uniform float opacity;
varying float render;
void main()
{
    if (render < 1.0)
    {
        discard;
    }
    else
    {
        gl_FragColor = vec4(color, opacity);
    }
})shadercode";


#endif // SHADERS_H

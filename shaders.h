#ifndef SHADERS_H
#define SHADERS_H

/* GLSL Shader Code */

char vShaderStr[] = R"shadercode(
uniform mat3 axisTransform;
uniform vec2 axisBase;
uniform float pointsize;
uniform bool tstrip;
attribute float time;
attribute float value;
attribute float rendertstrip;
varying float render;
void main()
{
    if (rendertstrip <= 0.25 || rendertstrip >= 0.75)
    {
        vec3 transformed = axisTransform * vec3(vec2(time, value) - axisBase, 1.0);
        gl_Position = vec4(transformed.xy, 0.0, 1.0);
        /* I want to read the sign bit of 'rendertstrip' and xor it with
         * tstrip.
         */
        render = (tstrip ^^ (rendertstrip < 0.0)) ? 1.0 : 0.0;
        gl_PointSize = pointsize;
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

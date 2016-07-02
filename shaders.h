#ifndef SHADERS_H
#define SHADERS_H

/* GLSL Shader Code */

char vShaderStr[] = R"shadercode(
uniform mat3 axisTransform;
uniform vec2 axisBase;
uniform float pointsize;
uniform bool tstrip;
uniform bool alwaysConnect;
attribute float time;
attribute float value;
attribute float flags;
varying float render;
void main()
{
    if ((alwaysConnect && (flags <= 0.6 || flags >= 0.9)) || flags <= 0.5 || flags >= 1.5)
    {
        vec3 transformed = axisTransform * vec3(vec2(time, value) - axisBase, 1.0);
        gl_Position = vec4(transformed.xy, 0.0, 1.0);
        /* If flags is FLAGS_LONEPT and we aren't joining all the points,
         * then I want to skip rendering the triangle strip (and line strip),
         * but draw the vertical line (and point).
         */
        render = (tstrip ^^ (!alwaysConnect && flags >= -1.5 && flags <= -0.5)) ? 1.0 : 0.0;
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

char ddvShaderStr[] = R"shadercode(
uniform mat3 axisTransform;
uniform vec2 axisBase;
attribute float time;
attribute float count;
void main()
{
    vec3 transformed = axisTransform * vec3(vec2(time, count) - axisBase, 1.0);
    gl_Position = vec4(transformed.xy, 0.0, 1.0);
}
)shadercode";

char ddfShaderStr[] = R"shadercode(
uniform vec3 color;
void main()
{
    gl_FragColor = vec4(color, 1.0);
}
)shadercode";


#endif // SHADERS_H

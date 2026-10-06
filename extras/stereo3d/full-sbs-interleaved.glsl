#pragma parameter RowParity "Row parity (toggle to match panel)" 0.0 0.0 1.0 1.0
#pragma parameter SwapEyes "Swap eyes" 0.0 0.0 1.0 1.0

#if defined(VERTEX)
#if __VERSION__ >= 130
#define ATTRIBUTE in
#define VARYING out
#else
#define ATTRIBUTE attribute
#define VARYING varying
#endif
ATTRIBUTE vec4 VertexCoord;
ATTRIBUTE vec2 TexCoord;
uniform mat4 MVPMatrix;
VARYING vec2 vTexCoord;
void main()
{
    gl_Position = MVPMatrix * VertexCoord;
    vTexCoord = TexCoord;
}
#elif defined(FRAGMENT)
#ifdef GL_ES
precision highp float;
#endif
#if __VERSION__ >= 130
#define VARYING in
#define SAMPLE texture
out vec4 FragColor;
#define COLOR FragColor
#else
#define VARYING varying
#define SAMPLE texture2D
#define COLOR gl_FragColor
#endif
VARYING vec2 vTexCoord;
uniform sampler2D Texture;
uniform vec2 InputSize;
uniform vec2 TextureSize;
#ifdef PARAMETER_UNIFORM
uniform float RowParity;
uniform float SwapEyes;
#else
#define RowParity 0.0
#define SwapEyes 0.0
#endif
void main()
{
    float eye = mod(floor(gl_FragCoord.y) + RowParity + SwapEyes, 2.0);
    vec2 pos = vTexCoord * TextureSize;
    float halfWidth = InputSize.x * 0.5;
    pos.x = clamp(pos.x * 0.5, 0.5, halfWidth - 0.5) + eye * halfWidth;
    pos.y = clamp(pos.y, 0.5, InputSize.y - 0.5);
    COLOR = SAMPLE(Texture, pos / TextureSize);
}
#endif

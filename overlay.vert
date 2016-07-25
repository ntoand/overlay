#version 400
in vec4 vertex; // <vec2 position, vec2 texCoords>
out vec2 TexCoords;

uniform mat4 transform;
uniform mat4 projection;

void main()
{
    TexCoords = vertex.zw;
    gl_Position = projection * transform * vec4(vertex.xy, 0.0, 1.0);
}
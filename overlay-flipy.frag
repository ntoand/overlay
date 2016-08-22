#version 400

uniform sampler2D image;

in vec2 TexCoords;
layout(location = 0) out vec4 color;

void main()
{    
    float y = 1 - TexCoords.y;
    color = texture(image, vec2(TexCoords.x, y));
}  
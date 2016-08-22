#version 400

uniform sampler2D image;
uniform float alpha;

in vec2 TexCoords;
layout(location = 0) out vec4 color;

void main()
{    
    color = texture(image, TexCoords);
    color.a *= alpha;
}  

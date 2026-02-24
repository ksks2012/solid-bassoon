#version 430 core

// Presents the compute-shader output texture on the fullscreen quad.

in vec2 vUV;
out vec4 fragColor;

uniform sampler2D u_texture;

void main() {
    fragColor = texture(u_texture, vUV);
}

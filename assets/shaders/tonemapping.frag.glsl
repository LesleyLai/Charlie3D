#version 460

layout (location = 0) out vec4 out_frag_color;

layout (set = 0, binding = 0) uniform sampler2D final_hdr_texture;

layout (location = 0) in vec2 v_uv;


vec3 aces_tone_mapping(vec3 x) {
    const float a = 2.51;
    const float b = 0.03;
    const float c = 2.43;
    const float d = 0.59;
    const float e = 0.14;
    return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0, 1.0);
}

vec3 reinhard_tone_mapping(vec3 radiance) {
    return radiance / (radiance + vec3(1.0));
}

vec3 tone_mapping(vec3 x) {
    // return reinhard_tone_mapping(x);
    return aces_tone_mapping(x);
}

void main() {
    vec3 color = texture(final_hdr_texture, v_uv).xyz;
    color = tone_mapping(color);
    out_frag_color = vec4(color, 1.0);
}
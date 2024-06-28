layout (set = 0, binding = 2) uniform sampler2D shadow_map;

const float sun_light_size = 20.0f;

#define SHADOW_SAMPLE_COUNT 16
const vec2 poisson_disk_16[SHADOW_SAMPLE_COUNT] = vec2[](
    vec2(-0.680088, -0.731923),
    vec2(-0.957909, -0.247622),
    vec2(0.0948045, -0.992508),
    vec2(-0.316418, -0.93561),
    vec2(0.508091, -0.270309),
    vec2(0.986855, -0.161122),
    vec2(-0.0783372, -0.377044),
    vec2(0.678299, -0.730012),
    vec2(0.264303, 0.150586),
    vec2(0.375585, 0.926198),
    vec2(0.869216, 0.485342),
    vec2(-0.0303609, 0.582547),
    vec2(-0.456659, 0.886469),
    vec2(-0.0502123, 0.998242),
    vec2(-0.522737, 0.0698312),
    vec2(-0.857891, 0.512805)
);


float shadow_map_proj(vec4 shadow_coord)
{
    if (0.0 < shadow_coord.z && shadow_coord.z < 1.0)
    {
        float z = texture(shadow_map, shadow_coord.xy).r;
        if (shadow_coord.w > 0.0 && z < shadow_coord.z)
        {
            return 0.0;
        }
    }
    return 1.0;
}


// Returns a uniform random number from [0, 1] base on a vec4
float random4(vec4 seed) {
    float dot_product = dot(seed, vec4(12.9898, 78.233, 45.164, 94.673));
    return fract(sin(dot_product) * 43758.5453);
}

mat2 rotation2d(float angle) {
    float s = sin(angle);
    float c = cos(angle);

    return mat2(
        c, -s,
        s, c
    );
}


// percentage closer filter
float shadow_PCF(vec4 shadow_coord, vec2 texel_size, float scale) {
    float visibility = 0.0;
    for (int i = 0; i < SHADOW_SAMPLE_COUNT; ++i) {
        int index = int(random4(vec4(shadow_coord.xyz, i)) * 16.0) % 16;
        float rotate = random4(vec4(shadow_coord.xyz, i)) * 3.1415926 * 2.0;

        vec2 offset = poisson_disk_16[i] * scale * texel_size;
        offset = rotation2d(rotate) * offset;
        visibility += shadow_map_proj(shadow_coord + vec4(offset, 0.0, 0.0));
    }
    return visibility / float(SHADOW_SAMPLE_COUNT);
}

// Calculate the average depth of occluders
// return false if no blocker
bool estimate_blocker_depth(vec2 uv, float z_receiver, out float z_blocker) {
    int blocker_count = 0;
    float blocker_depth_sum = 0.0;

    ivec2 tex_dim = textureSize(shadow_map, 0);
    vec2 delta = vec2(1.0) / tex_dim * 5.0;

    for (int i = 0; i < 4; i++) {
        int index = int(random4(vec4(uv, z_receiver, i)) * 16.0) % 16;
        vec2 offset = poisson_disk_16[index] * delta;
        float depth_on_shadow_map = texture(shadow_map, uv + offset).r;
        if (depth_on_shadow_map < z_receiver) {
            blocker_count++;
            blocker_depth_sum += depth_on_shadow_map;
        }
    }

    // No blockers
    if (blocker_count == 0)  return false;

    z_blocker = blocker_depth_sum / float(blocker_count);
    return true;
}

float shadow_PCSS(vec4 shadow_coord, vec2 texel_size, float light_size) {
    float z_receiver = shadow_coord.z;

    float z_blocker;
    bool has_blocker = estimate_blocker_depth(shadow_coord.xy, z_receiver, z_blocker);
    if (!has_blocker) { return 1.0; } // Fully visible if no blockers

    float penumbra_size = (z_receiver - z_blocker) * light_size / z_blocker;
    return shadow_PCF(shadow_coord, texel_size, penumbra_size);
}

// Returns a visibility between [0, 1]
float shadow_mapping(vec4 shadow_coord, uint shadow_mode) {
    ivec2 shadow_map_size = textureSize(shadow_map, 0);
    vec2 shadow_texel_size = vec2(1.0) / shadow_map_size;

    if (shadow_mode == 1) {
        return shadow_map_proj(shadow_coord);
    } else if (shadow_mode == 2) {
        return shadow_PCF(shadow_coord, shadow_texel_size, 1.0);
    } else { // 3
        return shadow_PCSS(shadow_coord, shadow_texel_size, sun_light_size);
    }
}
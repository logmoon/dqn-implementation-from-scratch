//
// LIGHT VERTEX SHADER
//
@vs light_vs
in vec2 position;
in vec4 color0;
in vec2 uv0;
in uint tex_index0;
in vec4 color_override0;

out vec4 color;
out vec4 color_override;
out vec2 uv;

void main() {
    gl_Position = vec4(position, 0, 1);
    color = color0;
    color_override = color_override0;
    uv = uv0;
}
@end

//
// LIGHT FRAGMENT SHADER
// This renders individual lights with radial falloff
//
@fs light_fs
in vec4 color;
in vec4 color_override;
in vec2 uv;

out vec4 col_out;

void main() {
    vec2 center = vec2(0.5, 0.5);
    float dist = distance(uv, center) * 2.0;
    
    // Get falloff from color_override.x
    float falloff = color_override.x;
    
    // Apply falloff (1.0 = linear, 2.0 = quadratic, etc.)
    float attenuation = 1.0 - pow(dist, falloff);
    attenuation = max(0.0, attenuation);
    
    col_out = vec4(color.rgb * attenuation, attenuation);
}
@end

@program light_shader light_vs light_fs
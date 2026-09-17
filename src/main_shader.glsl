//
// VERTEX SHADER
//
@vs vs
in vec2 position;
in vec4 color0;
in vec2 uv0;
in uint tex_index0;
in vec4 color_override0;
in vec2 world_pos0;

out vec4 color;
out vec2 uv;
out uint tex_index;
out vec4 color_override;
out vec2 world_pos;

void main() {
	gl_Position = vec4(position, 0, 1);
	color = color0;
	uv = uv0;
	tex_index = tex_index0;
	color_override = color_override0;
	world_pos = world_pos0;
}
@end


//
// FRAGMENT SHADER
//
@fs fs
layout(binding=0) uniform texture2D tex0;
layout(binding=1) uniform texture2D tex1;
layout(binding=2) uniform texture2D light_map;
layout(binding=0) uniform sampler default_sampler;

layout(binding=0) uniform fs_params {
    vec2 screen_size;
    float ambient_intensity;
    vec3 ambient_color;
};

in vec4 color;
in vec2 uv;
flat in uint tex_index;
in vec4 color_override;
in vec2 world_pos;

out vec4 col_out;

void main() {
	vec4 tex_col = vec4(1.0);
	if (tex_index == 0) {
		tex_col = texture(sampler2D(tex0, default_sampler), uv);
	} else if (tex_index == 1) {
		// this is text, it's only got the single .r channel so we stuff it into the alpha
		tex_col.a = texture(sampler2D(tex1, default_sampler), uv).r;
	}
	
	col_out = tex_col;
	col_out *= color;
	
	col_out.rgb = mix(col_out.rgb, color_override.rgb, color_override.a);
	
	// Sample light map using screen-space coordinates
	vec2 screen_uv = gl_FragCoord.xy / screen_size;
	vec4 light_sample = texture(sampler2D(light_map, default_sampler), screen_uv);
	
	// Combine ambient and dynamic lighting
	vec3 total_light = ambient_color * ambient_intensity + light_sample.rgb;
	
	// Apply lighting
	col_out.rgb *= total_light;
}
@end

@program quad vs fs
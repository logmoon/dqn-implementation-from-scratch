#include "assets.h"
#include "utils.h"

#define MAX_LIGHTS 256 + 1 // + 1 padding, cuz I want to be able to use 0 as "no light"

typedef enum {
    LIGHT_TYPE_POINT,
    LIGHT_TYPE_SPOT,
    LIGHT_TYPE_AMBIENT
} light_type_e;

typedef struct {
    bool active;
    u32 id;
    light_type_e type;
    vec2_t pos;
    vec3_t color;        // RGB 0-1
    float radius;
    float intensity;
    float falloff;       // 1.0 = linear, 2.0 = quadratic
    
    float angle;
    float direction;
} light_t;

typedef struct {
    light_t lights[MAX_LIGHTS];
    u32 light_count;
    u32 next_id;
    vec3_t ambient_color;
    float ambient_intensity;
} lighting_state_t;

typedef struct {
    vec2_t pos;
    vec4_t col;
    vec2_t uv;
    u32 tex_index;
    vec4_t col_override;
    vec2_t world_pos;
} vertex_t;

typedef vertex_t lf_quad_t[4];

#define MAX_QUADS 8192
#define MAX_VERTS (MAX_QUADS * 4)

typedef struct {
    lf_quad_t quads[MAX_QUADS];
    mat4_t projection;
    mat4_t camera_xform;
    u32 quad_count;
} draw_frame_t;

static draw_frame_t draw_frame;

static void draw_quad_projected(mat4_t world_to_clip, vec2_t positions[4], vec4_t colors[4], 
                               vec2_t uvs[4], u32 tex_indices[4], vec4_t color_overrides[4]) {
    if (draw_frame.quad_count >= MAX_QUADS) {
        log_error("Max quads reached (%d)", MAX_QUADS);
        return;
    }
    
    vertex_t* verts = draw_frame.quads[draw_frame.quad_count];
    draw_frame.quad_count++;
    
    // Transform positions to clip space and fill vertex data
    for (int i = 0; i < 4; i++) {
        vec4_t pos4 = vec4(positions[i].x, positions[i].y, 0.0f, 1.0f);
        vec4_t transformed = mat4_transform_vec4(world_to_clip, pos4);
        
        verts[i].pos = vec2(transformed.x, transformed.y);
        verts[i].col = colors[i];
        verts[i].uv = uvs[i];
        verts[i].tex_index = tex_indices[i];
        verts[i].col_override = color_overrides[i];
        verts[i].world_pos = positions[i];
    }
}

vec2_t get_sprite_size(image_id_e img_id) {
    if (img_id == IMAGE_NIL || img_id >= IMAGE_COUNT) return vec2(0, 0);
    
    const image_info_t* info = &image_infos[img_id];
    return vec2((float)info->width, (float)info->height);
}

void draw_sprite(vec2_t pos, image_id_e img_id, pivot_t pivot, mat4_t extra_xform, vec4_t color_override) {
    if (img_id == IMAGE_NIL || img_id >= IMAGE_COUNT) return;
    
    const image_info_t* info = &image_infos[img_id];
    vec2_t size = vec2((float)info->width, (float)info->height);

    vec2_t half_size = vec2(size.x * 0.5f, size.y * 0.5f);
    
    // Apply pivot offset
    vec2_t pivot_scale = scale_from_pivot(pivot);
    vec2_t pivot_offset = vec2(
        (pivot_scale.x - 0.5f) * size.x,
        (pivot_scale.y - 0.5f) * size.y
    );
    
    // Create positions around center, then offset by position and pivot
    vec2_t positions[4] = {
        vec2(pos.x - half_size.x - pivot_offset.x, pos.y - half_size.y - pivot_offset.y), // bottom-left
        vec2(pos.x - half_size.x - pivot_offset.x, pos.y + half_size.y - pivot_offset.y), // top-left  
        vec2(pos.x + half_size.x - pivot_offset.x, pos.y + half_size.y - pivot_offset.y), // top-right
        vec2(pos.x + half_size.x - pivot_offset.x, pos.y - half_size.y - pivot_offset.y)  // bottom-right
    };
    
    // If there's an extra transform, apply it to each position
    if (extra_xform.m[0] != 1.0f || extra_xform.m[5] != 1.0f || extra_xform.m[1] != 0.0f) { // Check if not identity
        for (int i = 0; i < 4; i++) {
            // Transform relative to sprite center
            vec2_t relative_pos = vec2(positions[i].x - pos.x, positions[i].y - pos.y);
            vec4_t pos4 = vec4(relative_pos.x, relative_pos.y, 0.0f, 1.0f);
            pos4 = mat4_transform_vec4(extra_xform, pos4);
            positions[i] = vec2(pos4.x + pos.x, pos4.y + pos.y);
        }
    }
    
    vec4_t white = vec4(1, 1, 1, 1);
    vec4_t colors[4] = {white, white, white, white};
    vec4_t overrides[4] = {color_override, color_override, color_override, color_override};
    
    vec2_t uvs[4] = {
        vec2(info->atlas_uvs[0], info->atlas_uvs[1]), // min_x, min_y
        vec2(info->atlas_uvs[0], info->atlas_uvs[3]), // min_x, max_y
        vec2(info->atlas_uvs[2], info->atlas_uvs[3]), // max_x, max_y
        vec2(info->atlas_uvs[2], info->atlas_uvs[1])  // max_x, min_y
    };
    
    u32 tex_indices[4] = {0, 0, 0, 0};
    
    // Use projection matrix
    mat4_t world_to_clip = mat4_multiply(draw_frame.camera_xform, draw_frame.projection);
    draw_quad_projected(world_to_clip, positions, colors, uvs, tex_indices, overrides);
}

void draw_rect(vec2_t pos, vec2_t size, vec4_t color, pivot_t pivot) {
    // Apply pivot offset
    vec2_t pivot_scale = scale_from_pivot(pivot);
    vec2_t pivot_offset = vec2(
        (pivot_scale.x - 0.5f) * size.x,
        (pivot_scale.y - 0.5f) * size.y
    );
    
    vec2_t half_size = vec2(size.x * 0.5f, size.y * 0.5f);
    
    // Create positions around center, then offset by position and pivot
    vec2_t positions[4] = {
        vec2(pos.x - half_size.x - pivot_offset.x, pos.y - half_size.y - pivot_offset.y), // bottom-left
        vec2(pos.x - half_size.x - pivot_offset.x, pos.y + half_size.y - pivot_offset.y), // top-left  
        vec2(pos.x + half_size.x - pivot_offset.x, pos.y + half_size.y - pivot_offset.y), // top-right
        vec2(pos.x + half_size.x - pivot_offset.x, pos.y - half_size.y - pivot_offset.y)  // bottom-right
    };
    
    // Use the provided color for all vertices
    vec4_t colors[4] = {color, color, color, color};
    
    // No texture coordinates needed for solid color, but we need to provide them
    // We'll use (0,0) which should map to a white pixel in your atlas
    vec2_t uvs[4] = {
        vec2(0.0f, 0.0f),
        vec2(0.0f, 0.0f),
        vec2(0.0f, 0.0f),
        vec2(0.0f, 0.0f)
    };
    
    // Use texture index 255 (no atlas) and no color override
    u32 tex_indices[4] = {255, 0, 0, 0};
    vec4_t no_override = vec4(0, 0, 0, 0);
    vec4_t overrides[4] = {no_override, no_override, no_override, no_override};
    
    // Use projection matrix
    mat4_t world_to_clip = mat4_multiply(draw_frame.camera_xform, draw_frame.projection);
    draw_quad_projected(world_to_clip, positions, colors, uvs, tex_indices, overrides);
}

void draw_rect_xform(vec2_t pos, vec2_t size, vec4_t color, pivot_t pivot, mat4_t extra_xform) {
    // Apply pivot offset
    vec2_t pivot_scale = scale_from_pivot(pivot);
    vec2_t pivot_offset = vec2(
        (pivot_scale.x - 0.5f) * size.x,
        (pivot_scale.y - 0.5f) * size.y
    );
    
    vec2_t half_size = vec2(size.x * 0.5f, size.y * 0.5f);
    
    // Create positions around center, then offset by position and pivot
    vec2_t positions[4] = {
        vec2(pos.x - half_size.x - pivot_offset.x, pos.y - half_size.y - pivot_offset.y), // bottom-left
        vec2(pos.x - half_size.x - pivot_offset.x, pos.y + half_size.y - pivot_offset.y), // top-left  
        vec2(pos.x + half_size.x - pivot_offset.x, pos.y + half_size.y - pivot_offset.y), // top-right
        vec2(pos.x + half_size.x - pivot_offset.x, pos.y - half_size.y - pivot_offset.y)  // bottom-right
    };
    
    // Apply extra transform if it's not identity
    if (extra_xform.m[0] != 1.0f || extra_xform.m[5] != 1.0f || extra_xform.m[1] != 0.0f) {
        for (int i = 0; i < 4; i++) {
            // Transform relative to rectangle center
            vec2_t relative_pos = vec2(positions[i].x - pos.x, positions[i].y - pos.y);
            vec4_t pos4 = vec4(relative_pos.x, relative_pos.y, 0.0f, 1.0f);
            pos4 = mat4_transform_vec4(extra_xform, pos4);
            positions[i] = vec2(pos4.x + pos.x, pos4.y + pos.y);
        }
    }
    
    // Use the provided color for all vertices
    vec4_t colors[4] = {color, color, color, color};
    
    // No texture coordinates needed for solid color
    vec2_t uvs[4] = {
        vec2(0.0f, 0.0f),
        vec2(0.0f, 0.0f),
        vec2(0.0f, 0.0f),
        vec2(0.0f, 0.0f)
    };
    
    // Use texture index 255 (no atlas) and no color override
    u32 tex_indices[4] = {255, 0, 0, 0};
    vec4_t no_override = vec4(0, 0, 0, 0);
    vec4_t overrides[4] = {no_override, no_override, no_override, no_override};
    
    // Use projection matrix
    mat4_t world_to_clip = mat4_multiply(draw_frame.camera_xform, draw_frame.projection);
    draw_quad_projected(world_to_clip, positions, colors, uvs, tex_indices, overrides);
}

void draw_text(vec2_t pos, const char* text, font_id_e font_id, float scale) {
    if (font_id == FONT_NIL || font_id >= FONT_COUNT) return;
    if (!text || *text == '\0') return;
    
    const font_info_t* font = &font_infos[font_id];
    float x = pos.x;
    float y = pos.y;
    
    for (const char* c = text; *c; c++) {
        int char_index = *c - 32;  // ASCII offset (space = 32)
        if (char_index < 0 || char_index >= 96) {
            // Skip unsupported characters
            continue;
        }
        
        const font_char_t* char_data = &font->char_data[char_index];
        
        // Calculate character dimensions
        float char_width = (float)(char_data->x1 - char_data->x0);
        float char_height = (float)(char_data->y1 - char_data->y0);
        
        // Skip rendering if character has no visible pixels
        if (char_width <= 0 || char_height <= 0) {
            x += char_data->xadvance * scale;
            continue;
        }
        
        // Calculate character position with offset
        // Note: yoff is typically negative for characters that sit on the baseline
        // We need to flip it because our Y-axis goes up, but font metrics assume Y goes down
        vec2_t char_pos = vec2(
            x + char_data->xoff * scale,
            y - char_data->yoff * scale - char_height * scale
        );
        
        // Create quad positions (character rect)
        vec2_t positions[4] = {
            vec2(char_pos.x, char_pos.y),                                    // bottom-left
            vec2(char_pos.x, char_pos.y + char_height * scale),              // top-left
            vec2(char_pos.x + char_width * scale, char_pos.y + char_height * scale), // top-right
            vec2(char_pos.x + char_width * scale, char_pos.y)                // bottom-right
        };
        
        // UV coordinates from the font atlas
        vec2_t uvs[4] = {
            vec2(char_data->s0, char_data->t1), // bottom-left (note: t1 for bottom)
            vec2(char_data->s0, char_data->t0), // top-left (note: t0 for top)
            vec2(char_data->s1, char_data->t0), // top-right
            vec2(char_data->s1, char_data->t1)  // bottom-right
        };
        
        // Set up colors and texture index
        vec4_t white = vec4(1, 1, 1, 1);
        vec4_t colors[4] = {white, white, white, white};
        vec4_t no_override = vec4(0, 0, 0, 0);
        vec4_t overrides[4] = {no_override, no_override, no_override, no_override};
        
        // Use texture index 1 for font textures (as per your shader)
        u32 tex_indices[4] = {1, 1, 1, 1};
        
        // Draw the character quad
        mat4_t world_to_clip = mat4_multiply(draw_frame.camera_xform, draw_frame.projection);
        draw_quad_projected(world_to_clip, positions, colors, uvs, tex_indices, overrides);
        
        // Advance to next character position
        x += char_data->xadvance * scale;
    }
}

vec2_t measure_text(const char* text, font_id_e font_id, float scale) {
    if (font_id == FONT_NIL || font_id >= FONT_COUNT) return vec2(0, 0);
    if (!text || *text == '\0') return vec2(0, 0);
    
    const font_info_t* font = &font_infos[font_id];
    float width = 0;
    float height = font->font_height * scale;
    
    for (const char* c = text; *c; c++) {
        int char_index = *c - 32;
        if (char_index < 0 || char_index >= 96) continue;
        
        const font_char_t* char_data = &font->char_data[char_index];
        width += char_data->xadvance * scale;
    }
    
    return vec2(width, height);
}

void lighting_init(lighting_state_t* state, vec3_t ambient_color, float ambient_intensity) {
    if (!state) return;
    
    memset(state, 0, sizeof(lighting_state_t));
    state->ambient_color = ambient_color;
    state->ambient_intensity = ambient_intensity;
    state->next_id = 1;
}

u32 light_create(lighting_state_t* state, vec2_t pos, vec3_t color, float radius, float intensity, float falloff) {
    if (!state) return 0;
    
    // Find free slot
    for (u32 i = 0; i < MAX_LIGHTS; i++) {
        if (!state->lights[i].active) {
            light_t* light = &state->lights[i];
            light->active = true;
            light->id = state->next_id++;
            light->type = LIGHT_TYPE_POINT;
            light->pos = pos;
            light->color = color;
            light->radius = radius;
            light->intensity = intensity;
            light->falloff = falloff;
            
            if (i >= state->light_count) {
                state->light_count = i + 1;
            }
            
            return light->id;
        }
    }
    
    log_error("Max lights reached (%d)", MAX_LIGHTS);
    return 0;
}

light_t* light_get(lighting_state_t* state, u32 id) {
    if (!state || id == 0) return NULL;
    
    for (u32 i = 0; i < state->light_count; i++) {
        if (state->lights[i].active && state->lights[i].id == id) {
            return &state->lights[i];
        }
    }
    
    return NULL;
}

void light_remove(lighting_state_t* state, u32 id) {
    light_t* light = light_get(state, id);
    if (light) {
        light->active = false;
    }
}

void light_set_pos(lighting_state_t* state, u32 id, vec2_t pos) {
    light_t* light = light_get(state, id);
    if (light) {
        light->pos = pos;
    }
}

void light_set_color(lighting_state_t* state, u32 id, vec3_t color) {
    light_t* light = light_get(state, id);
    if (light) light->color = color;
}

void light_set_radius(lighting_state_t* state, u32 id, float radius) {
    light_t* light = light_get(state, id);
    if (light) light->radius = radius;
}

void light_set_intensity(lighting_state_t* state, u32 id, float intensity) {
    light_t* light = light_get(state, id);
    if (light) light->intensity = intensity;
}

void light_set_falloff(lighting_state_t* state, u32 id, float falloff) {
    light_t* light = light_get(state, id);
    if (light) light->falloff = falloff;
}

void draw_light_quad(vec2_t pos, vec2_t size, vec4_t color, float falloff) {
    vec2_t half_size = vec2(size.x * 0.5f, size.y * 0.5f);
    
    vec2_t positions[4] = {
        vec2(pos.x - half_size.x, pos.y - half_size.y),
        vec2(pos.x - half_size.x, pos.y + half_size.y),
        vec2(pos.x + half_size.x, pos.y + half_size.y),
        vec2(pos.x + half_size.x, pos.y - half_size.y)
    };
    
    vec4_t colors[4] = {color, color, color, color};
    
    vec2_t uvs[4] = {
        vec2(0.0f, 0.0f),
        vec2(0.0f, 1.0f),
        vec2(1.0f, 1.0f),
        vec2(1.0f, 0.0f)
    };
    
    u32 tex_indices[4] = {255, 0, 0, 0};
    
    // Pack falloff into color_override.x
    vec4_t falloff_data = vec4(falloff, 0, 0, 0);
    vec4_t overrides[4] = {falloff_data, falloff_data, falloff_data, falloff_data};
    
    mat4_t world_to_clip = mat4_multiply(draw_frame.camera_xform, draw_frame.projection);
    draw_quad_projected(world_to_clip, positions, colors, uvs, tex_indices, overrides);
}

void lighting_render(lighting_state_t* state, vec2_t camera_pos) {
    if (!state) return;
    
    for (u32 i = 0; i < state->light_count; i++) {
        light_t* light = &state->lights[i];
        if (!light->active) continue;

        if (light->type == LIGHT_TYPE_POINT) {
            float size = light->radius * 2.0f;
            
            vec4_t light_color = vec4(
                light->color.x * light->intensity,
                light->color.y * light->intensity,
                light->color.z * light->intensity,
                1.0f
            );
            
            draw_light_quad(light->pos, vec2(size, size), light_color, light->falloff);
        }
    }
}
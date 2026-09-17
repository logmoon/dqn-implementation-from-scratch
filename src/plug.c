#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <math.h>
#include <string.h>

#include "build/config.h"
#include "plug.h"

#include "thirdparty/sokol/sokol_app.h"
#include "thirdparty/sokol/sokol_audio.h"
#include "thirdparty/sokol/sokol_gfx.h"
#include "thirdparty/sokol/sokol_log.h"
#include "thirdparty/sokol/sokol_glue.h"
#include "thirdparty/sokol/sokol_time.h"

#include "main_shader.h"
#include "lighting_shader.h"
#include "assets.h"
#include "utils.h"

#if defined(_WIN32) && defined(LF_HOTRELOAD)
    #define LF_PLUG __declspec(dllexport)
#else
    #define LF_PLUG
#endif

#define PLUG(name, ret, ...) LF_PLUG ret name(__VA_ARGS__);
LIST_OF_PLUGS
#undef PLUG

#include "game.c"

typedef struct {
    sg_pass_action pass_action;
    sg_pipeline pip;
    sg_bindings bind;
    sg_image atlas_texture;
    sg_image font_textures[FONT_COUNT];

    sg_pass_action light_pass_action;
    sg_pipeline light_pip;
    sg_bindings light_bind;
    sg_attachments light_map_attachments;
    sg_image light_map_texture;
    sg_pass light_map_pass;

    u64 time_now;
    double accumulator;
    f32 dt_multiplier;
    game_state_t game_state;

    lighting_state_t lighting_state;
} state_t;
static state_t *state = NULL;

static f64 fixed_dt = 1.0 / 120.0; // Delta-time
static f64 max_frame_time = 0.25;  // Maximum frame time to prevent slowdowns

void load_assets(void) {
    sg_image_desc atlas_desc = {
        .width = atlas_width,
        .height = atlas_height,
        .pixel_format = SG_PIXELFORMAT_RGBA8,
        .data.subimage[0][0] = {
            .ptr = atlas_data,
            .size = atlas_data_size
        }
    };
    state->atlas_texture = sg_make_image(&atlas_desc);
    if (state->atlas_texture.id == SG_INVALID_ID) {
        log_error("Failed to create atlas texture");
        return;
    }

    if (FONT_COUNT > 1) {
        sg_image_desc font_desc = {
            .width = font_infos[FONT_ALAGARD].bitmap_width,
            .height = font_infos[FONT_ALAGARD].bitmap_height,
            .pixel_format = SG_PIXELFORMAT_R8,
            .data.subimage[0][0] = {
                .ptr = font_alagard_data,
                .size = font_alagard_data_size
            }
        };
        state->font_textures[FONT_ALAGARD] = sg_make_image(&font_desc);
        if (state->font_textures[FONT_ALAGARD].id == SG_INVALID_ID) {
            log_error("Failed to create font texture for font: %d", FONT_ALAGARD);
            return;
        }
    }
}

void free_assets(void) {
    sg_destroy_image(state->atlas_texture);
    for (size_t i = 1; i < FONT_COUNT; i++) {  // Start from 1, skip FONT_NIL
        if (sg_query_image_state(state->font_textures[i]) == SG_RESOURCESTATE_VALID) {
            sg_destroy_image(state->font_textures[i]);
        }
    }
}

void bind_textures() {
    // Bind the atlas texture
    if (state->atlas_texture.id == SG_INVALID_ID) {
        log_error("Failed to create atlas texture");
        return;
    }
    state->bind.images[IMG_tex0] = state->atlas_texture;

    // Bind font textures
    if (FONT_COUNT <= 1) return; // No font textures to bind
    if (state->font_textures[FONT_ALAGARD].id == SG_INVALID_ID) {
        log_error("Failed to create font texture for font: %d", FONT_ALAGARD);
        return;
    }
    state->bind.images[IMG_tex1] = state->font_textures[FONT_ALAGARD];
}

void bind_light_textures() {
    if (state->light_map_texture.id == SG_INVALID_ID) {
        log_error("Failed to bind light map texture");
        return;
    }
    state->bind.images[IMG_light_map] = state->light_map_texture;
}


////
////

void tick(double dt) {
    // Reset all entities frame data at the start of the tick
    // We do this here before setting them so that draw() has access to them.
    for (u32 i = 0; i < state->game_state.entity_count; i++) {
        entity_t* entity = &state->game_state.entities[i];
        entity_reset_frame_data(entity);
    }

    // Process messages
    message_process(&state->game_state);

    // Update entities and apply physics
    for (u32 i = 0; i < state->game_state.entity_count; i++) {
        entity_t* entity = &state->game_state.entities[i];
        if (!entity || !entity_has_flag(entity, ENTITY_FLAG_ALLOCATED)) {
            continue;
        }

        if (entity->light_id != 0) {
            // Update light position if the entity has a light
            light_set_pos(&state->lighting_state, entity->light_id, entity->pos);
        }

        ////
        // Specific entity logic tick
        entity_def_t* def = entity_get_def(&state->game_state, entity->type);
        if (def && def->logic_tick) {
            def->logic_tick(&state->game_state, entity, dt);
        }
        ////

        // We check for ENTITY_FLAG_ALLOCATED again because the entity could've been destroyed above
        if (!entity_has_flag(entity, ENTITY_FLAG_ALLOCATED)) continue;

        ////
        // Physics
        if (entity_has_flag(entity, ENTITY_FLAG_ALLOCATED) && entity_has_flag(entity, ENTITY_FLAG_PHYSICS)) {
            entity->frame.input_axis = vec2_normalize(entity->frame.input_axis);

            float acceleration = 12.0f;
            float max_speed = entity->speed;
            float friction = entity->mvt_ignore_friction ? 1.0f : 0.8f;

            // If there's input, accelerate in that direction
            if (vec2_length(entity->frame.input_axis) > 0.1f) {
                // Calculate target velocity based on input direction and max speed
                vec2_t target_vel = vec2_scale(entity->frame.input_axis, max_speed);

                // Accelerate toward target velocity
                vec2_t accel_vector = vec2_subtract(target_vel, entity->vel);
                accel_vector = vec2_scale(accel_vector, acceleration * (float)dt);

                // Apply acceleration to velocity
                entity->vel = vec2_add(entity->vel, accel_vector);

                // Cap velocity at max speed
                float speed = vec2_length(entity->vel);
                if (speed > max_speed) {
                    entity->vel = vec2_scale(entity->vel, max_speed / speed);
                }
            } else {
                // Apply friction when no input
                entity->vel = vec2_scale(entity->vel, friction);

                // Stop completely if very slow
                if (vec2_length(entity->vel) < 1.0f) {
                    entity->vel = vec2(0, 0);
                }
            }

            entity->force_vel = vec2_scale(entity->force_vel, friction * 1.1f);

            // Update position
            vec2_t final_vel = vec2_add(entity->vel, entity->force_vel);
            entity->pos = vec2_add(entity->pos, vec2_scale(final_vel, (float)dt));

            // Check for collisions
            for (u32 j = 0; j < state->game_state.entity_count; j++) {
                if (i == j) continue;
                entity_t* other = &state->game_state.entities[j];
                if (!other || !entity_has_flag(other, ENTITY_FLAG_ALLOCATED)) continue;

                ////
                // Specific entity physics tick
                entity_def_t* def = entity_get_def(&state->game_state, entity->type);
                if (def && def->physics_tick) {
                    if (def->physics_tick(&state->game_state, entity, other, dt)) {
                        break; // Break from the j loop
                    }
                }
                ////
            }
        }
        ////

        // Increment ticks alive
        entity->ticks_alive++;
    }
}

void draw(f64 alpha) {
    vec2_t camera_pos = vec2(0, 0);
    
    float scale_x = (float)sapp_width() / LF_GAME_WIDTH;
    float scale_y = (float)sapp_height() / LF_GAME_HEIGHT;
    float scale_factor = (scale_x < scale_y) ? scale_x : scale_y;
    
    draw_frame.projection = mat4_ortho(
        sapp_width()  * -0.5 / scale_factor,
        sapp_width()  *  0.5 / scale_factor,
        sapp_height() * -0.5 / scale_factor,
        sapp_height() *  0.5 / scale_factor,
        -1, 1
    );
    
    // ========== PASS 1: Render lights to light map ==========
    draw_frame.quad_count = 0;
    draw_frame.camera_xform = mat4_identity();
    
    lighting_render(&state->lighting_state, camera_pos);

    if (draw_frame.quad_count > 0) {
        sg_update_buffer(state->light_bind.vertex_buffers[0], &(sg_range){
            .ptr = draw_frame.quads,
            .size = sizeof(vertex_t) * 4 * draw_frame.quad_count
        });
        
        sg_begin_pass(&(sg_pass){ 
            .action = state->light_pass_action, 
            .attachments = state->light_map_attachments 
        });
        sg_apply_pipeline(state->light_pip);
        sg_apply_bindings(&state->light_bind);
        sg_draw(0, draw_frame.quad_count * 6, 1);
        sg_end_pass();
    } else {
        sg_begin_pass(&(sg_pass){ 
            .action = state->light_pass_action, 
            .attachments = state->light_map_attachments 
        });
        sg_end_pass();
    }
    
    // ========== PASS 2: Render main scene with lighting ==========
    draw_frame.quad_count = 0;
    
    draw_frame.camera_xform = mat4_translate(vec2(-camera_pos.x, -camera_pos.y));
    
    draw_game(&state->game_state, alpha);
    
    sg_update_buffer(state->bind.vertex_buffers[0], &(sg_range){
        .ptr = draw_frame.quads,
        .size = sizeof(vertex_t) * 4 * draw_frame.quad_count
    });
    
    fs_params_t fs_params = {
        .screen_size = {(float)sapp_width(), (float)sapp_height()},
        .ambient_intensity = state->lighting_state.ambient_intensity,
        .ambient_color = {
            state->lighting_state.ambient_color.x,
            state->lighting_state.ambient_color.y,
            state->lighting_state.ambient_color.z
        }
    };
    
    sg_begin_pass(&(sg_pass){.action = state->pass_action, .swapchain = sglue_swapchain()});
    sg_apply_pipeline(state->pip);
    sg_apply_bindings(&state->bind);
    sg_apply_uniforms(UB_fs_params, &SG_RANGE(fs_params));
    sg_draw(0, draw_frame.quad_count * 6, 1);
    sg_end_pass();
    sg_commit();
}
// ============================================================================
// ============================================================================


// Initialize the plugin
LF_PLUG void plug_init() {
    // Allocate and initialize game state
    state = malloc(sizeof(*state));
    if (state == NULL) {
        log_critical("Buy more RAM lol");
    }
    memset(state, 0, sizeof(*state));
    state->dt_multiplier = 1.0f;

    // Start timer
    stm_setup();
    state->time_now = stm_now();
    state->accumulator = 0.0;

    // Start logging
    log_init("lf.log");

    // Start audio
    audio_init();

    // Initialize game state
    game_state_init(&state->game_state, 2);

    // Initialize lighting with dark ambient
    lighting_init(&state->lighting_state, state->game_state.ambient_color, state->game_state.ambient_intensity);
    state->game_state.lighting = &state->lighting_state;

    // Load the assets
    load_assets();

    // Locks/Unlocks the mouse cursor
    // sapp_lock_mouse(!sapp_mouse_locked());
    // Shows/Hides the mouse cursor
    // sapp_show_mouse(!sapp_mouse_shown());

    // Create light map texture
    state->light_map_texture = sg_make_image(&(sg_image_desc){
        .usage = { .render_attachment = true },
        .width = LF_WINDOW_WIDTH,
        .height = LF_WINDOW_HEIGHT,
        .pixel_format = SG_PIXELFORMAT_RGBA8,
    });
    
    state->light_map_attachments = sg_make_attachments(&(sg_attachments_desc){
        .colors[0].image = state->light_map_texture,
    });
    
    state->light_pass_action = (sg_pass_action){
        .colors[0] = {
            .load_action = SG_LOADACTION_CLEAR,
            .clear_value = {0, 0, 0, 0}
        }
    };
    
    // Create vertex buffers (separate for each pass)
    state->bind.vertex_buffers[0] = sg_make_buffer(&(sg_buffer_desc){
        .usage = {.dynamic_update = true},
        .size = sizeof(lf_quad_t) * MAX_QUADS,
    });
    
    state->light_bind.vertex_buffers[0] = sg_make_buffer(&(sg_buffer_desc){
        .usage = {.dynamic_update = true},
        .size = sizeof(lf_quad_t) * MAX_QUADS,
    });
    
    // Create index buffer (shared)
    uint16_t indices[MAX_QUADS * 6];
    for (int i = 0, quad = 0; i < MAX_QUADS * 6; i += 6, quad++) {
        indices[i + 0] = quad * 4 + 0;
        indices[i + 1] = quad * 4 + 1;
        indices[i + 2] = quad * 4 + 2;
        indices[i + 3] = quad * 4 + 0;
        indices[i + 4] = quad * 4 + 2;
        indices[i + 5] = quad * 4 + 3;
    }
    
    state->bind.index_buffer = sg_make_buffer(&(sg_buffer_desc){
        .usage = {.vertex_buffer = false, .index_buffer = true},
        .data = {.ptr = indices, .size = sizeof(indices)},
    });
    state->light_bind.index_buffer = state->bind.index_buffer;
    
    bind_textures();
    bind_light_textures();
    
    state->bind.samplers[SMP_default_sampler] = sg_make_sampler(&(sg_sampler_desc){
        .min_filter = SG_FILTER_NEAREST,
        .mag_filter = SG_FILTER_NEAREST,
    });
    state->light_bind.samplers[SMP_default_sampler] = state->bind.samplers[SMP_default_sampler];
    
    // Main pipeline
    state->pip = sg_make_pipeline(&(sg_pipeline_desc){
        .shader = sg_make_shader(quad_shader_desc(sg_query_backend())),
        .index_type = SG_INDEXTYPE_UINT16,
        .layout = {
            .attrs = {
                [0] = {.format = SG_VERTEXFORMAT_FLOAT2, .offset = 0},   // position
                [1] = {.format = SG_VERTEXFORMAT_FLOAT4, .offset = 8},   // color
                [2] = {.format = SG_VERTEXFORMAT_FLOAT2, .offset = 24},  // uv
                [3] = {.format = SG_VERTEXFORMAT_UINT,   .offset = 32},  // tex_index
                [4] = {.format = SG_VERTEXFORMAT_FLOAT4, .offset = 36},  // color_override
                [5] = {.format = SG_VERTEXFORMAT_FLOAT2, .offset = 52},  // world_pos
            },
            .buffers[0] = {.stride = sizeof(vertex_t)}
        },
        .colors[0] = {
            .blend = {
                .enabled = true,
                .src_factor_rgb = SG_BLENDFACTOR_SRC_ALPHA,
                .dst_factor_rgb = SG_BLENDFACTOR_ONE_MINUS_SRC_ALPHA,
                .op_rgb = SG_BLENDOP_ADD,
                .src_factor_alpha = SG_BLENDFACTOR_ONE,
                .dst_factor_alpha = SG_BLENDFACTOR_ONE_MINUS_SRC_ALPHA,
                .op_alpha = SG_BLENDOP_ADD,
            }
        }
    });
    
    // Light pipeline (additive blending)
    state->light_pip = sg_make_pipeline(&(sg_pipeline_desc){
        .shader = sg_make_shader(light_shader_shader_desc(sg_query_backend())),
        .index_type = SG_INDEXTYPE_UINT16,
        .layout = {
            .attrs = {
                [0] = {.format = SG_VERTEXFORMAT_FLOAT2, .offset = 0},  // position
                [1] = {.format = SG_VERTEXFORMAT_FLOAT4, .offset = 8},  // color
                [2] = {.format = SG_VERTEXFORMAT_FLOAT2, .offset = 24}, // uv
                [3] = {.format = SG_VERTEXFORMAT_UINT,   .offset = 32}, // tex_index
                [4] = {.format = SG_VERTEXFORMAT_FLOAT4, .offset = 36}, // color_override
            },
            .buffers[0] = {.stride = sizeof(vertex_t)}
        },
        .colors[0] = {
            .pixel_format = SG_PIXELFORMAT_RGBA8,
            .blend = {
                .enabled = true,
                .src_factor_rgb = SG_BLENDFACTOR_ONE,
                .dst_factor_rgb = SG_BLENDFACTOR_ONE,
                .op_rgb = SG_BLENDOP_ADD,
                .src_factor_alpha = SG_BLENDFACTOR_ONE,
                .dst_factor_alpha = SG_BLENDFACTOR_ONE,
                .op_alpha = SG_BLENDOP_ADD,
            }
        },
        .depth = {
            .pixel_format = SG_PIXELFORMAT_NONE,
        }
    });
    
    state->pass_action = (sg_pass_action){
        .colors[0] = {.load_action = SG_LOADACTION_CLEAR, .clear_value = {0, 0, 0, 1}}
    };
    
    start_game(&state->game_state);
    // We swap the message queues here to ensure that the initial messages are processed
    message_swap(&state->game_state.message_state);
}

// Main frame update function
LF_PLUG void plug_frame(void) {
    if (state == NULL) {
        log_critical("State was lost");
    }

    u64 new_time = stm_now();
    u64 delta_ticks = stm_diff(new_time, state->time_now);
    double frame_time = stm_sec(delta_ticks);
    if (frame_time > max_frame_time) frame_time = max_frame_time;
    state->time_now = new_time;

    state->accumulator += frame_time;

    while (state->accumulator >= fixed_dt) {
        // Process input
        input(&state->game_state);

        // Tick (logic first and then physics)
        tick(fixed_dt);

        // Swap message queues and clear the next queue for the next frame
        message_swap(&state->game_state.message_state);

        // Reset input state for the next frame
        input_reset_state_for_new_frame(&state->game_state.input_state);

        state->accumulator -= fixed_dt;
    }

    double alpha = state->accumulator / fixed_dt;
    draw(alpha);
}


LF_PLUG void plug_event(const void* ev)
{
    if (state == NULL) {
        log_critical("State was lost");
    }
    sapp_event* event = (sapp_event*)ev;
    input_event(&state->game_state.input_state, event);
}

// Save state before hot-reload
LF_PLUG void* plug_pre_reload(void) {
    // Free assets and return the state state
    audio_shutdown();
    free_assets();
    log_cleanup();
    return state;
}

// Restore state after hot-reload
LF_PLUG void plug_post_reload(void* s) {
    // Retrieve the state, load assets, and bind textures
    state = s;
    log_init("lf.log");
    audio_init();
    load_assets();
    bind_textures();
    bind_light_textures();
    define_entities(&state->game_state); // Re-define entity definitions
}

LF_PLUG void plug_cleanup()
{
    sg_destroy_image(state->light_map_texture);
    log_cleanup();
    free(state);
    state = NULL;
}

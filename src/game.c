#include "utils.h"
#include "audio.c"
#include "draw.c"

#include "rl.c"

// ===========================================================================
//                               TYPE DEFINITIONS
// ===========================================================================
typedef enum {
    INPUT_STATE_FLAG_NONE          = 0,      // 0
    INPUT_STATE_FLAG_DOWN          = 1 << 0, // 1
    INPUT_STATE_FLAG_JUST_PRESSED  = 1 << 1, // 2
    INPUT_STATE_FLAG_JUST_RELEASED = 1 << 2, // 4
    INPUT_STATE_FLAG_REPEAT        = 1 << 3  // 8
} input_state_flag_e;

typedef struct {
    input_state_flag_e flags;
    sapp_event event;  // Store the last event for this key
} key_state_t;

typedef struct {
    key_state_t keys[SAPP_MAX_KEYCODES];
    key_state_t mouse_buttons[SAPP_MAX_MOUSEBUTTONS];
    vec2_t mouse_pos;
    vec2_t mouse_delta;
    vec2_t mouse_scroll;
} input_state_t;

#define MAX_ENTITIES 1024

typedef enum {
    ENTITY_TYPE_NONE = 0,
    ENTITY_TYPE_PADDLE_LEFT,
    ENTITY_TYPE_PADDLE_RIGHT,
    ENTITY_TYPE_BALL,
    ENTITY_TYPE_COUNT
} entity_type_e;

typedef enum {
    ENTITY_FLAG_NONE      = 0,
    ENTITY_FLAG_ALLOCATED = 1 << 0, // 1
    ENTITY_FLAG_PHYSICS   = 1 << 1, // 2
    ENTITY_FLAG_INVISIBLE = 1 << 4, // 16
} entity_flag_e;

typedef enum {
    MESSAGE_TYPE_CREATE_ENTITY,
    MESSAGE_TYPE_COUNT
} message_type_e;

typedef struct {
    entity_type_e type;
    u32 parent_id; // ID of the entity that created this entity
    vec2_t pos;    // Position of the entity
    f32 rot;       // Rotation of the entity in radians
    u32 light_id; // ID of the light associated with this entity, -1 if none
} mt_create_entity_t;

typedef struct {
    message_type_e type;
    u32 from_entity;
    union {
        mt_create_entity_t create_entity; // For creating new entities
        // Other message types can be added here
    };
} message_t;

#define MAX_MESSAGES 64

typedef struct {
    message_t messages[MAX_MESSAGES];
    u32 count;
} message_queue_t;

typedef struct {
    message_queue_t message_queue_current;
    message_queue_t message_queue_next;
} message_state_t;

typedef struct entity_t entity_t;
typedef struct game_state_t game_state_t;

struct entity_t {
    u32 id;
    u32 parent_id;
    u64 ticks_alive;
    entity_type_e type;
    entity_flag_e flags;
    struct {
        vec2_t input_axis;
    } frame;
    vec2_t last_tick_input;
    f32 speed;
    f32 rot;
    vec2_t scale;
    vec2_t pos;
    vec2_t size;
    vec2_t vel;
    vec2_t force_vel;
    vec2_t acc;
    vec4_t color_override;
    bool mvt_ignore_friction;
    u32 light_id;
};


////
// Entity update functions:
// - Logic tick happens before physics, we can destroy entities there.
// - Physics tick happens inside physics, we can't destroy entities there,
//   we get the entity itself and another to check against for specific interactions,
//   if the physics tick func returns true, we stop processing that entity's physics,
//   and move on to the next entity.
typedef struct {
    entity_type_e type;
    entity_t* (*create)(game_state_t*, mt_create_entity_t*);
    void (*logic_tick)(game_state_t*, entity_t*, double);
    bool (*physics_tick)(game_state_t*, entity_t*, entity_t*, double);
    void (*destroy)(game_state_t*, entity_t*);
    u32 max_active;
    u32 max_attack_tokens; // -1 means use global pool of attack tokens
} entity_def_t;
////

typedef struct {
    int score_left;
    int score_right;
    u32 ball_id;
    u32 paddle_left_id;
    u32 paddle_right_id;
	int last_hit; // 0 if noone, -1 if left, 1 if right
    struct {
        int scored; // 0 if noone score, -1 if left scored, 1 if right scored
        bool hit;
    } frame;
} pong_state_t;

struct game_state_t {
    entity_t entities[MAX_ENTITIES];
    entity_def_t entity_defs[ENTITY_TYPE_COUNT];
    u32 player_id;
    u32 entity_count;
    message_state_t message_state;
    lighting_state_t* lighting;
    rng_state_t rng;
    input_state_t input_state;

    // Game mode specific states
    pong_state_t pong;
    rl_state_t rl_state;

    // Lighting stuff
    vec3_t ambient_color;
    float ambient_intensity;
};

typedef struct {
    vec2_t min;
    vec2_t max;
} aabb_t;
// ===========================================================================
// ===========================================================================
// ===========================================================================


// ===========================================================================
//                               GAME UTILS
// ===========================================================================
u64 seconds_to_ticks(f64 seconds, f64 dt) {
    return (u64)(seconds/dt);
}
f32 ticks_to_seconds(u64 ticks, f64 dt) {
    return (f32)(ticks*dt);
}
// ===========================================================================
// ===========================================================================
// ===========================================================================


// ===========================================================================
//                         MASSAGES (GAME API)
// ===========================================================================
bool message_add(message_state_t* state, message_t message) {
    if (!state) {
        log_error("Message state is null");
        return false;
    }

    message_queue_t* queue = &state->message_queue_next;
    if (queue->count < MAX_MESSAGES) {
        queue->messages[queue->count] = message;
        queue->count++;
        return true;
    }
    
    log_warn("Message queue full");
    return false;
}
// ===========================================================================
// ===========================================================================
// ===========================================================================

// ===========================================================================
//                               ENTITIES
// ===========================================================================
void entity_set_flag(entity_t* entity, entity_flag_e flag) {
    entity->flags |= flag;
}
void entity_clear_flag(entity_t* entity, entity_flag_e flag) {
    entity->flags &= ~flag;
}
bool entity_has_flag(entity_t* entity, entity_flag_e flag) {
    return (entity->flags & flag) != 0;
}
aabb_t get_entity_aabb(entity_t* entity) {
    aabb_t result;
    vec2_t half_size = vec2_scale(entity->size, 0.5f);
    result.min = vec2(entity->pos.x - half_size.x, entity->pos.y - half_size.y);
    result.max = vec2(entity->pos.x + half_size.x, entity->pos.y + half_size.y);
    return result;
}
bool aabb_collision(aabb_t a, aabb_t b) {
    return (a.min.x <= b.max.x && a.max.x >= b.min.x &&
            a.min.y <= b.max.y && a.max.y >= b.min.y);
}
bool entity_collision(entity_t* a, entity_t* b) {
    if (!entity_has_flag(a, ENTITY_FLAG_ALLOCATED) || !entity_has_flag(b, ENTITY_FLAG_ALLOCATED)) {
        return false;
    }
    
    aabb_t a_aabb = get_entity_aabb(a);
    aabb_t b_aabb = get_entity_aabb(b);
    
    return aabb_collision(a_aabb, b_aabb);
}

entity_t* entity_get_by_id(game_state_t* state, u32 id) {
    if (!state) {
        log_error("Game state is not null??");
        return NULL;
    }
    if (id >= MAX_ENTITIES) {
        log_error("Entity ID out of bounds: %d", id);
        return NULL;
    }
    if (!entity_has_flag(&state->entities[id], ENTITY_FLAG_ALLOCATED)) return NULL;
    return &state->entities[id];
}
entity_def_t* entity_get_def(game_state_t* state, entity_type_e type) {
    if (!state) {
        log_error("Game state is not null??");
        return NULL;
    }
    if (type >= 0 && type < ENTITY_TYPE_COUNT) {
        return &state->entity_defs[type];
    }
    return NULL;
}
void entity_initialize(entity_t* entity, u32 id) {
    memset(entity, 0, sizeof(entity_t));
    entity->id = id;
    entity->scale = vec2(1.0f, 1.0f);
    entity_set_flag(entity, ENTITY_FLAG_ALLOCATED);
}
entity_t* entity_create(game_state_t* state) {
    if (!state) {
        log_error("Game state is not null??");
        return NULL;
    }

    // Check for an existing free entity slot
    for (size_t i = 0; i < state->entity_count; i++) {
        if (!entity_has_flag(&state->entities[i], ENTITY_FLAG_ALLOCATED)) {
            entity_t* entity = &state->entities[i];
            entity_initialize(entity, i);
            return entity;
        }
    }

    if (state->entity_count >= MAX_ENTITIES) {
        log_error("Max entities reached (%d)", MAX_ENTITIES);
        return NULL;
    }

    entity_t* entity = &state->entities[state->entity_count];
    entity_initialize(entity, state->entity_count);
    state->entity_count++;
    return entity;
}
void entity_destroy(game_state_t* state, entity_t* entity) {
    if (!state) {
        log_error("Game state is not null??");
        return;
    }
    if (!entity) return;
    if (!entity_has_flag(entity, ENTITY_FLAG_ALLOCATED)) {
        log_error("Entity %d is already destroyed", entity->id);
        return;
    }
    // Check if we have a destory function for this entity type
    entity_def_t* def = entity_get_def(state, entity->type);
    if (def && def->destroy) {
        def->destroy(state, entity);
    }
    // If the entity has a light, remove it
    if (entity->light_id != 0) {
        light_remove(state->lighting, entity->light_id);
    }
    entity_clear_flag(entity, ENTITY_FLAG_ALLOCATED);
}
void entity_reset_frame_data(entity_t* entity) {
    if (!entity) return;
    entity->last_tick_input = entity->frame.input_axis;
    entity->frame.input_axis = vec2(0, 0);
}
// ===========================================================================
// ===========================================================================
// ===========================================================================


// ===========================================================================
//                            MESSAGES (PLUG API)
// ===========================================================================
void message_queue_init(message_queue_t* queue) {
    memset(queue, 0, sizeof(message_queue_t));
    memset(queue->messages, 0, sizeof(message_t) * MAX_MESSAGES);
    queue->count = 0;
}

void message_init(message_state_t* state) {
    if (!state) {
        log_error("Message state is null");
        return;
    }
    message_queue_init(&state->message_queue_current);
    message_queue_init(&state->message_queue_next);
}
void message_swap(message_state_t* state) {
    if (!state) {
        log_error("Message state is null");
        return;
    }
    // Swap the current and next queues
    message_queue_t temp = state->message_queue_current;
    state->message_queue_current = state->message_queue_next;
    state->message_queue_next = temp;
    
    // Clear the next queue
    message_queue_init(&state->message_queue_next);
}
void message_process(game_state_t* state) {
    if (!state) {
        log_error("Game state is not null??");
        return;
    }

    // Process current messages
    for (u32 i = 0; i < state->message_state.message_queue_current.count; i++) {
        message_t* msg = &state->message_state.message_queue_current.messages[i];
        switch (msg->type) {
            case MESSAGE_TYPE_CREATE_ENTITY: {
                entity_def_t* def = entity_get_def(state, msg->create_entity.type);
                if (!def) {
                    log_error("Couldn't find an entity definition for type: %d", msg->create_entity.type);
                    break;
                }
                else if (def->create) {
                    def->create(state, &msg->create_entity);
                }
                break;
            }
            default:
                log_warn("Unhandled message type: %d", msg->type);
                break;
        }
    }
}
// ===========================================================================
// ===========================================================================
// ===========================================================================

// ===========================================================================
//                                 INPUT
// ===========================================================================
void input_event(input_state_t* state, sapp_event* event) {
    if (event->type < 0 || event->type >= _SAPP_EVENTTYPE_NUM) {
        log_error("Invalid event type: %d", event->type);
        return;
    }

    // Update mouse position
    state->mouse_pos = vec2(event->mouse_x - sapp_width()/2, sapp_height()/2 - event->mouse_y);
    state->mouse_delta = vec2(event->mouse_dx, event->mouse_dy);
    state->mouse_scroll = vec2(event->scroll_x, event->scroll_y);

    // Process events based on type
    switch (event->type) {
        case SAPP_EVENTTYPE_KEY_DOWN:
            if (!event->key_repeat) {
                // Key was just pressed
                state->keys[event->key_code].flags |= INPUT_STATE_FLAG_DOWN | INPUT_STATE_FLAG_JUST_PRESSED;
            } else {
                // Key is repeating
                state->keys[event->key_code].flags |= INPUT_STATE_FLAG_REPEAT;
            }
            break;
            
        case SAPP_EVENTTYPE_KEY_UP:
            // Key was just released
            state->keys[event->key_code].flags &= ~INPUT_STATE_FLAG_DOWN;
            state->keys[event->key_code].flags |= INPUT_STATE_FLAG_JUST_RELEASED;
            break;
            
        case SAPP_EVENTTYPE_MOUSE_DOWN:
            // Mouse button was just pressed
            state->mouse_buttons[event->mouse_button].flags |= INPUT_STATE_FLAG_DOWN | INPUT_STATE_FLAG_JUST_PRESSED;
            break;
            
        case SAPP_EVENTTYPE_MOUSE_UP:
            // Mouse button was just released
            state->mouse_buttons[event->mouse_button].flags &= ~INPUT_STATE_FLAG_DOWN;
            state->mouse_buttons[event->mouse_button].flags |= INPUT_STATE_FLAG_JUST_RELEASED;
            break;
            
        default:
            break;
    }

    // Store the event for reference
    if (event->type == SAPP_EVENTTYPE_KEY_DOWN || event->type == SAPP_EVENTTYPE_KEY_UP) {
        state->keys[event->key_code].event = *event;
    } else if (event->type == SAPP_EVENTTYPE_MOUSE_DOWN || event->type == SAPP_EVENTTYPE_MOUSE_UP) {
        state->mouse_buttons[event->mouse_button].event = *event;
    }
}
void input_reset_state_for_new_frame(input_state_t* state) {
    // Clear just_pressed and just_released flags, but keep down flags
    for (int i = 0; i < SAPP_MAX_KEYCODES; i++) {
        state->keys[i].flags &= ~(INPUT_STATE_FLAG_JUST_PRESSED | INPUT_STATE_FLAG_JUST_RELEASED | INPUT_STATE_FLAG_REPEAT);
    }
    for (int i = 0; i < SAPP_MAX_MOUSEBUTTONS; i++) {
        state->mouse_buttons[i].flags &= ~(INPUT_STATE_FLAG_JUST_PRESSED | INPUT_STATE_FLAG_JUST_RELEASED);
    }
    // Reset mouse delta and scroll
    state->mouse_delta = vec2(0, 0);
    state->mouse_scroll = vec2(0, 0);
}
bool key_down(input_state_t* state, sapp_keycode key) {
    return (state->keys[key].flags & INPUT_STATE_FLAG_DOWN) != 0;
}
bool key_just_pressed(input_state_t* state, sapp_keycode key) {
    return (state->keys[key].flags & INPUT_STATE_FLAG_JUST_PRESSED) != 0;
}
bool key_just_released(input_state_t* state, sapp_keycode key) {
    return (state->keys[key].flags & INPUT_STATE_FLAG_JUST_RELEASED) != 0;
}
bool key_repeat(input_state_t* state, sapp_keycode key) {
    return (state->keys[key].flags & INPUT_STATE_FLAG_REPEAT) != 0;
}
bool mouse_button_down(input_state_t* state, sapp_mousebutton button) {
    return (state->mouse_buttons[button].flags & INPUT_STATE_FLAG_DOWN) != 0;
}
bool mouse_button_just_pressed(input_state_t* state, sapp_mousebutton button) {
    return (state->mouse_buttons[button].flags & INPUT_STATE_FLAG_JUST_PRESSED) != 0;
}
bool mouse_button_just_released(input_state_t* state, sapp_mousebutton button) {
    return (state->mouse_buttons[button].flags & INPUT_STATE_FLAG_JUST_RELEASED) != 0;
}
void input(game_state_t* state) {
    // Toggle train/play
    if (key_just_pressed(&state->input_state, SAPP_KEYCODE_T)) {
        state->rl_state.training_mode = !state->rl_state.training_mode;
        log_info("Training mode: %s", 
                state->rl_state.training_mode ? "ON" : "OFF");
    }
    
    // Save model
    if (key_just_pressed(&state->input_state, SAPP_KEYCODE_K)) {
        if (rl_save_model(&state->rl_state, "pong_model.bin")) {
            log_info("Model saved successfully!");
        } else {
            log_error("Failed to save model");
        }
    }
    
    // Load model
    if (key_just_pressed(&state->input_state, SAPP_KEYCODE_L)) {
        if (rl_load_model(&state->rl_state, "pong_model.bin")) {
            log_info("Model loaded successfully!");
        } else {
            log_error("Failed to load model");
        }
    }
    
}

// ===========================================================================
// ===========================================================================
// ===========================================================================


// ===========================================================================
//                               GAME DRAWING
// ===========================================================================
void draw_entities_y_sorted(game_state_t* state, f64 alpha) {
    // Create array of indices
    size_t* indices = malloc(state->entity_count * sizeof(size_t));
    if (!indices) return;
    
    // Initialize indices
    for (size_t i = 0; i < state->entity_count; i++) {
        indices[i] = i;
    }
    
    int compare_indices(const void* a, const void* b) {
        size_t idx_a = *(const size_t*)a;
        size_t idx_b = *(const size_t*)b;
        entity_t* entity_a = &state->entities[idx_a];
        entity_t* entity_b = &state->entities[idx_b];
        
        // Same logic as above
        int a_should_draw = entity_has_flag(entity_a, ENTITY_FLAG_ALLOCATED) && 
                           !entity_has_flag(entity_a, ENTITY_FLAG_INVISIBLE);
        int b_should_draw = entity_has_flag(entity_b, ENTITY_FLAG_ALLOCATED) && 
                           !entity_has_flag(entity_b, ENTITY_FLAG_INVISIBLE);
        
        if (!a_should_draw && !b_should_draw) return 0;
        if (!a_should_draw) return 1;
        if (!b_should_draw) return -1;

        // Sort by y position (higher y drawn first)
        if (entity_a->pos.y > entity_b->pos.y) return -1;
        if (entity_a->pos.y < entity_b->pos.y) return 1;
        return 0;
    }
    
    // Sort indices
    qsort(indices, state->entity_count, sizeof(size_t), compare_indices);
    
    // Draw using sorted indices
    for (size_t i = 0; i < state->entity_count; i++) {
        entity_t* entity = &state->entities[indices[i]];
        if (!entity_has_flag(entity, ENTITY_FLAG_ALLOCATED) || entity_has_flag(entity, ENTITY_FLAG_INVISIBLE)) continue;
       
        switch (entity->type) {
            case ENTITY_TYPE_PADDLE_LEFT:
            case ENTITY_TYPE_PADDLE_RIGHT:
            case ENTITY_TYPE_BALL:
                draw_rect(entity->pos, entity->size, 
                        vec4(1, 1, 1, 1), PIVOT_CENTER_CENTER);
                break;
        }
    }
    
    free(indices);
}
void draw_game(game_state_t* state, f64 alpha) {
    // Draw entities
    draw_entities_y_sorted(state, alpha);

    // Draw UI
    // Reset camera transform for UI
    mat4_t old_camera = draw_frame.camera_xform;
    draw_frame.camera_xform = mat4_identity();

    // ... draw UI here ...
        // Mode indicator
    char mode_str[64];
    snprintf(mode_str, sizeof(mode_str), state->rl_state.training_mode ? "TRAINING MODE" : "PLAY MODE");
    vec2_t mode_size = measure_text(mode_str, FONT_ALAGARD, 1.0f);
    vec2_t mode_pos = vec2(-(mode_size.x / 2.0f), (float)(LF_GAME_HEIGHT / 2) - mode_size.y - 8.0f);
    draw_text(mode_pos, mode_str, FONT_ALAGARD, 1.0f);

    // Score
    char score_str[64];
    snprintf(score_str, sizeof(score_str), "%d - %d", 
             state->pong.score_left, state->pong.score_right);
    vec2_t score_size = measure_text(score_str, FONT_ALAGARD, 1.0f);
    vec2_t score_pos = vec2(-(score_size.x / 2.0f), (float)(LF_GAME_HEIGHT / 2) - score_size.y - 32.0f);
    draw_text(score_pos, score_str, FONT_ALAGARD, 1.0f);

    if (state->rl_state.training_mode) {
        // Training statistics display
        float left_margin = -(float)(LF_GAME_WIDTH / 2) + 10.0f;
        float top_y = (float)(LF_GAME_HEIGHT / 2) - 44.0f;
        float line_height = 16.0f;
        float scale = 0.7f;

        // MSE
        char mse_str[64];
        snprintf(mse_str, sizeof(mse_str), "Error: %f", state->rl_state.mse);
        draw_text(vec2(left_margin, top_y), mse_str, FONT_ALAGARD, scale);
        
        // Iteration count
        char iter_str[64];
        snprintf(iter_str, sizeof(iter_str), "Iteration: %zu", state->rl_state.iter);
        draw_text(vec2(left_margin, top_y - line_height), iter_str, FONT_ALAGARD, scale);
        
        // Epsilon (exploration rate)
        char eps_str[64];
        snprintf(eps_str, sizeof(eps_str), "Epsilon: %.4f", state->rl_state.exploration_epsilon);
        draw_text(vec2(left_margin, top_y - line_height * 2), eps_str, FONT_ALAGARD, scale);
        
        // Replay buffer size
        char buffer_str[64];
        snprintf(buffer_str, sizeof(buffer_str), "Buffer: %zu/%zu", 
                 state->rl_state.replay_buffer->count,
                 state->rl_state.replay_buffer->capacity);
        draw_text(vec2(left_margin, top_y - line_height * 3), buffer_str, FONT_ALAGARD, scale);
        
        // Next target update countdown
        size_t next_update = TARGET_UPDATE_EVERY - (state->rl_state.iter % TARGET_UPDATE_EVERY);
        char update_str[64];
        snprintf(update_str, sizeof(update_str), "Target Update: %zu", next_update);
        draw_text(vec2(left_margin, top_y - line_height * 4), update_str, FONT_ALAGARD, scale);
        
        // Network info
        char net_str[64];
        snprintf(net_str, sizeof(net_str), "Net: %d-%d-%d-%d", 
                 NN_INPUT_SIZE, NN_HL_SIZE, NN_HL_SIZE, NN_OUTPUT_SIZE);
        draw_text(vec2(left_margin, top_y - line_height * 5), net_str, FONT_ALAGARD, scale);
        
        // Hyperparameters
        char lr_str[64];
        snprintf(lr_str, sizeof(lr_str), "LR: %.4f | Gamma: %.2f", LEARNING_RATE, GAMMA);
        draw_text(vec2(left_margin, top_y - line_height * 6), lr_str, FONT_ALAGARD, scale);

        // Batch Size
        char bs_str[64];
        snprintf(bs_str, sizeof(bs_str), "Batch Size: %d", BATCH_SIZE);
        draw_text(vec2(left_margin, top_y - line_height * 7), bs_str, FONT_ALAGARD, scale);
    }
        
    // Controls hint
    char controls_str[128];
    snprintf(controls_str, sizeof(controls_str), "T: Toggle | K: Save | L: Load");
    vec2_t controls_size = measure_text(controls_str, FONT_ALAGARD, 0.7f);
    draw_text(vec2(-(controls_size.x / 2.0f), -(float)(LF_GAME_HEIGHT / 2) + 10.0f), 
                controls_str, FONT_ALAGARD, 0.7f);

    // Restore camera transform
    draw_frame.camera_xform = old_camera;
}
// ===========================================================================
// ===========================================================================
// ===========================================================================

// ===========================================================================
//                                 ENTITIES
// ===========================================================================

// ===========================================================================
//                              PADDLE (LEFT)
// ===========================================================================
entity_t* create_paddle_left(game_state_t* state, mt_create_entity_t* msg) {
    entity_t* paddle = entity_create(state);
    if (!paddle) return NULL;
    
    paddle->type = ENTITY_TYPE_PADDLE_LEFT;
    entity_set_flag(paddle, ENTITY_FLAG_PHYSICS);
    
    paddle->pos = vec2(-LF_GAME_WIDTH / 2 + 15, 0.0f);
    paddle->size = vec2(15.0f, 80.0f);
    paddle->speed = 200.0f;

    state->pong.paddle_left_id = paddle->id;
    
    return paddle;
}
void paddle_left_logic_tick(game_state_t* state, entity_t* paddle, double dt) {
    if (state->rl_state.training_mode) {
        entity_t* ball = entity_get_by_id(state, state->pong.ball_id);
        float iter = (float)state->rl_state.iter;

        if (iter < 5000.0f) {
            paddle->frame.input_axis.y = 0.0f;
        } else if (iter < 20000.0f) {
            if (!ball) return;
            float progress = (iter - 5000.0f) / 15000.0f;
            paddle->speed = 120.0f + progress * 80.0f;
            float target_y = ball->pos.y;
            float max_error = 120.0f * (1.0f - progress);
            target_y += rng_float(&state->rng, -max_error, max_error);
            float reaction_threshold = 35.0f - (progress * 15.0f);
            float mistake_chance = 0.6f - (progress * 0.4f);
            if (rng_float(&state->rng, 0.0f, 1.0f) < mistake_chance) {
                target_y = rng_float(&state->rng, -80.0f, 80.0f);
            }
            float diff = target_y - paddle->pos.y;
            if (diff > reaction_threshold) {
                paddle->frame.input_axis.y = 1.0f;
            } else if (diff < -reaction_threshold) {
                paddle->frame.input_axis.y = -1.0f;
            } else {
                paddle->frame.input_axis.y = 0.0f;
            }
        } else {
            if (!ball) return;
            paddle->speed = 200.0f;
            float target_y = ball->pos.y;
            if (rng_float(&state->rng, 0.0f, 1.0f) < 0.15f) {
                target_y += rng_float(&state->rng, -60.0f, 60.0f);
            }
            float diff = target_y - paddle->pos.y;
            if (diff > 18.0f) {
                paddle->frame.input_axis.y = 1.0f;
            } else if (diff < -18.0f) {
                paddle->frame.input_axis.y = -1.0f;
            } else {
                paddle->frame.input_axis.y = 0.0f;
            }
        }
    } else {
        paddle->speed = 300.0f;
        if (key_down(&state->input_state, SAPP_KEYCODE_W)) {
            paddle->frame.input_axis.y = 1.0f;
        }
        if (key_down(&state->input_state, SAPP_KEYCODE_S)) {
            paddle->frame.input_axis.y = -1.0f;
        }
    }

    float half_height = paddle->size.y * 0.5f;
    float screen_bound = LF_GAME_HEIGHT / 2;
    if (paddle->pos.y + half_height > screen_bound) {
        paddle->pos.y = screen_bound - half_height;
        paddle->vel.y = 0;
    }
    if (paddle->pos.y - half_height < -screen_bound) {
        paddle->pos.y = -screen_bound + half_height;
        paddle->vel.y = 0;
    }
}

// ===========================================================================
//                       PADDLE (RIGHT) AKA the model
// ===========================================================================
entity_t* create_paddle_right(game_state_t* state, mt_create_entity_t* msg) {
    entity_t* paddle = entity_create(state);
    if (!paddle) return NULL;
    
    paddle->type = ENTITY_TYPE_PADDLE_RIGHT;
    entity_set_flag(paddle, ENTITY_FLAG_PHYSICS);
    
    paddle->pos = vec2(LF_GAME_WIDTH / 2 - 15, 0.0f);
    paddle->size = vec2(15.0f, 80.0f);
    paddle->speed = 300.0f;

    state->pong.paddle_right_id = paddle->id;
    
    return paddle;
}
void paddle_right_logic_tick(game_state_t* state, entity_t* paddle, double dt) {
    entity_t* ball = entity_get_by_id(state, state->pong.ball_id);
    entity_t* opponent = entity_get_by_id(state, state->pong.paddle_left_id);
    
    // Build current state (use last known positions if entities missing)
    static rl_input_t last_state = {0};  // Persist across calls
    rl_input_t current_state = {0};
    
    if (ball && opponent) {
        current_state.ball_x = ball->pos.x / (LF_GAME_WIDTH / 2);
        current_state.ball_y = ball->pos.y / (LF_GAME_HEIGHT / 2);
        current_state.ball_vel_x = fmax(-1.0f, fmin(ball->vel.x / 600.0f, 1.0f));
        current_state.ball_vel_y = fmax(-1.0f, fmin(ball->vel.y / 600.0f, 1.0f));
        current_state.own_paddle_y = paddle->pos.y / (LF_GAME_HEIGHT / 2);
        current_state.opponent_paddle_y = opponent->pos.y / (LF_GAME_HEIGHT / 2);
        last_state = current_state;  // Save for next frame
    } else {
        // Use last known state if entities destroyed
        current_state = last_state;
    }

    // Calculate reward
    float reward = 0.0f;
    bool terminal = false;

    if (state->pong.frame.scored == 1) {
        terminal = true;
		reward = state->pong.last_hit == 1 ? 1.0f : 0.5f;
    }
    else if (state->pong.frame.scored == -1) {
        terminal = true;
        reward = -1.0f;
    }
    else if (state->pong.frame.hit) {
        reward = 0.5f;
    }

	if (terminal) state->pong.last_hit = 0;
    state->pong.frame.scored = 0;
    state->pong.frame.hit = 0;
    
    // ALWAYS call rl_tick, even when entities are missing
    rl_action_e action = rl_tick(&state->rl_state, &state->rng, reward, &current_state, terminal);
    
    // Only apply movement if paddle exists
    if (!ball || !opponent) return;
    
    float y_axis = 0.0f;
    switch (action) {
        case RL_ACTION_UP:   y_axis =  1.0f; break;
        case RL_ACTION_DOWN: y_axis = -1.0f; break;
        case RL_ACTION_NONE: y_axis =  0.0f; break;
        default: break;
    }

    paddle->frame.input_axis.y = y_axis;
    
    // Clamp to screen bounds
    float half_height = paddle->size.y * 0.5f;
    float screen_bound = LF_GAME_HEIGHT / 2;
    if (paddle->pos.y + half_height > screen_bound) {
        paddle->pos.y = screen_bound - half_height;
        paddle->vel.y = 0;
    }
    if (paddle->pos.y - half_height < -screen_bound) {
        paddle->pos.y = -screen_bound + half_height;
        paddle->vel.y = 0;
    }
}

// ===========================================================================
//                                  BALL
// ===========================================================================
entity_t* create_ball(game_state_t* state, mt_create_entity_t* msg) {
    entity_t* ball = entity_create(state);
    if (!ball) return NULL;

    ball->type = ENTITY_TYPE_BALL;
    entity_set_flag(ball, ENTITY_FLAG_PHYSICS);

    ball->pos = vec2(-5.0f, 5.0f);
    ball->size = vec2(10.0f, 10.0f);
    ball->speed = 350.0f;

    float angle;

    if (state->rl_state.training_mode) {
        float bias_prob = 0.0f;
        if (state->rl_state.iter < 30000) {
            bias_prob = 0.8f * (1.0f - (float)state->rl_state.iter / 30000.0f);
        }

        if (rng_float(&state->rng, 0.0f, 1.0f) < bias_prob/* && rng_int(&state->rng, 0, 1) != 0*/) {
            entity_t* agent_paddle = entity_get_by_id(state, state->pong.paddle_right_id);
            if (agent_paddle) {
                vec2_t dir = vec2_subtract(agent_paddle->pos, ball->pos);
                angle = atan2f(dir.y, dir.x);
                angle += rng_float(&state->rng, -C_PI/12*0.5f, C_PI/12*0.5f);
            } else {
                angle = rng_float(&state->rng, -C_PI/4, C_PI/4);
                if (rng_int(&state->rng, 0, 1)) angle += C_PI;
            }
        } else {
            angle = rng_float(&state->rng, -C_PI/4, C_PI/4);
            if (rng_int(&state->rng, 0, 1)) angle += C_PI;
        }
    } else {
        angle = rng_float(&state->rng, -C_PI/4, C_PI/4);
        if (rng_int(&state->rng, 0, 1)) angle += C_PI;
    }

    ball->mvt_ignore_friction = true;
    ball->vel = vec2(
        cosf(angle) * ball->speed,
        sinf(angle) * ball->speed
    );

    state->pong.ball_id = ball->id;

    return ball;
}

void ball_logic_tick(game_state_t* state, entity_t* ball, double dt) {
    float screen_bound_y = LF_GAME_HEIGHT / 2;
    
    // Bounce off top/bottom
    float half_height = ball->size.y * 0.5f;
    if (ball->pos.y + half_height > screen_bound_y || 
        ball->pos.y - half_height < -screen_bound_y) {
        ball->vel.y = -ball->vel.y;
        ball->pos.y = ball->pos.y > 0 ? 
            screen_bound_y - half_height : 
            -screen_bound_y + half_height;
    }

    float screen_bound_x = LF_GAME_WIDTH / 2;
    
    // Score detection
    if (ball->pos.x > screen_bound_x) {
        state->pong.score_left++;
        state->pong.frame.scored = -1;

        // Reset ball
        message_t msg = {0};
        msg.type = MESSAGE_TYPE_CREATE_ENTITY;
        msg.create_entity.type = ENTITY_TYPE_BALL;
        message_add(&state->message_state, msg);

        entity_destroy(state, ball);
    }
    else if (ball->pos.x < -screen_bound_x) {
        state->pong.score_right++;
        state->pong.frame.scored = 1;

        // Reset ball
        message_t msg = {0};
        msg.type = MESSAGE_TYPE_CREATE_ENTITY;
        msg.create_entity.type = ENTITY_TYPE_BALL;
        message_add(&state->message_state, msg);

        entity_destroy(state, ball);
    }
}

bool ball_physics_tick(game_state_t* state, entity_t* ball, entity_t* other, double dt) {
    // Collision with paddles
    if ((other->type == ENTITY_TYPE_PADDLE_LEFT || 
         other->type == ENTITY_TYPE_PADDLE_RIGHT) && 
        entity_collision(ball, other)) {


		state->pong.last_hit = other->type == ENTITY_TYPE_PADDLE_LEFT ? -1 : 1;

        if (other->type == ENTITY_TYPE_PADDLE_RIGHT) {
            state->pong.frame.hit = true;
        }
        
        // Reverse X velocity
        ball->vel.x = -ball->vel.x;
        
        // Add spin based on where ball hits paddle
        float paddle_center = other->pos.y;
        float hit_offset = (ball->pos.y - paddle_center) / (other->size.y * 0.5f);
        ball->vel.y += hit_offset * 100.0f;
        
        // Move ball outside paddle to prevent double-collision
        if (other->type == ENTITY_TYPE_PADDLE_LEFT) {
            ball->pos.x = other->pos.x + other->size.x * 0.5f + ball->size.x * 0.5f + 1.0f;
        } else {
            ball->pos.x = other->pos.x - other->size.x * 0.5f - ball->size.x * 0.5f - 1.0f;
        }
        
        // Slight speed increase for challenge
        float speed = vec2_length(ball->vel);
        speed *= 1.05f;
        speed = fminf(speed, 600.0f);  // Cap max speed
        ball->vel = vec2_scale(vec2_normalize(ball->vel), speed);
        
        return true;
    }
    
    return false;
}

// ===========================================================================
//                      ENTITY DEFINITION FUNCTIONS
// ===========================================================================
void define_entities(game_state_t* state) {
    state->entity_defs[ENTITY_TYPE_PADDLE_LEFT] = (entity_def_t){
        .type = ENTITY_TYPE_PADDLE_LEFT,
        .create = create_paddle_left,
        .logic_tick = paddle_left_logic_tick,
        .physics_tick = NULL,
        .max_active = 1
    };
    
    state->entity_defs[ENTITY_TYPE_PADDLE_RIGHT] = (entity_def_t){
        .type = ENTITY_TYPE_PADDLE_RIGHT,
        .create = create_paddle_right,
        .logic_tick = paddle_right_logic_tick,
        .physics_tick = NULL,
        .max_active = 1
    };
    
    state->entity_defs[ENTITY_TYPE_BALL] = (entity_def_t){
        .type = ENTITY_TYPE_BALL,
        .create = create_ball,
        .logic_tick = ball_logic_tick,
        .physics_tick = ball_physics_tick,
        .max_active = 1
    };
}
// ===========================================================================

// ===========================================================================
//                                 GAME STATE
// ===========================================================================
void game_state_init(game_state_t* state, u32 rng_seed) {
    if (!state) {
        log_error("Game state is null??");
        return;
    }
    memset(state->entities, 0, sizeof(entity_t) * MAX_ENTITIES);
    state->entity_count = 0;
    state->player_id = -1; // No player created yet
    message_queue_init(&state->message_state.message_queue_current);
    message_queue_init(&state->message_state.message_queue_next);
    
    // Initialize random number generator
    rng_init(&state->rng, rng_seed);

    state->rl_state = *rl_initialize(&state->rng);

    // Define entity types
    define_entities(state);

    // Ambient lighting
    state->ambient_color = vec3(0.95f, 0.95f, 0.95f);
    state->ambient_intensity = 1.0f;
}
void start_game(game_state_t* state) {
    // Reset scores
    state->pong.score_left  = 0;
    state->pong.score_right = 0;
    state->pong.last_hit    = 0;
    
    // Spawn paddles
    message_t left_paddle = {0};
    left_paddle.type = MESSAGE_TYPE_CREATE_ENTITY;
    left_paddle.create_entity.type = ENTITY_TYPE_PADDLE_LEFT;
    message_add(&state->message_state, left_paddle);
    
    message_t right_paddle = {0};
    right_paddle.type = MESSAGE_TYPE_CREATE_ENTITY;
    right_paddle.create_entity.type = ENTITY_TYPE_PADDLE_RIGHT;
    message_add(&state->message_state, right_paddle);
    
    // Spawn ball
    message_t ball = {0};
    ball.type = MESSAGE_TYPE_CREATE_ENTITY;
    ball.create_entity.type = ENTITY_TYPE_BALL;
    message_add(&state->message_state, ball);
}
// ===========================================================================

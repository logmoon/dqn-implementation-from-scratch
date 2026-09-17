typedef struct {
    float ball_x;
    float ball_y;
    float ball_vel_x;
    float ball_vel_y;
    float own_paddle_y;
    float opponent_paddle_y;
} rl_input_t;

typedef enum {
    RL_ACTION_NONE = 0,
    RL_ACTION_UP = 1,
    RL_ACTION_DOWN = 2,
    RL_ACTION_COUNT = 3
} rl_action_e;

#define NN_INPUT_SIZE 6
#define NN_HL_SIZE 64
#define NN_OUTPUT_SIZE RL_ACTION_COUNT

typedef enum {
    ACTIVATION_RELU,
    ACTIVATION_SIGMOID,
    ACTIVATION_LINEAR,
} activation_e;

float activation_func(float in, activation_e activation) {
    float out = 0.0f;
    switch (activation)
    {
    case ACTIVATION_RELU:
        out = in > 0.0f ? in : 0.0f; // max(0, in)
        break;
    case ACTIVATION_SIGMOID:
        out = 1.0f/(1.0f+expf(-in)); // 1 / (1 + e^(-in))
        break;
    case ACTIVATION_LINEAR:
        out = in; // Keep it as is lol
        break;
    default:
        break;
    }
    return out;
}
float derivative_of_activation_func(float in, activation_e activation) {
    float out = 0.0f;
    switch (activation)
    {
    case ACTIVATION_RELU:
        out = in > 0.0f ? 1.0f : 0.0f;
        break;
    case ACTIVATION_SIGMOID:
        float sig = 1.0f/(1.0f+expf(-in));
        out = sig * (1.0f - sig);
        break;
    case ACTIVATION_LINEAR:
        out = 1.0f;
        break;
    default:
        break;
    }
    return out;
}

typedef struct {
    float preactivation; // Z      -> sum(input*weight)
    float activation;    // afn(Z) -> activation function of Z
} neuron_t;

typedef struct {
    float input[NN_INPUT_SIZE];
    float in_hl1_weights[NN_INPUT_SIZE*NN_HL_SIZE];
    float hl1_biases[NN_HL_SIZE];
    neuron_t neurons_hl1[NN_HL_SIZE];
    float hl1_hl2_weights[NN_HL_SIZE*NN_HL_SIZE];
    float hl2_biases[NN_HL_SIZE];
    neuron_t neurons_hl2[NN_HL_SIZE];
    float hl2_out_weights[NN_HL_SIZE*NN_OUTPUT_SIZE];
    float out_biases[NN_OUTPUT_SIZE];
    neuron_t output[NN_OUTPUT_SIZE];
} nn_t;

float neuron_process(neuron_t* neuron, float bias, float* weights, float* inputs, int count, activation_e activation) {
    float sum = bias;
    for (int i = 0; i < count; ++i) {
        sum += weights[i] * inputs[i];
    }
    neuron->preactivation = sum;

    float act = sum;
    switch (activation) {
        case ACTIVATION_RELU:
            act = sum > 0.0f ? sum : 0.0f; // max(0, sum)
            break;
        case ACTIVATION_SIGMOID:
            act = 1.0f/(1.0f+expf(-sum)); // 1 / (1 + e^(-sum))
            break;
        case ACTIVATION_LINEAR:
            act = sum; // Keep it as is lol
            break;
        default:
            log_error("Unknown activation function passed: %d. Using Linear activation.", activation);
            break;
    }

    neuron->activation = act;
    return act;
}

nn_t* nn_initialize(rng_state_t* rng) {
    nn_t* nn = malloc(sizeof(nn_t));
    memset(nn, 0, sizeof(nn_t));

    // Initialize weights for input -> hidden layer 1
    for (int i = 0; i < NN_INPUT_SIZE * NN_HL_SIZE; ++i) {
        // Xavier/He initialization: random between -sqrt(6/n) and +sqrt(6/n)
        // Apparently it's good for ts
        float limit = sqrtf(6.0f / (NN_INPUT_SIZE + NN_HL_SIZE));
        nn->in_hl1_weights[i] = rng_float(rng, 0.0f, 1.0f) * 2 * limit - limit;
    }
    
    // Initialize biases for hidden layer 1
    for (int i = 0; i < NN_HL_SIZE; ++i) {
        nn->hl1_biases[i] = 0.0f;  // Start biases at zero
    }

    // Initialize weights for hidden1 -> hidden2
    for (int i = 0; i < NN_HL_SIZE * NN_HL_SIZE; ++i) {
        float limit = sqrtf(6.0f / (NN_HL_SIZE + NN_HL_SIZE));
        nn->hl1_hl2_weights[i] = rng_float(rng, 0.0f, 1.0f) * 2 * limit - limit;
    }
    
    // Initialize biases for hidden layer 2
    for (int i = 0; i < NN_HL_SIZE; ++i) {
        nn->hl2_biases[i] = 0.0f;
    }

    // Initialize weights for hidden2 -> output
    for (int i = 0; i < NN_HL_SIZE * NN_OUTPUT_SIZE; ++i) {
        float limit = sqrtf(6.0f / (NN_HL_SIZE + NN_OUTPUT_SIZE));
        nn->hl2_out_weights[i] = rng_float(rng, 0.0f, 1.0f) * 2 * limit - limit;
    }
    
    // Initialize biases for output layer
    for (int i = 0; i < NN_OUTPUT_SIZE; ++i) {
        nn->out_biases[i] = 0.0f;
    }

    return nn;
}
void nn_free(nn_t* neural_network) {
    free(neural_network);
}

void forward_prop(nn_t* neural_network, rl_input_t state) {
    // Populate inputs from the state
    neural_network->input[0] = state.ball_x;
    neural_network->input[1] = state.ball_y;
    neural_network->input[2] = state.ball_vel_x;
    neural_network->input[3] = state.ball_vel_y;
    neural_network->input[4] = state.own_paddle_y;
    neural_network->input[5] = state.opponent_paddle_y;

    // Input -> Hidden Layer 1
    float hl1_out[NN_HL_SIZE];
    for (int i = 0; i < NN_HL_SIZE; ++i) {
        neuron_t* neuron = &neural_network->neurons_hl1[i];
        float bias = neural_network->hl1_biases[i];
        // Neuron i uses weights from index [i * NN_INPUT_SIZE] to [(i+1) * NN_INPUT_SIZE - 1]
        float* weights = &neural_network->in_hl1_weights[i * NN_INPUT_SIZE];
        hl1_out[i] = neuron_process(neuron, bias, weights, neural_network->input, NN_INPUT_SIZE, ACTIVATION_RELU);
    }

    // Hidden Layer 1 -> Hidden Layer 2
    float hl2_out[NN_HL_SIZE];
    for (int i = 0; i < NN_HL_SIZE; ++i) {
        neuron_t* neuron = &neural_network->neurons_hl2[i];
        float bias = neural_network->hl2_biases[i];
        float* weights = &neural_network->hl1_hl2_weights[i * NN_HL_SIZE];
        hl2_out[i] = neuron_process(neuron, bias, weights, hl1_out, NN_HL_SIZE, ACTIVATION_RELU);
    }

    // Hidden Layer 2 -> Output Layer
    for (int i = 0; i < NN_OUTPUT_SIZE; ++i) {
        neuron_t* neuron = &neural_network->output[i];
        float bias = neural_network->out_biases[i];
        float* weights = &neural_network->hl2_out_weights[i * NN_HL_SIZE];
        
        // Output will be cached in the output neurons
        neuron_process(neuron, bias, weights, hl2_out, NN_HL_SIZE, ACTIVATION_LINEAR);
    }
}

#define REPLAY_BUFFER_MAX_SIZE 150000

typedef struct {
    rl_input_t current;
    rl_action_e action;
    float reward;
    rl_input_t next;
    bool terminal;
} rb_entry_t;

typedef struct {
    rb_entry_t* entries;
    size_t count;
    size_t capacity;
    size_t write_index;
} rb_t;

rb_t* rb_initialize_with_capacity(size_t capacity) {
    rb_t* rb = malloc(sizeof(rb_t));
    memset(rb, 0, sizeof(rb_t));
    rb->capacity = capacity;

    rb->entries = malloc(sizeof(rb_entry_t) * rb->capacity);
    memset(rb->entries, 0, sizeof(rb_entry_t) * rb->capacity);

    rb->count = 0;
    rb->write_index = 0;

    return rb;
}
rb_t* rb_initialize() {
    return rb_initialize_with_capacity(REPLAY_BUFFER_MAX_SIZE);
}
void rb_add(rb_t* rb, rb_entry_t entry) {
    if (rb == NULL) {
        log_error("Replay buffer is null");
        return;
    }

    // Write to current position
    rb->entries[rb->write_index] = entry;
    
    // Move write index forward (circular)
    rb->write_index = (rb->write_index + 1) % rb->capacity;
    
    // Update count (max out at capacity)
    if (rb->count < rb->capacity) {
        rb->count++;
    }
}
rb_t* rb_get_batch(rb_t* src, int batch_size, rng_state_t* rng) {
    rb_t* batch = rb_initialize_with_capacity(batch_size);
    
    // Track which indices we've already picked
    size_t size = sizeof(bool) * src->count;
    bool* picked = malloc(size);
    memset(picked, 0, size);
    
    int attempts = 0;
    int max_attempts = batch_size * 100;

    while (batch->count < batch_size && attempts < max_attempts) {
        int max_idx = src->count > 1 ? src->count - 1 : 1;
        int idx = rng_int(rng, 0, max_idx - 1);
        
        // Skip if already picked ts
        if (picked[idx]) {
            attempts++;
            continue;
        }
        
        rb_add(batch, src->entries[idx]);
        picked[idx] = true;
        attempts = 0;
    }
    
    free(picked);

    return batch;
}
void rb_free(rb_t* rb) {
    free(rb->entries);
    rb->count = 0;
    rb->capacity = 0;
    free(rb);
}

// Hyperparams
#define LEARNING_RATE       0.001
#define GAMMA               0.99
#define BATCH_SIZE          64
#define LEARNING_STARTS     1000
#define TRAIN_EVERY         2
#define TARGET_UPDATE_EVERY 1000
#define GRAD_CLIP_VALUE     1.0f

#define EPSILON_START       1.0f
#define EPSILON_END         0.05f
#define EPSILON_DECAY       0.9999f
float get_epsilon(size_t iter) {
    float eps = EPSILON_START * powf(EPSILON_DECAY, (float)iter);
    return eps > EPSILON_END ? eps : EPSILON_END;
}

typedef struct {
    nn_t* main_network;
    nn_t* target_network;
    rb_t* replay_buffer;
    size_t iter;
    float exploration_epsilon;
    bool training_mode;
    float mse;
} rl_state_t;

rl_state_t* rl_initialize(rng_state_t* rng) {
    rl_state_t* state = malloc(sizeof(rl_state_t));
    memset(state, 0, sizeof(rl_state_t));

    // Create our main network and our target
    state->main_network = nn_initialize(rng);
    state->target_network = malloc(sizeof(nn_t));
    memcpy(state->target_network, state->main_network, sizeof(nn_t));

    // Create the replay buffer
    state->replay_buffer = rb_initialize();

    state->iter = 0;
    state->exploration_epsilon = 1.0f;
    state->training_mode = true; // Start in training mode
    state->mse = 69.69f;

    return state;
}

rl_action_e rl_select_action(nn_t* nn, rl_input_t input, float epsilon, rng_state_t* rng) {
    if (rng_float(rng, 0.0f, 1.0f) < epsilon) {
        // Explore: random action
        return rng_int(rng, 0, RL_ACTION_COUNT - 1);
    } else {
        // Exploit: best action
        forward_prop(nn, input);
        rl_action_e best_action = RL_ACTION_NONE;
        float best_q = nn->output[0].activation;
        for (int i = 1; i < NN_OUTPUT_SIZE; ++i) {
            if (nn->output[i].activation > best_q) {
                best_q = nn->output[i].activation;
                best_action = (rl_action_e)i;
            }
        }
        return best_action;
    }
}

float clip_gradient(float grad, float max_val) {
    if (grad > max_val) return max_val;
    if (grad < -max_val) return -max_val;
    return grad;
}

// =======================================================================================
// Backprop and Gradient Descent:
// d(loss)/d(w_i,l) = d(loss)/d(act_i,l) * d(act_i,l)/d(pre_i,l) * d(pre_i,l)/d(w_i,l)
// var_i,l = d(loss)/d(act_i,l) * d(act_i,l)/d(pre_i,l)
// => d(loss)/d(w_i,l) = var_i,l * d(pre_i,l)/d(w_i,l)
// For bias, we just replace w_i,l with b_i,l to get:
// => d(loss)/d(b_i,l) = var_i,l * d(pre_i,l)/d(b_i,l)
// To propagate that var_i,l in layer l to layer l-1, and so on:
// var_i,l = sum(var_i,l+1 * w_i,l+1) * d(act_i,l)/d(pre_i,l)
// To update the weights and biases:
// w_i,l = w_i,l - lr * d(loss)/d(w_i,l)
// b_i,l = b_i,l - lr * d(loss)/d(b_i,l)
// Open it up:
// w_i,l = w_i,l - lr * var_i,l * d(pre_i,l)/d(w_i,l)
// b_i,l = b_i,l - lr * var_i,l * d(pre_i,l)/d(b_i,l)
// With; lr: learning rate
// =======================================================================================
float backprop_and_update(nn_t* main_nn, nn_t* target_nn, rb_t* batch) {
    // Cache for the gradients:
    // Starting from the end and walking back:
    size_t batch_size = batch->count;

    if (batch_size <= 0) {
        return -1.0f;
    }
    
    // OUTPUT
    int grad_output_biases_size = NN_OUTPUT_SIZE;
    int grad_output_weights_size = NN_OUTPUT_SIZE * NN_HL_SIZE;
    float* grad_output_biases = calloc(NN_OUTPUT_SIZE * batch_size, sizeof(float));
    float* grad_output_weights = calloc(NN_OUTPUT_SIZE * NN_HL_SIZE * batch_size, sizeof(float));
    
    // HIDDEN LAYER 2
    int grad_hl2_biases_size = NN_HL_SIZE;
    int grad_hl2_weights_size = NN_HL_SIZE * NN_HL_SIZE;
    float* grad_hl2_biases = calloc(NN_HL_SIZE * batch_size, sizeof(float));
    float* grad_hl2_weights = calloc(NN_HL_SIZE * NN_HL_SIZE * batch_size, sizeof(float));
    
    // HIDDEN LAYER 1
    int grad_hl1_biases_size = NN_HL_SIZE;
    int grad_hl1_weights_size = NN_HL_SIZE * NN_INPUT_SIZE;
    float* grad_hl1_biases = calloc(NN_HL_SIZE * batch_size, sizeof(float));
    float* grad_hl1_weights = calloc(NN_HL_SIZE * NN_INPUT_SIZE * batch_size, sizeof(float));
    
    // Lil cache for the variations
    int variations_size = NN_OUTPUT_SIZE + NN_HL_SIZE + NN_HL_SIZE;
    float* variations = calloc((NN_OUTPUT_SIZE + NN_HL_SIZE + NN_HL_SIZE) * batch_size, sizeof(float));
    
    // Check if all allocations succeeded
    if (!grad_output_biases || !grad_output_weights || 
        !grad_hl2_biases || !grad_hl2_weights ||
        !grad_hl1_biases || !grad_hl1_weights || !variations) {

        // Clean up
        free(grad_output_biases);
        free(grad_output_weights);
        free(grad_hl2_biases);
        free(grad_hl2_weights);
        free(grad_hl1_biases);
        free(grad_hl1_weights);
        free(variations);

        log_critical("Couldn't allocate cache for back propagation, buy more ram lol");
        return -1.0f;
    }

    // OUTPUT
    float* avg_grad_output_biases = calloc(grad_output_biases_size, sizeof(float));
    float* avg_grad_output_weights = calloc(grad_output_weights_size, sizeof(float));
    
    // HIDDEN LAYER 2
    float* avg_grad_hl2_biases = calloc(grad_hl2_biases_size, sizeof(float));
    float* avg_grad_hl2_weights = calloc(grad_hl2_weights_size, sizeof(float));
    
    // HIDDEN LAYER 1
    float* avg_grad_hl1_biases = calloc(grad_hl1_biases_size, sizeof(float));
    float* avg_grad_hl1_weights = calloc(grad_hl1_weights_size, sizeof(float));
    
    float sum_of_loss = 0;
    
    for (size_t i = 0; i < batch->count; ++i) {
        rb_entry_t* entry = &batch->entries[i];

        // Get the target
        forward_prop(target_nn, entry->next);
        float max_next_q = target_nn->output[0].activation;
        for (size_t j = 1; j < NN_OUTPUT_SIZE; ++j) {
            if (target_nn->output[j].activation > max_next_q) {
                max_next_q = target_nn->output[j].activation;
            }
        }
        float target = entry->reward + (entry->terminal ? 0.0f : GAMMA * max_next_q);

        // Get the predicted
        forward_prop(main_nn, entry->current);
        float predicted = main_nn->output[entry->action].activation;

        float td_error = predicted - target;

        float huber_delta = 1.0f;
        float loss, loss_derivative;
        if (fabsf(td_error) <= huber_delta) {
            loss = 0.5f * td_error * td_error;
            loss_derivative = td_error;
        } else {
            loss = huber_delta * (fabsf(td_error) - 0.5f * huber_delta);
            loss_derivative = huber_delta * (td_error > 0 ? 1.0f : -1.0f);
        }
        sum_of_loss += loss;

        // We basically need to find that var_i,l.

        // Then using that we compute this d(loss)/d(w_i,l) which is the gradient for the weights
        // and this d(loss)/d(b_i,l) which is the gradient for the biases.

        // We also need to cache em.
    
        // Let's calculate the variations
        // var = derivative_of_loss_func * derivative_of_activation_func_wr_preactivation
        // This is for output layer, we backprop to get other variations like discussed above
        size_t current_layer_padding = i*variations_size;
        size_t next_layer_padding = i*variations_size;
        for (size_t j = 0; j < NN_OUTPUT_SIZE; ++j) {
            if (j == entry->action) {
                variations[j+current_layer_padding] = loss_derivative *
                    derivative_of_activation_func(main_nn->output[j].preactivation, ACTIVATION_LINEAR);
            } else {
                variations[j+current_layer_padding] = 0.0f;  // No gradient for other actions
            }
        }
        current_layer_padding += NN_OUTPUT_SIZE;
        next_layer_padding += 0;
        for (size_t j = 0; j < NN_HL_SIZE; ++j) {
            float sum = 0;
            for (size_t k = 0; k < NN_OUTPUT_SIZE; ++k) {
                sum += variations[k+next_layer_padding] * main_nn->hl2_out_weights[k * NN_HL_SIZE + j];
            }
            variations[j+current_layer_padding] = sum * derivative_of_activation_func(main_nn->neurons_hl2[j].preactivation, ACTIVATION_RELU);
        }
        current_layer_padding += NN_HL_SIZE;
        next_layer_padding += NN_OUTPUT_SIZE;
        for (size_t j = 0; j < NN_HL_SIZE; ++j) {
            float sum = 0;
            for (size_t k = 0; k < NN_HL_SIZE; ++k) {
                sum += variations[k+next_layer_padding] * main_nn->hl1_hl2_weights[k * NN_HL_SIZE + j];
            }
            variations[j+current_layer_padding] = sum * derivative_of_activation_func(main_nn->neurons_hl1[j].preactivation, ACTIVATION_RELU);
        }

        // Let's calculate gradients
        // OUTPUT
        current_layer_padding = i*variations_size;
        for (size_t k = 0; k < NN_OUTPUT_SIZE; ++k) {
            for (size_t j = 0; j < NN_HL_SIZE; ++j) {
                grad_output_weights[j + (k*NN_HL_SIZE) + i*grad_output_weights_size] = variations[k] * main_nn->neurons_hl2[j].activation;
                avg_grad_output_weights[j + (k*NN_HL_SIZE)] += grad_output_weights[j + (k*NN_HL_SIZE) + i*grad_output_weights_size];
            }
            grad_output_biases[k + i*grad_output_biases_size] = variations[k];
            avg_grad_output_biases[k] += grad_output_biases[k + i*grad_output_biases_size];
        }
        current_layer_padding += NN_OUTPUT_SIZE;
        // HIDDEN LAYER 2
        for (size_t k = 0; k < NN_HL_SIZE; ++k) {
            for (size_t j = 0; j < NN_HL_SIZE; ++j) {
                grad_hl2_weights[j + (k*NN_HL_SIZE) + i*grad_hl2_weights_size] = variations[k+current_layer_padding] * main_nn->neurons_hl1[j].activation;
                avg_grad_hl2_weights[j + (k*NN_HL_SIZE)] += grad_hl2_weights[j + (k*NN_HL_SIZE) + i*grad_hl2_weights_size];
            }
            grad_hl2_biases[k + i*grad_hl2_biases_size] = variations[k+current_layer_padding];
            avg_grad_hl2_biases[k] += grad_hl2_biases[k + i*grad_hl2_biases_size];
        }
        current_layer_padding += NN_HL_SIZE;
        // HIDDEN LAYER 1
        for (size_t k = 0; k < NN_HL_SIZE; ++k) {
            for (size_t j = 0; j < NN_INPUT_SIZE; ++j) {
                grad_hl1_weights[j + (k*NN_INPUT_SIZE) + i*grad_hl1_weights_size] = variations[k+current_layer_padding] * main_nn->input[j];
                avg_grad_hl1_weights[j + (k*NN_INPUT_SIZE)] += grad_hl1_weights[j + (k*NN_INPUT_SIZE) + i*grad_hl1_weights_size];
            }
            grad_hl1_biases[k + i*grad_hl1_biases_size] = variations[k+current_layer_padding];
            avg_grad_hl1_biases[k] += grad_hl1_biases[k + i*grad_hl1_biases_size];
        }
    }
    
    // Now we have all the gradients
    // We average them over all samples in the batch
    // Then we update all the weights and biases
    // OUTPUT LAYER
    for (size_t i = 0; i < grad_output_biases_size; ++i) {
        avg_grad_output_biases[i] = clip_gradient(avg_grad_output_biases[i] / batch->count, GRAD_CLIP_VALUE);
        main_nn->out_biases[i] -= LEARNING_RATE * avg_grad_output_biases[i];
    }
    for (size_t i = 0; i < grad_output_weights_size; ++i) {
        avg_grad_output_weights[i] = clip_gradient(avg_grad_output_weights[i] / batch->count, GRAD_CLIP_VALUE);
        main_nn->hl2_out_weights[i] -= LEARNING_RATE * avg_grad_output_weights[i];
    }
    // HIDDEN LAYER 2
    for (size_t i = 0; i < grad_hl2_biases_size; ++i) {
        avg_grad_hl2_biases[i] = clip_gradient(avg_grad_hl2_biases[i] / batch->count, GRAD_CLIP_VALUE);
        main_nn->hl2_biases[i] -= LEARNING_RATE * avg_grad_hl2_biases[i];
    }
    for (size_t i = 0; i < grad_hl2_weights_size; ++i) {
        avg_grad_hl2_weights[i] = clip_gradient(avg_grad_hl2_weights[i] / batch->count, GRAD_CLIP_VALUE);
        main_nn->hl1_hl2_weights[i] -= LEARNING_RATE * avg_grad_hl2_weights[i];
    }
    // HIDDEN LAYER 1
    for (size_t i = 0; i < grad_hl1_biases_size; ++i) {
        avg_grad_hl1_biases[i] = clip_gradient(avg_grad_hl1_biases[i] / batch->count, GRAD_CLIP_VALUE);
        main_nn->hl1_biases[i] -= LEARNING_RATE * avg_grad_hl1_biases[i];
    }
    for (size_t i = 0; i < grad_hl1_weights_size; ++i) {
        avg_grad_hl1_weights[i] = clip_gradient(avg_grad_hl1_weights[i] / batch->count, GRAD_CLIP_VALUE);
        main_nn->in_hl1_weights[i] -= LEARNING_RATE * avg_grad_hl1_weights[i];
    }


    float mean_error = sum_of_loss / batch->count; // Average the errors to get mean squared error

    // Clean up
    free(grad_output_biases);
    free(grad_output_weights);
    free(grad_hl2_biases);
    free(grad_hl2_weights);
    free(grad_hl1_biases);
    free(grad_hl1_weights);
    free(variations);
    free(avg_grad_output_biases);
    free(avg_grad_output_weights);
    free(avg_grad_hl2_biases);
    free(avg_grad_hl2_weights);
    free(avg_grad_hl1_biases);
    free(avg_grad_hl1_weights);

    return mean_error;
}

rl_action_e rl_tick(rl_state_t* state, rng_state_t* rng, float reward, rl_input_t* current, bool terminal) {

    if (state->training_mode) {
        if (state->iter > 0) {
            // First, handle last tick
            size_t prev_write_idx = (state->replay_buffer->write_index + state->replay_buffer->capacity - 1) 
                            % state->replay_buffer->capacity;
            rb_entry_t* last_tick_entry = &state->replay_buffer->entries[prev_write_idx];
            if (last_tick_entry != NULL) {
                last_tick_entry->next = *current;
                last_tick_entry->reward = reward;
                last_tick_entry->terminal = terminal;  // Mark if episode ended
            } else {
                log_error("Previous tick's replay buffer entry is null?");
            }
        }
    }

    // Play
    float epsilon = state->training_mode ? state->exploration_epsilon : 0.0f;
    rl_action_e chosen_action = rl_select_action(state->main_network, *current, epsilon, rng);

    if (state->training_mode) {
        rb_entry_t this_tick_entry = {0};
        this_tick_entry.action = chosen_action;
        this_tick_entry.current = *current;
        rb_add(state->replay_buffer, this_tick_entry);

        if (state->replay_buffer->count > LEARNING_STARTS) {
            // Train
            // Check if we should train this tick
            if (state->iter > 0 && state->iter % TRAIN_EVERY == 0) {
                // Decay exploration epsilon
                state->exploration_epsilon = get_epsilon(state->iter);

                rb_t* batch = rb_get_batch(state->replay_buffer, BATCH_SIZE, rng);
                state->mse = backprop_and_update(state->main_network, state->target_network, batch);

                rb_free(batch);
            }

            // Check to copy main to target
            if (state->iter > 0 && state->iter % TARGET_UPDATE_EVERY == 0) {
                memcpy(state->target_network, state->main_network, sizeof(nn_t));
                if (state->target_network == NULL) {
                    log_critical("failed to update target network");
                }
            }
            state->iter++;
        }
    }

    return chosen_action;
}

void rl_free(rl_state_t* state) {
    rb_free(state->replay_buffer);
    state->replay_buffer = NULL;
    nn_free(state->main_network);
    nn_free(state->target_network);
    state->main_network = NULL;
    state->target_network = NULL;
    state->iter = -1;
    free(state);
}

// Define a unique ID for your model files
#define MODEL_MAGIC 0x504F4E47 // "PONG" in hex
#define MODEL_VERSION 2 // Increment when changing format

// Helper macro for checked writes
#define CHECKED_WRITE(ptr, size, count, file) \
    if (fwrite(ptr, size, count, file) != count) { \
        log_error("Failed to write to file at line %d", __LINE__); \
        fclose(file); \
        return false; \
    }

// Helper macro for checked reads
#define CHECKED_READ(ptr, size, count, file) \
    if (fread(ptr, size, count, file) != count) { \
        log_error("Failed to read from file at line %d", __LINE__); \
        fclose(file); \
        return false; \
    }

bool rl_save_model(rl_state_t* state, const char* filepath) {
    FILE* f = fopen(filepath, "wb");
    if (!f) {
        log_error("Failed to open file for writing: %s", filepath);
        return false;
    }

    // 1. Write Header
    uint32_t magic = MODEL_MAGIC;
    uint32_t version = MODEL_VERSION;
    uint32_t hl_size = NN_HL_SIZE;
    uint32_t input_size = NN_INPUT_SIZE;
    uint32_t output_size = NN_OUTPUT_SIZE;
    
    CHECKED_WRITE(&magic, sizeof(uint32_t), 1, f);
    CHECKED_WRITE(&version, sizeof(uint32_t), 1, f);
    CHECKED_WRITE(&hl_size, sizeof(uint32_t), 1, f);
    CHECKED_WRITE(&input_size, sizeof(uint32_t), 1, f);
    CHECKED_WRITE(&output_size, sizeof(uint32_t), 1, f);

    // 2. Write RL State (to resume training)
    CHECKED_WRITE(&state->iter, sizeof(size_t), 1, f);
    CHECKED_WRITE(&state->exploration_epsilon, sizeof(float), 1, f);
    
    // Save training mode flag
    uint8_t training_mode = state->training_mode ? 1 : 0;
    CHECKED_WRITE(&training_mode, sizeof(uint8_t), 1, f);

    // 3. Write Weights and Biases from main_network
    nn_t* nn = state->main_network;
    
    // Input -> HL1
    CHECKED_WRITE(nn->in_hl1_weights, sizeof(float), NN_INPUT_SIZE * NN_HL_SIZE, f);
    CHECKED_WRITE(nn->hl1_biases, sizeof(float), NN_HL_SIZE, f);
    
    // HL1 -> HL2
    CHECKED_WRITE(nn->hl1_hl2_weights, sizeof(float), NN_HL_SIZE * NN_HL_SIZE, f);
    CHECKED_WRITE(nn->hl2_biases, sizeof(float), NN_HL_SIZE, f);
    
    // HL2 -> Output
    CHECKED_WRITE(nn->hl2_out_weights, sizeof(float), NN_HL_SIZE * NN_OUTPUT_SIZE, f);
    CHECKED_WRITE(nn->out_biases, sizeof(float), NN_OUTPUT_SIZE, f);

    // 4. Write a simple checksum (sum of all weights for basic validation)
    float checksum = 0.0f;
    for (int i = 0; i < NN_INPUT_SIZE * NN_HL_SIZE; i++) 
        checksum += nn->in_hl1_weights[i];
    for (int i = 0; i < NN_HL_SIZE * NN_HL_SIZE; i++) 
        checksum += nn->hl1_hl2_weights[i];
    for (int i = 0; i < NN_HL_SIZE * NN_OUTPUT_SIZE; i++) 
        checksum += nn->hl2_out_weights[i];
    
    CHECKED_WRITE(&checksum, sizeof(float), 1, f);

    fclose(f);
    log_info("Saved model to %s (Iter: %zu, Eps: %.4f, Mode: %s)", 
             filepath, state->iter, state->exploration_epsilon,
             state->training_mode ? "TRAIN" : "PLAY");
    return true;
}

bool rl_load_model(rl_state_t* state, const char* filepath) {
    FILE* f = fopen(filepath, "rb");
    if (!f) {
        log_error("Failed to open model file: %s", filepath);
        return false;
    }

    // 1. Verify Magic Number and Version
    uint32_t magic, version;
    CHECKED_READ(&magic, sizeof(uint32_t), 1, f);
    
    if (magic != MODEL_MAGIC) {
        log_error("Invalid model file format! Got 0x%X, expected 0x%X", magic, MODEL_MAGIC);
        fclose(f);
        return false;
    }
    
    CHECKED_READ(&version, sizeof(uint32_t), 1, f);
    if (version != MODEL_VERSION) {
        log_error("Model version mismatch! File has v%u, code expects v%u", 
                  version, MODEL_VERSION);
        fclose(f);
        return false;
    }

    // 2. Check Architecture Compatibility
    uint32_t loaded_hl_size, loaded_input_size, loaded_output_size;
    CHECKED_READ(&loaded_hl_size, sizeof(uint32_t), 1, f);
    CHECKED_READ(&loaded_input_size, sizeof(uint32_t), 1, f);
    CHECKED_READ(&loaded_output_size, sizeof(uint32_t), 1, f);
    
    if (loaded_hl_size != NN_HL_SIZE) {
        log_error("Hidden layer size mismatch! File: %u, Code: %u", 
                  loaded_hl_size, NN_HL_SIZE);
        fclose(f);
        return false;
    }
    if (loaded_input_size != NN_INPUT_SIZE) {
        log_error("Input size mismatch! File: %u, Code: %u", 
                  loaded_input_size, NN_INPUT_SIZE);
        fclose(f);
        return false;
    }
    if (loaded_output_size != NN_OUTPUT_SIZE) {
        log_error("Output size mismatch! File: %u, Code: %u", 
                  loaded_output_size, NN_OUTPUT_SIZE);
        fclose(f);
        return false;
    }

    // 3. Load State
    CHECKED_READ(&state->iter, sizeof(size_t), 1, f);
    CHECKED_READ(&state->exploration_epsilon, sizeof(float), 1, f);
    
    uint8_t training_mode;
    CHECKED_READ(&training_mode, sizeof(uint8_t), 1, f);
    state->training_mode = (training_mode != 0);

    // 4. Load Weights
    nn_t* nn = state->main_network;
    
    // Input -> HL1
    CHECKED_READ(nn->in_hl1_weights, sizeof(float), NN_INPUT_SIZE * NN_HL_SIZE, f);
    CHECKED_READ(nn->hl1_biases, sizeof(float), NN_HL_SIZE, f);
    
    // HL1 -> HL2
    CHECKED_READ(nn->hl1_hl2_weights, sizeof(float), NN_HL_SIZE * NN_HL_SIZE, f);
    CHECKED_READ(nn->hl2_biases, sizeof(float), NN_HL_SIZE, f);
    
    // HL2 -> Output
    CHECKED_READ(nn->hl2_out_weights, sizeof(float), NN_HL_SIZE * NN_OUTPUT_SIZE, f);
    CHECKED_READ(nn->out_biases, sizeof(float), NN_OUTPUT_SIZE, f);

    // 5. Verify Checksum
    float saved_checksum, computed_checksum = 0.0f;
    CHECKED_READ(&saved_checksum, sizeof(float), 1, f);
    
    for (int i = 0; i < NN_INPUT_SIZE * NN_HL_SIZE; i++) 
        computed_checksum += nn->in_hl1_weights[i];
    for (int i = 0; i < NN_HL_SIZE * NN_HL_SIZE; i++) 
        computed_checksum += nn->hl1_hl2_weights[i];
    for (int i = 0; i < NN_HL_SIZE * NN_OUTPUT_SIZE; i++) 
        computed_checksum += nn->hl2_out_weights[i];
    
    // Allow small floating point differences
    if (fabsf(saved_checksum - computed_checksum) > 0.01f) {
        log_error("Checksum mismatch! File may be corrupted. Saved: %.2f, Computed: %.2f", 
                  saved_checksum, computed_checksum);
        fclose(f);
        return false;
    }

    // 6. Sync Target Network
    memcpy(state->target_network, state->main_network, sizeof(nn_t));
    
    // 7. Clear replay buffer (old experiences are stale)
    if (state->replay_buffer) {
        state->replay_buffer->count = 0;
        state->replay_buffer->write_index = 0;
        log_info("Cleared replay buffer (stale experiences)");
    }

    fclose(f);
    log_info("Loaded model from %s", filepath);
    log_info("  Iter: %zu | Epsilon: %.4f | Mode: %s", 
             state->iter, state->exploration_epsilon,
             state->training_mode ? "TRAIN" : "PLAY");
    
    return true;
}

#undef CHECKED_WRITE
#undef CHECKED_READ

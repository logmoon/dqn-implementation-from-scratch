typedef struct {
    bool active;
    int audio_id;
    const int16_t* data; // interleaved int16 samples
    uint32_t frames; // number of frames
    double pos; // fractional frame position
    double step; // frames to advance per output frame (resampling)
    float volume; // 0..1
    float pan; // -1 (left) .. 1 (right)
    int channels; // source channels
    int sample_rate;
} audio_voice_t;

#define MAX_VOICES 64 // Previously 32

static audio_voice_t voices[MAX_VOICES];

static void mix_into(float* out, int num_frames) {
    // zero buffer
    int out_channels = 2;
    int total = num_frames * out_channels;
    for (int i = 0; i < total; i++) out[i] = 0.0f;

    for (int v = 0; v < (int)(sizeof(voices)/sizeof(voices[0])); v++) {
        audio_voice_t* voice = &voices[v];
        if (!voice->active) continue;
        for (int f = 0; f < num_frames; f++) {
            float left = 0.0f, right = 0.0f;
            if ((uint32_t)voice->pos >= voice->frames) { voice->active = false; break; }
            // read nearest sample (no interpolation) from source
            uint32_t src_frame = (uint32_t)voice->pos;
            if (voice->channels == 1) {
                int16_t s = voice->data[src_frame];
                float fs = (float)s / 32768.0f * voice->volume;
                float pan = (voice->pan + 1.0f) * 0.5f; // 0..1
                left = fs * (1.0f - pan);
                right = fs * pan;
            } else {
                uint32_t idx = src_frame * voice->channels;
                int16_t sL = voice->data[idx];
                int16_t sR = voice->data[idx + 1];
                float fL = (float)sL / 32768.0f * voice->volume;
                float fR = (float)sR / 32768.0f * voice->volume;
                float pan = voice->pan; // -1..1
                float left_mul = (pan <= 0.0f) ? 1.0f : 1.0f - pan;
                float right_mul = (pan >= 0.0f) ? 1.0f : 1.0f + pan;
                left = fL * left_mul + fR * (0.5f * (1.0f - pan));
                right = fR * right_mul + fL * (0.5f * (1.0f + pan));
            }
            int out_idx = f * out_channels;
            out[out_idx + 0] += left;
            out[out_idx + 1] += right;
            voice->pos += voice->step;
            if ((uint32_t)voice->pos >= voice->frames) { voice->active = false; break; }
        }
    }
    // clamp
    for (int i = 0; i < total; i++) {
        if (out[i] > 1.0f) out[i] = 1.0f;
        if (out[i] < -1.0f) out[i] = -1.0f;
    }
}

static void audio_stream_callback(float* buffer, int num_frames, int num_channels) {
    mix_into(buffer, num_frames);
}

void audio_stop_all(void) {
    for (int i = 0; i < (int)(sizeof(voices)/sizeof(voices[0])); i++) voices[i].active = false;
}

void audio_init(void) {
    // Setup sokol audio with callback, stereo output
    saudio_setup(&(saudio_desc){ 
        .sample_rate = 44100, 
        .stream_cb = audio_stream_callback, 
        .num_channels = 2, 
        .logger.func = slog_func 
    });
    memset(voices, 0, sizeof(voices));
}

void audio_shutdown(void) {
    audio_stop_all();
    saudio_shutdown();
}

float audio_get_pan_from_position(vec2_t source, vec2_t listener, float max_distance) {
    vec2_t to_source = vec2_subtract(source, listener);
    float distance = vec2_length(to_source);
    if (distance < 0.001f) return 0.0f; // At listener position = centered
    float pan = to_source.x / distance;
    
    // Scale pan based on distance - closer = less pan, farther = more pan
    float distance_factor = fminf(distance / max_distance, 1.0f);
    pan *= distance_factor;
    
    return pan;
}

float audio_get_volume_from_position(vec2_t source, vec2_t listener, float max_distance) {
    vec2_t to_source = vec2_subtract(source, listener);
    float distance = vec2_length(to_source);
    if (distance >= max_distance) return 0.0f;
    float volume = 1.0f - (distance / max_distance); // Linear falloff
    return volume * 0.1f;
}

bool audio_is_playing(audio_id_e audio_id) {
    if (audio_id <= 0 || audio_id >= AUDIO_COUNT) return false;
    for (int i = 0; i < (int)(sizeof(voices)/sizeof(voices[0])); i++) {
        if (voices[i].active && voices[i].audio_id == audio_id) {
            return true;
        }
    }
    return false;
}

int audio_play(audio_id_e audio_id, float volume, float pan) {
    if (audio_id <= 0 || audio_id >= AUDIO_COUNT) return -1;
    if (volume <= 0.01f) return -1; // Too quiet to play

    // If already playing, restart it
    for (int i = 0; i < (int)(sizeof(voices)/sizeof(voices[0])); i++) {
        if (voices[i].active && voices[i].audio_id == audio_id) {
            voices[i].pos = 0.0;
            voices[i].volume = volume;
            voices[i].pan = pan;
            return i;
        }
    }

    // Find free voice
    int slot = -1;
    for (int i = 0; i < (int)(sizeof(voices)/sizeof(voices[0])); i++) {
        if (!voices[i].active) { slot = i; break; }
    }
    if (slot < 0) return -1;
    const audio_info_t* info = &audio_infos[audio_id];
    const int16_t* data = audio_data_ptrs[audio_id];
    uint32_t size = audio_data_sizes[audio_id];
    if (!data || size == 0) return -1;

    voices[slot].active = true;
    voices[slot].audio_id = audio_id;
    voices[slot].data = data;
    voices[slot].frames = info->sample_count; // frames (as written by generator)
    voices[slot].pos = 0.0;
    // compute step: source_frames * step == output_frames. step = src_rate / out_rate
    int out_rate = saudio_sample_rate();
    voices[slot].step = (double)info->sample_rate / (double)out_rate;
    voices[slot].volume = volume;
    voices[slot].pan = pan;
    voices[slot].channels = info->channels;
    voices[slot].sample_rate = info->sample_rate;
    return slot;
}

int audio_play_2d(audio_id_e audio_id, vec2_t source, vec2_t listener, float base_volume) {
    if (audio_id <= 0 || audio_id >= AUDIO_COUNT) return -1;
    float max_distance = fmax(LF_GAME_WIDTH * 1.15f, LF_GAME_HEIGHT * 1.15f);
    float pan = audio_get_pan_from_position(source, listener, max_distance);
    float volume = audio_get_volume_from_position(source, listener, max_distance) * base_volume;
    return audio_play(audio_id, volume, pan);
}
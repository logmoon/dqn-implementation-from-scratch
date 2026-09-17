typedef struct {
    char* name;
    int width, height, channels;
    uint8_t* data;
    float atlas_uvs[4];
} build_image_t;

typedef struct {
    char* name;
    uint8_t* font_data;
    size_t font_data_size;
    stbtt_bakedchar char_data[96];
    int bitmap_width, bitmap_height;
    uint8_t* bitmap_data;
} build_font_t;

typedef struct {
    const char** items;
    size_t count;
    size_t capacity;
} files_in_dir_t;

bool find_all_files_in_dir(const char* dir, files_in_dir_t* file_paths, files_in_dir_t* file_names) {
    Nob_File_Paths children = {0};
    if (!nob_read_entire_dir(dir, &children)) {
        nob_log(NOB_ERROR, "Could not read images directory");
        return false;
    }
    for (size_t i = 0; i < children.count; ++i) {
        if (children.items[i][0] == '.') continue;
        char item_path[512];
        snprintf(item_path, sizeof(item_path), "%s/%s", dir, children.items[i]);
        Nob_File_Type type = nob_get_file_type(item_path);
        switch (type) {
            case NOB_FILE_REGULAR:
                nob_da_append(file_names, children.items[i]);
                char* name_copy = strdup(item_path);
                nob_da_append(file_paths, name_copy);
                break;
            case NOB_FILE_DIRECTORY:
                if (!find_all_files_in_dir(item_path, file_paths, file_names)) return false;
                break;
        }
    }
    return true;
}

void capitalize_and_remove_extension(const char *input, char *output) {
    const char *dot = strrchr(input, '.');
    size_t length = (dot != NULL) ? (dot - input) : strlen(input);
    for (size_t i = 0; i < length; i++) {
        output[i] = toupper(input[i]);
    }
    output[length] = '\0';
}

bool generate_assets_config(const char *assets_config_path)
{
    nob_log(NOB_INFO, "Generating %s", assets_config_path);

    // Get the images
    files_in_dir_t image_paths = {0};
    files_in_dir_t image_names = {0};
    const char* images_dir = "./res/images";
    if (!find_all_files_in_dir(images_dir, &image_paths, &image_names)) {
        nob_log(NOB_ERROR, "Could not read images in %s", images_dir);
        return false;
    }

    // Get the fonts
    files_in_dir_t font_paths = {0};
    files_in_dir_t font_names = {0};
    const char* fonts_dir = "./res/fonts";
    if (!find_all_files_in_dir(fonts_dir, &font_paths, &font_names)) {
        nob_log(NOB_ERROR, "Could not read fonts in %s", fonts_dir);
        return false;
    }

    // Get the audio files
    files_in_dir_t audio_paths = {0};
    files_in_dir_t audio_names = {0};
    const char* audio_dir = "./res/audio";
    if (!find_all_files_in_dir(audio_dir, &audio_paths, &audio_names)) {
        nob_log(NOB_ERROR, "Could not read audio files in %s", audio_dir);
        return false;
    }

    // Load all images
    build_image_t* images = malloc(sizeof(build_image_t) * image_names.count);
    for (size_t i = 0; i < image_names.count; ++i) {
        stbi_set_flip_vertically_on_load(1);
        images[i].name = strdup(image_names.items[i]);
        images[i].data = stbi_load(image_paths.items[i], 
                                &images[i].width, 
                                &images[i].height, 
                                &images[i].channels, 4);
        if (!images[i].data) {
            nob_log(NOB_ERROR, "Failed to load image: %s", image_paths.items[i]);
            return false;
        }
    }


    // Pack images into atlas
    int atlas_width = 512, atlas_height = 512;
    stbrp_context pack_context;
    stbrp_node* pack_nodes = malloc(sizeof(stbrp_node) * atlas_width);
    stbrp_init_target(&pack_context, atlas_width, atlas_height, pack_nodes, atlas_width);
    
    stbrp_rect* pack_rects = malloc(sizeof(stbrp_rect) * image_names.count);
    for (size_t i = 0; i < image_names.count; i++) {
        pack_rects[i].id = i;
        pack_rects[i].w = images[i].width;
        pack_rects[i].h = images[i].height;
    }
    
    if (!stbrp_pack_rects(&pack_context, pack_rects, image_names.count)) {
        nob_log(NOB_ERROR, "Failed to pack images into atlas");
        return false;
    }

    // Create atlas bitmap
    uint8_t* atlas_data = calloc(atlas_width * atlas_height * 4, 1);
    // Set all pixels to white with full opacity (RGBA: 255,255,255,255)
    for (int i = 0; i < atlas_width * atlas_height * 4; i += 4) {
        atlas_data[i] = 255;     // R
        atlas_data[i+1] = 255;   // G
        atlas_data[i+2] = 255;   // B
        atlas_data[i+3] = 255;   // A
    }
    
    // Copy images into atlas and calculate UVs
    for (size_t i = 0; i < image_names.count; i++) {
        stbrp_rect* rect = &pack_rects[i];
        build_image_t* img = &images[rect->id];
        
        // Copy image data to atlas
        for (int row = 0; row < rect->h; row++) {
            uint8_t* src_row = img->data + (row * rect->w * 4);
            uint8_t* dst_row = atlas_data + ((rect->y + row) * atlas_width + rect->x) * 4;
            memcpy(dst_row, src_row, rect->w * 4);
        }

        float pixel_padding = 0.05f;

        // Calculate UV coordinates
        img->atlas_uvs[0] = (float)(rect->x + pixel_padding) / atlas_width;           // min_x + padding
        img->atlas_uvs[1] = (float)(rect->y + pixel_padding) / atlas_height;          // min_y + padding  
        img->atlas_uvs[2] = (float)(rect->x + rect->w - pixel_padding) / atlas_width; // max_x - padding
        img->atlas_uvs[3] = (float)(rect->y + rect->h - pixel_padding) / atlas_height;// max_y - padding
    }

    // Debugging: Save atlas to file for testing
    // stbi_write_png("atlas.png", atlas_width, atlas_height, 4, atlas_data, atlas_width * 4);

    // Generate font atlases
    build_font_t* fonts = malloc(sizeof(build_font_t) * font_names.count);
    for (size_t i = 0; i < font_names.count; i++) {
        fonts[i].name = strdup(font_names.items[i]);
        fonts[i].bitmap_width = 255;
        fonts[i].bitmap_height = 255;
        fonts[i].bitmap_data = malloc(fonts[i].bitmap_width * fonts[i].bitmap_height);
        
        FILE* font_file = fopen(font_paths.items[i], "rb");
        if (!font_file) {
            nob_log(NOB_ERROR, "Could not open font file: %s", font_paths.items[i]);
            return false;
        }

        fseek(font_file, 0, SEEK_END);
        long file_size = ftell(font_file);
        fseek(font_file, 0, SEEK_SET);

        fonts[i].font_data = malloc(file_size);
        fonts[i].font_data_size = file_size;
        size_t bytes_read = fread(fonts[i].font_data, 1, file_size, font_file);
        fclose(font_file);

        if (bytes_read != file_size) {
            nob_log(NOB_ERROR, "Could not read entire font file: %s", font_paths.items[i]);
            free(fonts[i].font_data);
            return false;
        }

        float font_height = 16.0f;
        int result = stbtt_BakeFontBitmap(fonts[i].font_data, 0, font_height,
                                          fonts[i].bitmap_data, 
                                          fonts[i].bitmap_width, fonts[i].bitmap_height,
                                          32, 96, fonts[i].char_data);
        if (result <= 0) {
            nob_log(NOB_ERROR, "Failed to bake font: %s", font_paths.items[i]);
            return false;
        }
    }

    FILE *f = fopen(assets_config_path, "wb");
    if (f == NULL) {
        nob_log(NOB_ERROR, "Could not generate %s: %s", assets_config_path, strerror(errno));
        return false;
    }

    genf(f, "// GENERATED FILE - DO NOT EDIT");
    genf(f, "#ifndef ASSETS_H_");
    genf(f, "#define ASSETS_H_");
    genf(f, "#include <stdint.h>");

    // Image enum
    genf(f, "// Image IDs");
    genf(f, "typedef enum {");
    genf(f, "    IMAGE_NIL = 0,");
    for (size_t i = 0; i < image_names.count; ++i) {
        char enum_name[256];
        capitalize_and_remove_extension(image_names.items[i], enum_name);
        genf(f, "    IMAGE_%s = %llu,", enum_name, i + 1);
    }
    genf(f, "    IMAGE_COUNT = %llu", image_names.count + 1);
    genf(f, "} image_id_e;");
    genf(f, "");

    // Font enum
    genf(f, "// Font IDs");
    genf(f, "typedef enum {");
    genf(f, "    FONT_NIL = 0,");
    for (size_t i = 0; i < font_names.count; ++i) {
        char enum_name[256];
        capitalize_and_remove_extension(font_names.items[i], enum_name);
        genf(f, "    FONT_%s = %llu,", enum_name, i + 1);
    }
    genf(f, "    FONT_COUNT = %llu", font_names.count + 1);
    genf(f, "} font_id_e;");
    genf(f, "");

    // Structs
    genf(f, "// Image metadata");
    genf(f, "typedef struct {");
    genf(f, "    int32_t width, height;");
    genf(f, "    uint8_t tex_index;");
    genf(f, "    float atlas_uvs[4];");
    genf(f, "} image_info_t;");
    genf(f, "");

    genf(f, "// Font character data");
    genf(f, "typedef struct {");
    genf(f, "    uint16_t x0, y0, x1, y1;");
    genf(f, "    float xoff, yoff, xadvance;");
    genf(f, "    float s0, t0, s1, t1;");
    genf(f, "} font_char_t;");
    genf(f, "");

    genf(f, "// Font metadata");
    genf(f, "typedef struct {");
    genf(f, "    int32_t bitmap_width, bitmap_height;");
    genf(f, "    uint8_t tex_index;");
    genf(f, "    font_char_t char_data[96];");
    genf(f, "    float font_height;");
    genf(f, "} font_info_t;");
    genf(f, "");

    genf(f, "extern const uint8_t atlas_data[];");
    genf(f, "extern const uint32_t atlas_data_size;");
    genf(f, "extern const int32_t atlas_width;");
    genf(f, "extern const int32_t atlas_height;");
    genf(f, "");

    for (size_t i = 0; i < font_names.count; i++) {
        char font_name[256];
        capitalize_and_remove_extension(font_names.items[i], font_name);
        for (char* p = font_name; *p; p++) *p = tolower(*p);
        genf(f, "extern const uint8_t font_%s_data[];", font_name);
        genf(f, "extern const uint32_t font_%s_data_size;", font_name);
    }
    genf(f, "");

    genf(f, "extern const image_info_t image_infos[IMAGE_COUNT];");
    genf(f, "extern const font_info_t font_infos[FONT_COUNT];");
    genf(f, "");

    // Audio declarations
    genf(f, "// Audio IDs");
    genf(f, "typedef enum {");
    genf(f, "    AUDIO_NIL = 0,");
    for (size_t i = 0; i < audio_names.count; ++i) {
        char enum_name[256];
        capitalize_and_remove_extension(audio_names.items[i], enum_name);
        genf(f, "    AUDIO_%s = %llu,", enum_name, i + 1);
    }
    genf(f, "    AUDIO_COUNT = %llu", audio_names.count + 1);
    genf(f, "} audio_id_e;");
    genf(f, "");

    genf(f, "// Audio metadata");
    genf(f, "typedef struct {");
    genf(f, "    uint32_t sample_rate;");
    genf(f, "    uint16_t channels;");
    genf(f, "    uint32_t sample_count; // number of frames (samples per channel)");
    genf(f, "} audio_info_t;");
    genf(f, "");

    for (size_t i = 0; i < audio_names.count; i++) {
        char audio_name[256];
        capitalize_and_remove_extension(audio_names.items[i], audio_name);
        for (char* p = audio_name; *p; p++) *p = tolower(*p);
        genf(f, "extern const int16_t audio_%s_data[];", audio_name);
        genf(f, "extern const uint32_t audio_%s_data_size;", audio_name);
    }
    genf(f, "");

    genf(f, "const uint8_t atlas_data[] = {");
    for (int i = 0; i < atlas_width * atlas_height * 4; i++) {
        if (i == 0) fprintf(f, "    ");
        else if (i % 17 == 0) fprintf(f, "\n    ");
        fprintf(f, "0x%02x,", atlas_data[i]);
    }
    genf(f, "\n};");
    genf(f, "const uint32_t atlas_data_size = %d;", atlas_width * atlas_height * 4);
    genf(f, "const int32_t atlas_width = %d;", atlas_width);
    genf(f, "const int32_t atlas_height = %d;", atlas_height);

    // Write audio bitstreams and metadata
    for (size_t audio_idx = 0; audio_idx < audio_names.count; audio_idx++) {
        // load file into memory
        FILE* af = fopen(audio_paths.items[audio_idx], "rb");
        if (!af) {
            nob_log(NOB_ERROR, "Could not open audio file: %s", audio_paths.items[audio_idx]);
            return false;
        }
        fseek(af, 0, SEEK_END);
        long audio_file_size = ftell(af);
        fseek(af, 0, SEEK_SET);
        unsigned char* audio_mem = malloc(audio_file_size);
        if (!audio_mem) {
            fclose(af);
            nob_log(NOB_ERROR, "Out of memory reading audio: %s", audio_paths.items[audio_idx]);
            return false;
        }
        size_t r = fread(audio_mem, 1, audio_file_size, af);
        fclose(af);
        if (r != audio_file_size) {
            free(audio_mem);
            nob_log(NOB_ERROR, "Could not read entire audio file: %s", audio_paths.items[audio_idx]);
            return false;
        }

        int channels = 0;
        int sample_rate = 0;
        short* decoded = NULL;
        int samples = stb_vorbis_decode_memory(audio_mem, (int)audio_file_size, &channels, &sample_rate, &decoded);
        free(audio_mem);
        if (samples <= 0 || decoded == NULL) {
            nob_log(NOB_ERROR, "Failed to decode vorbis audio: %s", audio_paths.items[audio_idx]);
            return false;
        }

        char enum_name[256];
        capitalize_and_remove_extension(audio_names.items[audio_idx], enum_name);
        for (char* p = enum_name; *p; p++) *p = tolower(*p);

        genf(f, "const int16_t audio_%s_data[] = {", enum_name);
        int total_shorts = samples * channels; // Total interleaved samples
        for (int i = 0; i < total_shorts; i++) {
            if (i == 0) fprintf(f, "    ");
            else if (i % 17 == 0) fprintf(f, "\n    ");
            fprintf(f, "0x%04x,", (uint16_t)decoded[i] & 0xffff);
        }
        genf(f, "\n};");
        genf(f, "const uint32_t audio_%s_data_size = %d;", enum_name, total_shorts);

        // free decoded buffer after writing
        free(decoded);
    }

    // Write audio_infos
    genf(f, "const audio_info_t audio_infos[AUDIO_COUNT] = {");
    genf(f, "    [AUDIO_NIL] = {0},");
    for (size_t i = 0; i < audio_names.count; ++i) {
        char enum_name[256];
        capitalize_and_remove_extension(audio_names.items[i], enum_name);
        // We need to re-open file to decode header info (channels, sample_rate, sample_count)
        FILE* af = fopen(audio_paths.items[i], "rb");
        if (!af) {
            nob_log(NOB_ERROR, "Could not open audio file for metadata: %s", audio_paths.items[i]);
            return false;
        }
        fseek(af, 0, SEEK_END);
        long audio_file_size = ftell(af);
        fseek(af, 0, SEEK_SET);
        unsigned char* audio_mem = malloc(audio_file_size);
        if (!audio_mem) {
            fclose(af);
            nob_log(NOB_ERROR, "Out of memory reading audio for metadata: %s", audio_paths.items[i]);
            return false;
        }
        size_t r = fread(audio_mem, 1, audio_file_size, af);
        fclose(af);
        if (r != audio_file_size) {
            free(audio_mem);
            nob_log(NOB_ERROR, "Could not read entire audio file for metadata: %s", audio_paths.items[i]);
            return false;
        }
        int channels = 0;
        int sample_rate = 0;
        short* decoded = NULL;
        int samples = stb_vorbis_decode_memory(audio_mem, (int)audio_file_size, &channels, &sample_rate, &decoded);
        free(audio_mem);
        if (samples <= 0 || decoded == NULL) {
            nob_log(NOB_ERROR, "Failed to decode vorbis audio for metadata: %s", audio_paths.items[i]);
            return false;
        }
        int frames = samples;
        genf(f, "    [AUDIO_%s] = {%d, %d, %d},", enum_name, sample_rate, channels, frames);
        free(decoded);
    }
    genf(f, "};");

    // Write pointer and size tables for audio data
    genf(f, "// Tables for runtime access");
    genf(f, "const int16_t* audio_data_ptrs[AUDIO_COUNT] = {");
    genf(f, "    [AUDIO_NIL] = NULL,");
    for (size_t i = 0; i < audio_names.count; ++i) {
        char enum_name[256];
        char audio_name[256];
        capitalize_and_remove_extension(audio_names.items[i], enum_name);
        capitalize_and_remove_extension(audio_names.items[i], audio_name);
        for (char* p = audio_name; *p; p++) *p = tolower(*p);
        genf(f, "    [AUDIO_%s] = audio_%s_data,", enum_name, audio_name);
    }
    genf(f, "};");
    genf(f, "const uint32_t audio_data_sizes[AUDIO_COUNT] = {");
    genf(f, "    [AUDIO_NIL] = 0,");
    for (size_t i = 0; i < audio_names.count; ++i) {
        char enum_name[256];
        char audio_name[256];
        capitalize_and_remove_extension(audio_names.items[i], enum_name);
        capitalize_and_remove_extension(audio_names.items[i], audio_name);
        for (char* p = audio_name; *p; p++) *p = tolower(*p);
        genf(f, "    [AUDIO_%s] = audio_%s_data_size,", enum_name, audio_name);
    }
    genf(f, "};");

    for (size_t font_idx = 0; font_idx < font_names.count; font_idx++) {
        char enum_name[256];
        capitalize_and_remove_extension(font_names.items[font_idx], enum_name);
        for (char* p = enum_name; *p; p++) *p = tolower(*p);

        genf(f, "const uint8_t font_%s_data[] = {", enum_name);
        int font_size = fonts[font_idx].bitmap_width * fonts[font_idx].bitmap_height;
        for (int i = 0; i < font_size; i++) {
            if (i == 0) fprintf(f, "    ");
            else if (i % 17 == 0) fprintf(f, "\n    ");
            fprintf(f, "0x%02x,", fonts[font_idx].bitmap_data[i]);
        }
        genf(f, "\n};");
        genf(f, "const uint32_t font_%s_data_size = %d;", enum_name, font_size);
    }

    genf(f, "const image_info_t image_infos[IMAGE_COUNT] = {");
    genf(f, "    [IMAGE_NIL] = {0},");
    for (size_t i = 0; i < image_names.count; i++) {
        char enum_name[256];
        capitalize_and_remove_extension(image_names.items[i], enum_name);
        genf(f, "    [IMAGE_%s] = {%d, %d, 0, {%.4ff, %.4ff, %.4ff, %.4ff}},", 
             enum_name, 
             images[i].width, images[i].height,
             images[i].atlas_uvs[0], images[i].atlas_uvs[1], 
             images[i].atlas_uvs[2], images[i].atlas_uvs[3]);
    }
    genf(f, "};");

    genf(f, "const font_info_t font_infos[FONT_COUNT] = {");
    genf(f, "    [FONT_NIL] = {0},");
    for (size_t i = 0; i < font_names.count; i++) {
        char enum_name[256];
        capitalize_and_remove_extension(font_names.items[i], enum_name);
        genf(f, "    [FONT_%s] = {%d, %d, 1, {", enum_name, 
             fonts[i].bitmap_width, fonts[i].bitmap_height);
        for (int c = 0; c < 96; c++) {
            stbtt_bakedchar* bc = &fonts[i].char_data[c];
            float s0 = (float)bc->x0 / fonts[i].bitmap_width;
            float t0 = (float)bc->y0 / fonts[i].bitmap_height;
            float s1 = (float)bc->x1 / fonts[i].bitmap_width;
            float t1 = (float)bc->y1 / fonts[i].bitmap_height;
            genf(f, "        {%d, %d, %d, %d, %.4ff, %.4ff, %.4ff, %.4ff, %.4ff, %.4ff, %.4ff},",
                 bc->x0, bc->y0, bc->x1, bc->y1,
                 bc->xoff, bc->yoff, bc->xadvance,
                 s0, t0, s1, t1);
        }
        genf(f, "    }, 16.0000f},");
    }
    genf(f, "};");

    genf(f, "#endif // ASSETS_H_");
    fclose(f);

    free(pack_nodes);
    free(pack_rects);
    free(atlas_data);
    for (size_t i = 0; i < image_names.count; i++) {
        stbi_image_free(images[i].data);
        free(images[i].name);
    }
    free(images);
    
    for (size_t i = 0; i < font_names.count; i++) {
        free(fonts[i].bitmap_data);
        free(fonts[i].font_data);
        free(fonts[i].name);
    }
    free(fonts);

    return true;
}
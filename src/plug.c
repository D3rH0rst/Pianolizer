#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <assert.h>
#ifdef _WIN32
#include "../WinDependencies/include/raylib.h"
#include "../WinDependencies/include/fluidsynth.h"
#else
#include <raylib.h>
#include <fluidsynth.h>
#endif
#include "plug.h"

#define N_KEYS 88
#define N_WHITE_KEYS 52
#define N_BLACK_KEYS 36
#define KEYS_IN_OCTAVE 12

#define SCROLL_RECT_CAP 10

#define SCROLL_SPEED 200
#define KEY_SCROLL_RECT_OFFSET 5

#define FONT_SIZE 64

float padding = 1.0f;

float small_offset;

float whiteKey_width;
float whiteKey_height;

float blackKey_width;
float blackKey_height;


typedef struct {
    Rectangle rect;
    bool finished;
} ScrollRect;

typedef struct {
    ScrollRect* scrollRects;
    size_t size;
    size_t capacity;
} ScrollRects;

typedef struct {
    size_t index;
    size_t color_index;
    size_t key_oct;
    bool pressed;
    bool white;
    Rectangle key_rect;
    ScrollRects scroll_rects;
} Key;

typedef struct {
    // Key stuff
    Key keys[N_KEYS];
    Key* white_keys[N_WHITE_KEYS];
    Key* black_keys[N_BLACK_KEYS];
    Key* last_pressed_key;


    Font font;

    //fluidsynth
    fluid_settings_t* fs_settings;
    fluid_synth_t* fs_synth;
    fluid_audio_driver_t* fs_audio_driver;
    fluid_player_t* fs_player;
    int sound_font_id;
} Plug;

static Plug* p = NULL;


void init_sr_array(ScrollRects* arr, size_t initialCapacity) {
    arr->scrollRects = malloc(initialCapacity * sizeof(ScrollRect));
    arr->size = 0;
    arr->capacity = initialCapacity;
}

void append_scroll_rect(ScrollRects* arr, ScrollRect sr) {
    if (arr->size == arr->capacity) {
        arr->capacity *= 2;
        arr->scrollRects = realloc(arr->scrollRects, arr->capacity * sizeof(ScrollRect));
    }
    arr->scrollRects[arr->size++] = sr;
}

void delete_scroll_rect(ScrollRects* arr, size_t index) {
    if (index >= arr->size) {
        TraceLog(LOG_WARNING, "Attempted to delete an invalid index %ld from array with size %ld", index, arr->size);
        return;
    }

    // Shift elements after index one position to the left
    for (size_t i = index; i < arr->size - 1; i++) {
        arr->scrollRects[i] = arr->scrollRects[i + 1];
    }

    arr->size--;
}

bool is_white(size_t key_octave) {
    if (key_octave < 5)
        return key_octave % 2 == 0;
    return key_octave % 2 != 0;
}

void* plug_pre_reload(void) {
    return p;
}

void plug_post_reload(Plug* pP) {
    p = pP;
}

void calculate_key_rects() {
    whiteKey_width = ((float)GetScreenWidth() - (padding * (N_WHITE_KEYS - 1))) / N_WHITE_KEYS;
    whiteKey_height = whiteKey_width * 7;

    blackKey_width = whiteKey_width * 0.6f;
    blackKey_height = whiteKey_height * 0.75f;

    small_offset = whiteKey_width + padding;
    for (size_t i = 0; i < N_KEYS; i++) {
        if (p->keys[i].white) {
            p->keys[i].key_rect.x = (float) p->keys[i].color_index * (whiteKey_width + padding);
            p->keys[i].key_rect.y = (float) GetScreenHeight() - whiteKey_height;
            p->keys[i].key_rect.width = whiteKey_width;
            p->keys[i].key_rect.height = whiteKey_height;

        } else {
            p->keys[i].key_rect.x = (p->keys[i - 1].color_index + 1) * small_offset - blackKey_width / 2.f;
            p->keys[i].key_rect.y = (float) GetScreenHeight() - whiteKey_height;
            p->keys[i].key_rect.width = blackKey_width;
            p->keys[i].key_rect.height = blackKey_height;
        }

        for (size_t j = 0; j < p->keys[i].scroll_rects.size; j++) {
            if (p->keys[i].white) {
                p->keys[i].scroll_rects.scrollRects[j].rect.width = whiteKey_width;
            }
            else {
                p->keys[i].scroll_rects.scrollRects[j].rect.width = blackKey_width;
            }
            p->keys[i].scroll_rects.scrollRects[j].rect.x = p->keys[i].key_rect.x;
        }
    }
}

int player_callback(void* data, fluid_midi_event_t* event) {
    (void)data;
    uint8_t status, data1, data2;
    status = fluid_midi_event_get_type(event);
    data1 = fluid_midi_event_get_key(event);
    data2 = fluid_midi_event_get_velocity(event);
    if (status == 0x80 || (status == 0x90 && data2 == 0x0)) {
        // Key off
        p->keys[data1 - 21].pressed = false;
    }

    if (status == 0x90 && data2 > 0x0) {
        // Key on
        ScrollRect sr = {0};
        size_t key_index = data1 - 21;
        p->keys[key_index].pressed = true;
        sr.finished = false;
        sr.rect.width = p->keys[key_index].white ? whiteKey_width : blackKey_width;
        sr.rect.height = 1;
        sr.rect.x = p->keys[key_index].key_rect.x;
        sr.rect.y = p->keys[key_index].key_rect.y - sr.rect.height - KEY_SCROLL_RECT_OFFSET;
        append_scroll_rect(&p->keys[key_index].scroll_rects, sr);
    }

    return fluid_synth_handle_midi_event(p->fs_synth, event);
}

void init_keys(void) {
    whiteKey_width = ((float)GetScreenWidth() - (padding * (N_WHITE_KEYS - 1))) / N_WHITE_KEYS;
    whiteKey_height = whiteKey_width * 7;

    blackKey_width = whiteKey_width * 0.6f;
    blackKey_height = whiteKey_height * 0.75f;

    small_offset = whiteKey_width + padding;
    size_t black_index = 0;
    size_t white_index = 0;
    for (size_t i = 0; i < N_KEYS; i++) {
        p->keys[i].index = i;
        p->keys[i].pressed = false;
        p->keys[i].key_oct = (i + 9) % KEYS_IN_OCTAVE;
        if (is_white(p->keys[i].key_oct)) {
            p->keys[i].white = true;
            p->keys[i].color_index = white_index;

            // set up the key rectangle (needed for mouse hit detection)
            p->keys[i].key_rect.x = (float)p->keys[i].color_index * (whiteKey_width + padding);
            p->keys[i].key_rect.y = (float)GetScreenHeight() - whiteKey_height;
            p->keys[i].key_rect.width = whiteKey_width;
            p->keys[i].key_rect.height = whiteKey_height;

            p->white_keys[white_index] = &p->keys[i];
            white_index++;
        } else {
            p->keys[i].white = false;
            p->keys[i].color_index = black_index;

            // key rect
            p->keys[i].key_rect.x = (p->keys[i - 1].color_index + 1) * small_offset - blackKey_width / 2.f;
            p->keys[i].key_rect.y = (float)GetScreenHeight() - whiteKey_height;
            p->keys[i].key_rect.width = blackKey_width;
            p->keys[i].key_rect.height = blackKey_height;

            p->black_keys[black_index] = &p->keys[i];
            black_index++;
        }
        init_sr_array(&p->keys[i].scroll_rects, SCROLL_RECT_CAP);
    }
}

void init_fluid_synth(void) {
    p->fs_settings = new_fluid_settings();
    if (p->fs_settings == NULL) {
        TraceLog(LOG_ERROR, "FLUIDSYNTH: failed to create settings");
    }

    p->fs_synth = new_fluid_synth(p->fs_settings);
    if (p->fs_synth == NULL) {
        TraceLog(LOG_ERROR, "FLUIDSYNTH: failed to create synth");
    }

    p->fs_player = new_fluid_player(p->fs_synth);
    if (p->fs_player == NULL) {
        TraceLog(LOG_ERROR, "FLUIDSYNTH: failed to create player");
    }
    fluid_player_set_playback_callback(p->fs_player, player_callback, NULL);

    p->fs_audio_driver = new_fluid_audio_driver(p->fs_settings, p->fs_synth);
    if (p->fs_audio_driver == NULL) {
        TraceLog(LOG_ERROR, "FLUIDSYNTH: failed to create audio driver");
    }
}

void plug_init(void) {
    p = malloc(sizeof(*p));
    assert(p != NULL && "Buy more RAM lol");
    memset(p, 0, sizeof(*p));
    p->font = LoadFontEx("../resources/LouisGeorgeCafe.ttf", FONT_SIZE, NULL, 0);
    GenTextureMipmaps(&p->font.texture);
    SetTextureFilter(p->font.texture, TEXTURE_FILTER_BILINEAR);
    init_keys();
    init_fluid_synth();
}

void plug_clean(void) {
    delete_fluid_audio_driver(p->fs_audio_driver);
    delete_fluid_player(p->fs_player);
    delete_fluid_synth(p->fs_synth);
    delete_fluid_settings(p->fs_settings);
    UnloadFont(p->font);
    free(p);
}

void render_key(Key* key) {
    Color topColor;
    Color bottomColor;

    if (key->white) {
        topColor = WHITE;
        bottomColor = key->pressed ? GRAY : topColor;
        DrawRectangleGradientV(key->key_rect.x, key->key_rect.y, key->key_rect.width, key->key_rect.height, topColor, bottomColor);
    } else {
        topColor = CLITERAL(Color){ 50, 50, 50, 255 };
        bottomColor = key->pressed ? BLACK : topColor;
        DrawRectangleGradientV(key->key_rect.x, key->key_rect.y, key->key_rect.width, key->key_rect.height, topColor, bottomColor);
    }
}

void reset_keys() {
    for (size_t i = 0; i < N_KEYS; i++) {
        p->keys[i].pressed = false;
    }
}

void render_keys() {
    for (size_t i = 0; i < N_WHITE_KEYS; i++) {
        render_key(p->white_keys[i]);          // Render all white keys before all black keys to avoid overlapping
    }

    for (size_t i = 0; i < N_BLACK_KEYS; i++) {
        render_key(p->black_keys[i]);
    }
}

void update_keys() {    // handles mouse input, creating scroll rects and playing notes with fluidsynth if a key was newly pressed
    bool black_pressed = false;
    ScrollRect sr = {0};

    if (!IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
        if (p->last_pressed_key != NULL) {
            p->last_pressed_key->pressed = false;
            // turn off note in fluidsynth
            fluid_synth_noteoff(p->fs_synth, 0, p->last_pressed_key->index + 21);
        }
    } else {
        for (size_t i = 0; i < N_BLACK_KEYS; i++) {
            if (CheckCollisionPointRec(GetMousePosition(), p->black_keys[i]->key_rect)) {
                if (!p->black_keys[i]->pressed) {
                    if (p->last_pressed_key != NULL) {
                        p->last_pressed_key->pressed = false;
                        fluid_synth_noteoff(p->fs_synth, 0, p->last_pressed_key->index + 21);
                    }
                    p->black_keys[i]->pressed = true;
                    p->last_pressed_key = p->black_keys[i];

                    // create sound with fluidsynth
                    fluid_synth_noteon(p->fs_synth, 0, p->black_keys[i]->index + 21, 80);

                    // Create the Scroll Rect
                    sr.finished = false;
                    sr.rect.width = blackKey_width;
                    sr.rect.height = 1;
                    sr.rect.x = p->black_keys[i]->key_rect.x;
                    sr.rect.y = p->black_keys[i]->key_rect.y - sr.rect.height - KEY_SCROLL_RECT_OFFSET;
                    append_scroll_rect(&p->black_keys[i]->scroll_rects, sr);
                }
                black_pressed = true;                           // prevent white key being pressed through black key
            }
        }
        for (size_t i = 0; i < N_WHITE_KEYS; i++) {
            if (black_pressed) {
                // p->white_keys[i]->pressed = false;
            } else {
                if (CheckCollisionPointRec(GetMousePosition(), p->white_keys[i]->key_rect)) {
                    if (!p->white_keys[i]->pressed) {
                        if (p->last_pressed_key != NULL) {
                            p->last_pressed_key->pressed = false;
                            fluid_synth_noteoff(p->fs_synth, 0, p->last_pressed_key->index + 21);
                        }
                        p->white_keys[i]->pressed = true;
                        p->last_pressed_key = p->white_keys[i];

                        // create sound with fluidsynth.
                        fluid_synth_noteon(p->fs_synth, 0, p->white_keys[i]->index + 21, 80);

                        // Create the Scroll Rect
                        sr.finished = false;
                        sr.rect.width = whiteKey_width;
                        sr.rect.height = 1;
                        sr.rect.x = p->white_keys[i]->key_rect.x;
                        sr.rect.y = p->white_keys[i]->key_rect.y - sr.rect.height - KEY_SCROLL_RECT_OFFSET;
                        append_scroll_rect(&p->white_keys[i]->scroll_rects, sr);
                    }
                }
            }
        }
    }
}

void render_scroll_rects() {
    float hue_top, hue_bottom;
    Rectangle current_rect;
    Color color_top, color_bottom;

    for (size_t i = 0; i < N_WHITE_KEYS; i++) {
        for (size_t j = 0; j < p->white_keys[i]->scroll_rects.size; j++) {
            current_rect = p->white_keys[i]->scroll_rects.scrollRects[j].rect;
            hue_top = (current_rect.y) / ((float)GetScreenHeight() - whiteKey_height - KEY_SCROLL_RECT_OFFSET);
            hue_bottom = (current_rect.y + current_rect.height) / ((float)GetScreenHeight() - whiteKey_height - KEY_SCROLL_RECT_OFFSET);
            color_top = ColorFromHSV(hue_top * 360, 1, 1);
            color_bottom = ColorFromHSV(hue_bottom * 360, 1, 1);
            DrawRectangleGradientV(current_rect.x, current_rect.y, current_rect.width, current_rect.height, color_top, color_bottom);
            // DrawRectangleRounded(current_rect, 0.3, 5, color);
        }
    }
    for (size_t i = 0; i < N_BLACK_KEYS; i++) {
        for (size_t j = 0; j < p->black_keys[i]->scroll_rects.size; j++) {
            current_rect = p->black_keys[i]->scroll_rects.scrollRects[j].rect;
            hue_top = 1.f - (current_rect.y) / ((float)GetScreenHeight() - whiteKey_height - KEY_SCROLL_RECT_OFFSET);
            hue_bottom = 1.f - (current_rect.y + current_rect.height) / ((float)GetScreenHeight() - whiteKey_height - KEY_SCROLL_RECT_OFFSET);
            color_top = ColorFromHSV(hue_top * 360, 1.f, 0.5f);
            color_bottom = ColorFromHSV(hue_bottom * 360, 1.f, 0.5f);
            DrawRectangleGradientV(current_rect.x, current_rect.y, current_rect.width, current_rect.height, color_top, color_bottom);
            // DrawRectangleRounded(current_rect, 0.3, 5, color);
        }
    }
}

void update_scroll_rects() {
    float dt = GetFrameTime();
    float offset = SCROLL_SPEED * dt;
    size_t n_scroll_rects = 0;
    ScrollRect* current_rects = NULL;

    for (size_t i = 0; i < N_KEYS; i++) {
        n_scroll_rects = p->keys[i].scroll_rects.size;
        current_rects = p->keys[i].scroll_rects.scrollRects;
        if (!p->keys[i].pressed) {
            if (n_scroll_rects > 0) {
                current_rects[n_scroll_rects - 1].finished = true;
            }
        } else {
            current_rects[n_scroll_rects - 1].rect.height += offset;
        }

        for (size_t j = 0; j < n_scroll_rects; j++) {
            current_rects[j].rect.y -= offset;

            if (current_rects[j].finished) {
                if (current_rects[j].rect.y + current_rects[j].rect.height < 0) {
                    delete_scroll_rect(&p->keys[i].scroll_rects, j);
                }
            }
        }
    }
}

void handle_dropped_file() {
    FilePathList dropped_files = LoadDroppedFiles();

    const char* file0 = dropped_files.paths[0];
    if (fluid_is_midifile(file0)) {
        if (p->fs_player != NULL) {
            fluid_player_stop(p->fs_player);
            delete_fluid_player(p->fs_player);
            fluid_synth_all_notes_off(p->fs_synth, -1);
            p->fs_player = new_fluid_player(p->fs_synth);
            if (p->fs_player == NULL) {
                TraceLog(LOG_ERROR, "FLUIDSYNTH: failed to create player");
            }
            fluid_player_set_playback_callback(p->fs_player, player_callback, NULL);
            fluid_player_add(p->fs_player, file0);
            fluid_player_set_loop(p->fs_player, -1);
            fluid_player_play(p->fs_player);

            reset_keys();
        }
        TraceLog(LOG_INFO, "MIDI: Midi file loaded: %s Press P to play/pause", file0);

    }
    else if (fluid_is_soundfont(file0) && strcmp(".sf2", GetFileExtension(file0)) == 0) {
        p->sound_font_id = fluid_synth_sfload(p->fs_synth, file0, 1);
        TraceLog(LOG_INFO, "Sound Font ID: %d", p->sound_font_id);
        if (p->sound_font_id == FLUID_FAILED) {
            TraceLog(LOG_ERROR, "FLUIDSYNTH: failed to load soundfont [%s]", file0);
        }
        else {
            TraceLog(LOG_INFO, "FLUIDSYNTH: Loaded sound font file [%s]", file0);
        }
    }
    else {
        TraceLog(LOG_INFO, "MIDI: Unupported file fropped: %s", file0);
    }
    UnloadDroppedFiles(dropped_files);
}

void handle_user_input() {
    if (IsKeyPressed(KEY_P)) {
        int fp_status = fluid_player_get_status(p->fs_player);
        if (fp_status == FLUID_PLAYER_PLAYING) {
            fluid_player_stop(p->fs_player);
        } else if (fp_status == FLUID_PLAYER_DONE) {
            fluid_player_play(p->fs_player);
        }
        reset_keys();
    }
    if (IsKeyPressed(KEY_Q)) {
        fluid_player_seek(p->fs_player, 0);
        reset_keys();
    }
    if (IsKeyPressed(KEY_H)) {
        int total_ticks = fluid_player_get_total_ticks(p->fs_player);
        fluid_player_seek(p->fs_player, total_ticks / 2);
        reset_keys();
    }

    if (IsFileDropped()) {
        handle_dropped_file();
    }

    if (IsWindowResized()) {
        calculate_key_rects();
    }
}

void plug_update(void) {
    BeginDrawing();
        ClearBackground(DARKGRAY);
        render_keys();
        update_keys();

        render_scroll_rects();
        update_scroll_rects();

        if (fluid_synth_sfcount(p->fs_synth) == 0) {
            DrawTextEx(p->font, "No SoundFont file loaded (.sf2). Drag&Drop one to hear sound", CLITERAL(Vector2){50, 50}, 20, 0, BLACK);
        }

    EndDrawing();
    handle_user_input();
}
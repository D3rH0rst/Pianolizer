#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <assert.h>
#include <math.h>

#ifdef _WIN32
#include "../WinDependencies/include/raylib.h"
#include "../WinDependencies/include/rlgl.h"
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

#define WHITE_KEY_WH_RATIO 5
#define WHITE_BLACK_HEIGHT_RATIO 0.65f
#define WHITE_BLACK_WIDTH_RATIO 0.6f

#define SCROLL_RECT_CAP 10

#define SCROLL_SPEED 200
#define KEY_SCROLL_RECT_OFFSET 5

#define FONT_SIZE 64

#define GAIN_MAX 10.f

float padding = 1.0f;

float whiteKey_width;
float whiteKey_height;

float blackKey_width;
float blackKey_height;

float bottom_offset = 0.f;
float left_offset = 0.f;

Image perlin_image;
Texture perlin_texture;

int perlin_offset_x;
int perlin_offset_y;

float perlin_dt = 0.f;

int wk_perlin_threshold_loc;
int bk_perlin_threshold_loc;
float wk_perlin_threshold = 0.6f;
float bk_perlin_threshold = 0.4f;
float wk_perlin_threshold_mult = 0.2f;
float bk_perlin_threshold_mult = -0.2f;

typedef struct {
    const char *file_path;
    int duration;
    int total_ticks;
    float progress;
} MidiPiece;

typedef struct {
    Rectangle bounds;
    bool hovered;
} UserInterfaceItem;

typedef struct {
    Rectangle bounds;
    UserInterfaceItem slider;
} Timeline;

typedef struct {
    Rectangle bounds;
    UserInterfaceItem slider;
} VolumeSlider;

typedef struct {
    VolumeSlider volume_slider;
    VolumeSlider timeline;
} UserInterface;

typedef struct {
    Rectangle rect;
    bool finished;
} ScrollRect;

typedef struct {
    ScrollRect *scrollRects;
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
    Key *white_keys[N_WHITE_KEYS];
    Key *black_keys[N_BLACK_KEYS];
    Key *last_pressed_key;


    Font font;
    MidiPiece current_piece;
    bool new_piece_loaded;

    //fluidsynth
    fluid_settings_t *fs_settings;
    fluid_synth_t *fs_synth;
    fluid_audio_driver_t *fs_audio_driver;
    fluid_player_t *fs_player;
    int sound_font_id;

    Shader wk_shader;
    Shader bk_shader;

    UserInterface ui;
} Plug;

static Plug *p = NULL;


void init_sr_array(ScrollRects *arr, size_t initialCapacity) {
    arr->scrollRects = malloc(initialCapacity * sizeof(ScrollRect));
    arr->size = 0;
    arr->capacity = initialCapacity;
}

void append_scroll_rect(ScrollRects *arr, ScrollRect sr) {
    if (arr->size == arr->capacity) {
        arr->capacity *= 2;
        arr->scrollRects = realloc(arr->scrollRects, arr->capacity * sizeof(ScrollRect));
    }
    arr->scrollRects[arr->size++] = sr;
}

void delete_scroll_rect(ScrollRects *arr, size_t index) {
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

void *plug_pre_reload(void) {
    return p;
}

void plug_post_reload(Plug *pP) {
    p = pP;
}

void calculate_key_rects(void) {
    whiteKey_width = (((float) GetScreenWidth() - left_offset) - (padding * (N_WHITE_KEYS - 1))) / N_WHITE_KEYS;
    whiteKey_height = whiteKey_width * WHITE_KEY_WH_RATIO;

    blackKey_width = whiteKey_width * WHITE_BLACK_WIDTH_RATIO;
    blackKey_height = whiteKey_height * WHITE_BLACK_HEIGHT_RATIO;

    float small_offset = whiteKey_width + padding;
    for (size_t i = 0; i < N_KEYS; i++) {
        if (p->keys[i].white) {
            p->keys[i].key_rect.x = (float) p->keys[i].color_index * (whiteKey_width + padding) + left_offset;
            p->keys[i].key_rect.y = (float) GetScreenHeight() - whiteKey_height - bottom_offset;
            p->keys[i].key_rect.width = whiteKey_width;
            p->keys[i].key_rect.height = whiteKey_height;

        } else {
            p->keys[i].key_rect.x =
                    (p->keys[i - 1].color_index + 1) * small_offset - blackKey_width / 2.f + left_offset;
            p->keys[i].key_rect.y = (float) GetScreenHeight() - whiteKey_height - bottom_offset;
            p->keys[i].key_rect.width = blackKey_width;
            p->keys[i].key_rect.height = blackKey_height;
        }

        for (size_t j = 0; j < p->keys[i].scroll_rects.size; j++) {
            if (p->keys[i].white) {
                p->keys[i].scroll_rects.scrollRects[j].rect.width = whiteKey_width;
            } else {
                p->keys[i].scroll_rects.scrollRects[j].rect.width = blackKey_width;
            }
            p->keys[i].scroll_rects.scrollRects[j].rect.x = p->keys[i].key_rect.x;
        }
    }
}

int player_midi_callback(void *data, fluid_midi_event_t *event) {
    (void) data;
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

int player_tick_callback(void *data, int tick) {
    (void) data;
    (void) tick;
    // fluid_player_t* player = (fluid_player_t*)data;
    if (!p->new_piece_loaded) {
        p->current_piece.total_ticks = fluid_player_get_total_ticks(p->fs_player);
        p->current_piece.progress = 0.f;
        p->current_piece.duration = (float) (p->current_piece.total_ticks / fluid_player_get_division(p->fs_player)) /
                                    (float) fluid_player_get_bpm(p->fs_player) * 60.f;
        p->new_piece_loaded = true;
    }
    return FLUID_OK;
}

void init_ui(void) {
    int screen_width = GetScreenWidth();
    int screen_height = GetScreenHeight();
    float slider_padding;
    float slider_height;

    p->ui.timeline.bounds = CLITERAL(Rectangle)
    {
        .x = 0,
        .y = screen_height * 0.9f,
        .width = screen_width * 0.8f,
        .height = screen_height * 0.1f
    };

    slider_padding = p->ui.timeline.bounds.width * 0.1f / 2.f;
    slider_height = p->ui.timeline.bounds.height * 0.1f;
    p->ui.timeline.slider.bounds = CLITERAL(Rectangle)
    {
        .x = p->ui.timeline.bounds.x + slider_padding,
        .y = p->ui.timeline.bounds.y + (p->ui.timeline.bounds.height / 2.f) - (slider_height / 2.f),
        .width = p->ui.timeline.bounds.width - 2.f * slider_padding,
        .height = slider_height
    };

    p->ui.volume_slider.bounds = CLITERAL(Rectangle)
    {
        .x = p->ui.timeline.bounds.width,
        .y = p->ui.timeline.bounds.y,
        .width = screen_width - p->ui.timeline.bounds.width,
        .height = p->ui.timeline.bounds.height
    };

    slider_padding = p->ui.volume_slider.bounds.width * 0.1f / 2.f;
    slider_height = p->ui.volume_slider.bounds.height * 0.1f;
    p->ui.volume_slider.slider.bounds = CLITERAL(Rectangle)
    {
        .x = p->ui.volume_slider.bounds.x + slider_padding,
        .y = p->ui.volume_slider.bounds.y + (p->ui.volume_slider.bounds.height / 2.f) - (slider_height / 2.f),
        .width = p->ui.volume_slider.bounds.width - 2.f * slider_padding,
        .height = slider_height
    };
    bottom_offset = screen_height - p->ui.timeline.bounds.y;
    // left_offset = 100.f;
}


void init_keys(void) {
    whiteKey_width = (((float) GetScreenWidth() - left_offset) - (padding * (N_WHITE_KEYS - 1))) / N_WHITE_KEYS;
    whiteKey_height = whiteKey_width * WHITE_KEY_WH_RATIO;

    blackKey_width = whiteKey_width * WHITE_BLACK_WIDTH_RATIO;
    blackKey_height = whiteKey_height * WHITE_BLACK_HEIGHT_RATIO;

    float small_offset = whiteKey_width + padding;
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
            p->keys[i].key_rect.x = (float) p->keys[i].color_index * (whiteKey_width + padding) + left_offset;
            p->keys[i].key_rect.y = (float) GetScreenHeight() - whiteKey_height - bottom_offset;
            p->keys[i].key_rect.width = whiteKey_width;
            p->keys[i].key_rect.height = whiteKey_height;

            p->white_keys[white_index] = &p->keys[i];
            white_index++;
        } else {
            p->keys[i].white = false;
            p->keys[i].color_index = black_index;

            // key rect
            p->keys[i].key_rect.x =
                    (p->keys[i - 1].color_index + 1) * small_offset - blackKey_width / 2.f + left_offset;
            p->keys[i].key_rect.y = (float) GetScreenHeight() - whiteKey_height - bottom_offset;
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
    assert(p->fs_settings != NULL && "Buy more RAM lol");

    p->fs_synth = new_fluid_synth(p->fs_settings);
    assert(p->fs_synth != NULL && "Buy more RAM lol");

    p->fs_audio_driver = new_fluid_audio_driver(p->fs_settings, p->fs_synth);
    assert(p->fs_audio_driver != NULL && "Buy more RAM lol");
}

void plug_init(void) {
    p = malloc(sizeof(*p));
    assert(p != NULL && "Buy more RAM lol");
    memset(p, 0, sizeof(*p));

    p->font = LoadFontEx("../resources/fonts/LouisGeorgeCafe.ttf", FONT_SIZE, NULL, 0);
    GenTextureMipmaps(&p->font.texture);
    SetTextureFilter(p->font.texture, TEXTURE_FILTER_BILINEAR);

    init_ui();
    init_keys();
    init_fluid_synth();

    p->wk_shader = LoadShader(NULL, "../resources/shaders/white_keys.frag");
    p->bk_shader = LoadShader(NULL, "../resources/shaders/black_keys.frag");

    wk_perlin_threshold_loc = GetShaderLocation(p->wk_shader, "perlin_treshold");
    bk_perlin_threshold_loc = GetShaderLocation(p->bk_shader, "perlin_treshold");

    int monitor_width = GetMonitorWidth(GetCurrentMonitor());
    int monitor_height = GetMonitorHeight(GetCurrentMonitor());
    perlin_image = GenImagePerlinNoise(monitor_width, monitor_height, 0, 0, 5);
    perlin_texture = LoadTextureFromImage(perlin_image);

    perlin_offset_x = (monitor_width - GetScreenWidth()) / 2;
    perlin_offset_y = (monitor_height - GetScreenHeight()) / 2;

    // default_texture = CLITERAL(Texture){ rlGetTextureIdDefault(), 1, 1, 1, PIXELFORMAT_UNCOMPRESSED_R8G8B8 };

}

void plug_clean(void) {
    delete_fluid_audio_driver(p->fs_audio_driver);
    delete_fluid_player(p->fs_player);
    delete_fluid_synth(p->fs_synth);
    delete_fluid_settings(p->fs_settings);
    UnloadFont(p->font);
    UnloadTexture(perlin_texture);
    UnloadImage(perlin_image);
    UnloadShader(p->wk_shader);
    UnloadShader(p->bk_shader);
    free(p);
}

float volume_to_pos(float vol) {
    if (vol <= 1.f) {
        return log2f(vol + 1) / 2;
    }
    return (vol - 1) / (2 * (GAIN_MAX - 1)) + 0.5f;
}

float pos_to_volume(float pos) {
    if (pos <= 0.5f) {
        return powf(2.f, 2.f * pos) - 1;
    }
    return (pos - 0.5f) * 2 * (GAIN_MAX - 1) + 1;
}

void render_key(Key *key) {
    Color topColor;
    Color bottomColor;

    if (key->white) {
        topColor = WHITE;
        bottomColor = key->pressed ? GRAY : topColor;
        DrawRectangleGradientV(key->key_rect.x, key->key_rect.y, key->key_rect.width, key->key_rect.height, topColor,
                               bottomColor);
    } else {
        topColor = CLITERAL(Color)
        { 50, 50, 50, 255 };
        bottomColor = key->pressed ? BLACK : topColor;
        DrawRectangleGradientV(key->key_rect.x, key->key_rect.y, key->key_rect.width, key->key_rect.height, topColor,
                               bottomColor);
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

// handles mouse input, creating scroll rects and playing notes with fluidsynth if a key was newly pressed
void update_keys() {
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
        if (!black_pressed) {
            for (size_t i = 0; i < N_WHITE_KEYS; i++) {
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
    wk_perlin_threshold += wk_perlin_threshold_mult * GetFrameTime();
    if (wk_perlin_threshold > 0.9f || wk_perlin_threshold < 0.1f) {
        wk_perlin_threshold_mult *= -1.f;
    }
    bk_perlin_threshold += bk_perlin_threshold_mult * GetFrameTime();
    if (bk_perlin_threshold > 0.9f || bk_perlin_threshold < 0.1f) {
        bk_perlin_threshold_mult *= -1.f;
    }
    SetShaderValue(p->wk_shader, wk_perlin_threshold_loc, &wk_perlin_threshold, SHADER_UNIFORM_FLOAT);
    SetShaderValue(p->bk_shader, bk_perlin_threshold_loc, &bk_perlin_threshold, SHADER_UNIFORM_FLOAT);

    Rectangle current_rect;
    Rectangle source_rect;
    perlin_dt += GetFrameTime();
    float source_offset_x = sinf(perlin_dt) * 320 + perlin_offset_x;
    float source_offset_y = cosf(perlin_dt) * 180 + perlin_offset_y;

    BeginShaderMode(p->wk_shader);
    for (size_t i = 0; i < N_WHITE_KEYS; i++) {
        for (size_t j = 0; j < p->white_keys[i]->scroll_rects.size; j++) {
            current_rect = p->white_keys[i]->scroll_rects.scrollRects[j].rect;
            source_rect = CLITERAL(Rectangle)
            {
                .x = current_rect.x + source_offset_x,
                .y = current_rect.y + source_offset_y,
                .width = current_rect.width,
                .height = current_rect.height
            };
            DrawTexturePro(perlin_texture, source_rect, current_rect, CLITERAL(Vector2)
            { 0, 0 }, 0.f, WHITE);
            DrawRectangleRoundedLines(current_rect, 0.5f, 5, 2, WHITE);
        }
    }
    EndShaderMode();

    BeginShaderMode(p->bk_shader);
    for (size_t i = 0; i < N_BLACK_KEYS; i++) {
        for (size_t j = 0; j < p->black_keys[i]->scroll_rects.size; j++) {
            current_rect = p->black_keys[i]->scroll_rects.scrollRects[j].rect;
            source_rect = CLITERAL(Rectangle)
            {
                current_rect.x + source_offset_x, current_rect.y +
                                                  source_offset_y, current_rect.width, current_rect.height
            };
            DrawTexturePro(perlin_texture, source_rect, current_rect, CLITERAL(Vector2)
            { 0, 0 }, 0.f, WHITE);
        }
    }
    EndShaderMode();

    // For some reason the color of DrawRectangleRoundedLines is always white in the shader, so I have to render the black ones separately
    for (size_t i = 0; i < N_BLACK_KEYS; i++) {
        for (size_t j = 0; j < p->black_keys[i]->scroll_rects.size; j++) {
            current_rect = p->black_keys[i]->scroll_rects.scrollRects[j].rect;
            DrawRectangleRoundedLines(current_rect, 0.5f, 5, 2, BLACK);
        }
    }
}

void update_scroll_rects() {
    float dt = GetFrameTime();
    float offset = SCROLL_SPEED * dt;
    size_t n_scroll_rects = 0;
    ScrollRect *current_rects = NULL;

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

void render_timeline(void) {
    DrawRectangleRec(p->ui.timeline.bounds, RED);
    DrawRectangleRec(p->ui.timeline.slider.bounds, WHITE);

    if (p->new_piece_loaded) {
        float progress = (float) fluid_player_get_current_tick(p->fs_player) / (float) p->current_piece.total_ticks;
        Vector2 progress_start =
                {
                    .x = p->ui.timeline.slider.bounds.x,
                    .y = p->ui.timeline.slider.bounds.y + p->ui.timeline.slider.bounds.height / 2.f
                };
        Vector2 progress_end =
                {
                        .x = p->ui.timeline.slider.bounds.x + progress * p->ui.timeline.slider.bounds.width,
                        .y = progress_start.y
                };

        DrawLineEx(progress_start, progress_end, 3, BLACK);
        DrawCircle(progress_end.x, progress_end.y, 5, p->ui.timeline.slider.hovered ? BLUE : GREEN);
    }
}

void render_volume_slider(void) {
    DrawRectangleRec(p->ui.volume_slider.bounds, BLUE);
    DrawRectangleRec(p->ui.volume_slider.slider.bounds, WHITE);
    float volume = fluid_synth_get_gain(p->fs_synth);
    float pos = volume_to_pos(volume);
    Vector2 volume_start =
            {
                .x = p->ui.volume_slider.slider.bounds.x,
                .y = p->ui.volume_slider.slider.bounds.y + p->ui.volume_slider.slider.bounds.height / 2.f
            };
    Vector2 volume_end =
            {
                .x = p->ui.volume_slider.slider.bounds.x + pos * p->ui.volume_slider.slider.bounds.width,
                .y = volume_start.y
            };
    DrawLineEx(volume_start, volume_end, 3, BLACK);
    DrawCircle(volume_end.x, volume_end.y, 5, p->ui.volume_slider.slider.hovered ? RED : GREEN);
}

void render_ui(void) {
    render_timeline();
    render_volume_slider();
}

void update_ui(void) {
    Vector2 mouse_position = GetMousePosition();

    // handle the volume slider
    if (CheckCollisionPointRec(mouse_position, p->ui.volume_slider.slider.bounds)) {
        p->ui.volume_slider.slider.hovered = true;
        if (IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
            float pos_normal = (mouse_position.x - p->ui.volume_slider.slider.bounds.x) / p->ui.volume_slider.slider.bounds.width;
            float volume = pos_to_volume(pos_normal);
            fluid_synth_set_gain(p->fs_synth, volume);
        }
    } else {
        p->ui.volume_slider.slider.hovered = false;
    }

    // handle the timeline
    if (p->new_piece_loaded) {
        if (CheckCollisionPointRec(mouse_position, p->ui.timeline.slider.bounds)) {
            p->ui.timeline.slider.hovered = true;
            if (IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
                float pos_normal = (mouse_position.x - p->ui.timeline.slider.bounds.x) / p->ui.timeline.slider.bounds.width;
                int ticks = (int)(pos_normal * (float)p->current_piece.total_ticks);
                reset_keys();
                fluid_synth_all_notes_off(p->fs_synth, -1);
                fluid_player_seek(p->fs_player, ticks);
            }
        } else {
            p->ui.timeline.slider.hovered = false;
        }
    }
}

void handle_dropped_file(void) {
    FilePathList dropped_files = LoadDroppedFiles();

    const char *file0 = dropped_files.paths[0];
    if (fluid_is_midifile(file0)) {
        if (p->fs_player != NULL) {
            fluid_player_stop(p->fs_player);
            delete_fluid_player(p->fs_player);
            fluid_synth_all_notes_off(p->fs_synth, -1);
            reset_keys();
        }

        p->fs_player = new_fluid_player(p->fs_synth);
        assert(p->fs_player != NULL && "Failed to make new Fluid player");
        fluid_player_set_playback_callback(p->fs_player, player_midi_callback, NULL);
        fluid_player_set_tick_callback(p->fs_player, player_tick_callback, NULL);
        fluid_player_add(p->fs_player, file0);

        fluid_player_set_loop(p->fs_player, -1);
        fluid_player_play(p->fs_player);

        p->current_piece.file_path = strdup(file0);

        p->new_piece_loaded = false;
        TraceLog(LOG_INFO, "MIDI: Midi file loaded: %s Press P to play/pause", file0);

    } else if (fluid_is_soundfont(file0) && strcmp(".sf2", GetFileExtension(file0)) == 0) {
        p->sound_font_id = fluid_synth_sfload(p->fs_synth, file0, 1);
        TraceLog(LOG_INFO, "Sound Font ID: %d", p->sound_font_id);
        if (p->sound_font_id == FLUID_FAILED) {
            TraceLog(LOG_ERROR, "FLUIDSYNTH: failed to load soundfont [%s]", file0);
        } else {
            TraceLog(LOG_INFO, "FLUIDSYNTH: Loaded sound font file [%s]", file0);
        }
    } else {
        TraceLog(LOG_INFO, "MIDI: Unupported file fropped: %s", file0);
    }
    UnloadDroppedFiles(dropped_files);
}

void handle_user_input(void) {
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
        init_ui();
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

    render_ui();
    update_ui();

    if (fluid_synth_sfcount(p->fs_synth) == 0) {
        DrawTextEx(p->font, "No SoundFont file loaded (.sf2). Drag&Drop one to hear sound", CLITERAL(Vector2)
        { 50, 50 }, 20, 0, BLACK);
    }

    if (p->new_piece_loaded) {
        const char *file_name = GetFileName(p->current_piece.file_path);
        int total_min = p->current_piece.duration / 60;
        int total_sec = p->current_piece.duration % 60;

        int progress_total =
                (float) fluid_player_get_current_tick(p->fs_player) / (float) p->current_piece.total_ticks *
                p->current_piece.duration;
        int progress_min = progress_total / 60;
        int progress_sec = progress_total % 60;
        DrawTextEx(p->font, TextFormat("Current Piece: %s Time: %02d:%02d Progress: %02d:%02d", file_name,
                                       total_min,
                                       total_sec,
                                       progress_min,
                                       progress_sec),
                   CLITERAL(Vector2)
        { 50, 100 }, 20, 0, BLACK);
    }
    DrawFPS(10, 10);
    EndDrawing();
    handle_user_input();
}
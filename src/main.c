#include <stdio.h>
#ifdef _WIN32
#include "../WinDependencies/include/raylib.h"
#else
#include <raylib.h>
#endif
#include "hotreload.h"


int main(void) {

    if (!reload_libplug()) return 1;

    size_t factor = 80;
    SetConfigFlags(FLAG_WINDOW_RESIZABLE);
    InitWindow(factor*16, factor*9, "Pianolizer");
    SetTargetFPS(144);

    plug_init();

    while (!WindowShouldClose()) {
        if (IsKeyPressed(KEY_R)) {
            void* state = plug_pre_reload();
            if (!reload_libplug()) return 1;
            plug_post_reload(state);
        }
        plug_update();
    }
    CloseWindow();
    return 0;
}

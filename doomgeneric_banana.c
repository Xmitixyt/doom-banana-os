#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <banana.h>
#include "doomgeneric.h"
#include "doomkeys.h"

#define WIN_W 640
#define WIN_H 400

static bwin_t s_win;

#define KEYQUEUE_SIZE 256
static struct {
    int pressed;
    unsigned char key;
} s_key_queue[KEYQUEUE_SIZE];
static int s_qhead = 0;
static int s_qtail = 0;

static void queue_key(int pressed, unsigned char key) {
    int next = (s_qhead + 1) % KEYQUEUE_SIZE;
    if (next != s_qtail) {
        s_key_queue[s_qhead].pressed = pressed;
        s_key_queue[s_qhead].key = key;
        s_qhead = next;
    }
}

/* Key hold tracking: Banana OS sends keypress on typed chars without keyup.
 * PC keyboards have ~500ms initial typematic delay, followed by ~30ms repeats.
 * We keep keys held for INITIAL_MS on first press, and REPEAT_MS on repeats. */
#define KEY_HOLD_INITIAL_MS 650
#define KEY_HOLD_REPEAT_MS  280

static uint8_t  s_key_down[256];
static uint32_t s_key_expire[256];
static uint32_t s_key_repeat[256];
static uint32_t s_pad_prev = 0;
static int s_last_mouse_x = -1;
static int s_last_mouse_y = -1;
static int s_mouse_buttons = 0;
static int s_mouse_speed = 4;
static int s_mouse_captured = 0;
static int s_show_cursor = 0;

static void DG_RestoreCursor(void) {
    if (s_mouse_captured && BANANA_HAS(__banana, win_capture_mouse)) {
        bwin_capture_mouse(&s_win, 0);
        s_mouse_captured = 0;
        DG_Log("DG_RestoreCursor: mouse released.\n");
    }
}

static void DG_HideCursor(void) {
    /* Use API v20 relative mouse capture: hides cursor, suspends VM absolute integration, enables 360 look */
    if (BANANA_HAS(__banana, win_capture_mouse)) {
        if (bwin_capture_mouse(&s_win, 1) == 0) {
            s_mouse_captured = 1;
            DG_Log("DG_HideCursor: native relative mouse capture active (API v20)\n");
            printf("DOOM: Capture souris relative activee (API v20)\n");
            return;
        }
    }
    DG_Log("DG_HideCursor: API v20 capture unavailable, fallback mode active.\n");
}

static void release_key(unsigned char dk) {
    if (!dk) return;
    if (s_key_down[dk]) {
        s_key_down[dk] = 0;
        s_key_repeat[dk] = 0;
        s_key_expire[dk] = 0;
        queue_key(0, dk);
    }
}

static void press_key(unsigned char dk) {
    if (!dk) return;
    uint32_t now = banana_ticks();

    /* Directional opposing cancellation for instant responsiveness */
    if (dk == KEY_UPARROW)    release_key(KEY_DOWNARROW);
    if (dk == KEY_DOWNARROW)  release_key(KEY_UPARROW);
    if (dk == KEY_LEFTARROW)  release_key(KEY_RIGHTARROW);
    if (dk == KEY_RIGHTARROW) release_key(KEY_LEFTARROW);
    if (dk == KEY_STRAFE_L)   release_key(KEY_STRAFE_R);
    if (dk == KEY_STRAFE_R)   release_key(KEY_STRAFE_L);

    if (!s_key_down[dk]) {
        s_key_down[dk] = 1;
        s_key_repeat[dk] = 0;
        s_key_expire[dk] = now + KEY_HOLD_INITIAL_MS;
        queue_key(1, dk);
    } else {
        s_key_repeat[dk]++;
        s_key_expire[dk] = now + KEY_HOLD_REPEAT_MS;
    }
}

static void check_key_timeouts(void) {
    uint32_t now = banana_ticks();
    for (int k = 0; k < 256; k++) {
        if (s_key_down[k] && now >= s_key_expire[k]) {
            s_key_down[k] = 0;
            s_key_repeat[k] = 0;
            s_key_expire[k] = 0;
            queue_key(0, (unsigned char)k);
        }
    }
}

static unsigned char map_key(int key) {
    switch (key) {
        /* Arrow keys */
        case BANANA_KEY_UP:    return KEY_UPARROW;
        case BANANA_KEY_DOWN:  return KEY_DOWNARROW;
        case BANANA_KEY_LEFT:  return KEY_LEFTARROW;
        case BANANA_KEY_RIGHT: return KEY_RIGHTARROW;

        /* Controls / Escape / Enter */
        case 27:               return KEY_ESCAPE;
        case 10:
        case 13:               return KEY_ENTER;
        case '\t':             return KEY_TAB;
        case ' ':              return KEY_USE;
        case 8:
        case 127:              return KEY_BACKSPACE;

        /* QWERTY & AZERTY Movement:
         * Z / W = Avancer (Forward)
         * S = Reculer (Backward)
         * Q / A = Tourner à gauche (Turn Left)
         * D = Tourner à droite (Turn Right) */
        case 'w': case 'W':
        case 'z': case 'Z':    return KEY_UPARROW;

        case 's': case 'S':    return KEY_DOWNARROW;

        case 'a': case 'A':
        case 'q': case 'Q':    return KEY_LEFTARROW;

        case 'd': case 'D':    return KEY_RIGHTARROW;

        /* Strafe (Pas chassés) */
        case ',': case '<':    return KEY_STRAFE_L;
        case '.': case '>':    return KEY_STRAFE_R;

        /* Action / Ouvrir portes */
        case 'e': case 'E':    return KEY_USE;

        /* Tirer (Fire) */
        case 'f': case 'F':
        case 'x': case 'X':
        case 'c': case 'C':    return KEY_FIRE;

        /* Menu confirm / abort */
        case 'y': case 'Y':
        case 'o': case 'O':    return 'y';
        case 'n': case 'N':    return 'n';

        /* Weapons 1-7 (QWERTY & AZERTY unshifted top-row numbers) */
        case '1': case '&':    return '1';
        case '2': case 233:    return '2'; /* 'é' (0xE9 = 233) */
        case '3': case '"':    return '3';
        case '4': case '\'':   return '4';
        case '5': case '(':    return '5';
        case '6': case '-':    return '6';
        case '7': case 232:    return '7'; /* 'è' (0xE8 = 232) */

        default:
            if (key >= '1' && key <= '9') return (unsigned char)key;
            if (key >= 'a' && key <= 'z') return (unsigned char)key;
            if (key >= 'A' && key <= 'Z') return (unsigned char)(key + 32);
            return 0;
    }
}

extern const banana_api_t* __banana;

typedef struct {
    unsigned char doom_key;
    int is_down;
    int banana_keys[5];
} action_key_map_t;

static action_key_map_t s_action_keys[] = {
    { KEY_UPARROW,    0, {'w', 'z', BANANA_KEY_UP, 0} },
    { KEY_DOWNARROW,  0, {'s', BANANA_KEY_DOWN, 0, 0} },
    { KEY_LEFTARROW,  0, {'a', 'q', BANANA_KEY_LEFT, 0} },
    { KEY_RIGHTARROW, 0, {'d', BANANA_KEY_RIGHT, 0, 0} },
    { KEY_STRAFE_L,   0, {',', '<', 0, 0} },
    { KEY_STRAFE_R,   0, {'.', '>', 0, 0} },
    { KEY_USE,        0, {' ', 'e', 'E', 0} },
    { KEY_FIRE,       0, {BANANA_KEY_CTRL, 'f', 'x', 'c', 0} },
    { KEY_RSHIFT,     0, {BANANA_KEY_SHIFT, 0, 0, 0} },
    { KEY_RALT,       0, {BANANA_KEY_ALT, 0, 0, 0} },
    { KEY_ESCAPE,     0, {27, 0, 0, 0} },
    { KEY_ENTER,      0, {'\n', '\r', 0, 0} },
    { KEY_TAB,        0, {'\t', 0, 0, 0} },
    { KEY_BACKSPACE,  0, {'\b', 127, 0, 0} },
    { '1',            0, {'1', '&', 0, 0} },
    { '2',            0, {'2', 233, 0, 0} }, /* 'é' (0xE9) */
    { '3',            0, {'3', '"', 0, 0} },
    { '4',            0, {'4', '\'', 0, 0} },
    { '5',            0, {'5', '(', 0, 0} },
    { '6',            0, {'6', '-', 0, 0} },
    { '7',            0, {'7', 232, 0, 0} }, /* 'è' (0xE8) */
    { 'y',            0, {'y', 'o', 0, 0} },
    { 'n',            0, {'n', 0, 0, 0} },
};

static int is_action_key(int key) {
    for (int i = 0; i < (int)(sizeof(s_action_keys)/sizeof(s_action_keys[0])); i++) {
        for (int k = 0; k < 5 && s_action_keys[i].banana_keys[k] != 0; k++) {
            if (s_action_keys[i].banana_keys[k] == key) return 1;
        }
    }
    return 0;
}

static void check_multi_keys(void) {
    if (!__banana || __banana->version < 14) return;

    for (int i = 0; i < (int)(sizeof(s_action_keys)/sizeof(s_action_keys[0])); i++) {
        action_key_map_t* ak = &s_action_keys[i];
        int currently_down = 0;
        for (int k = 0; k < 5 && ak->banana_keys[k] != 0; k++) {
            if (banana_key_down(ak->banana_keys[k])) {
                currently_down = 1;
                break;
            }
        }
        if (currently_down && !ak->is_down) {
            ak->is_down = 1;
            queue_key(1, ak->doom_key);
        } else if (!currently_down && ak->is_down) {
            ak->is_down = 0;
            queue_key(0, ak->doom_key);
        }
    }
}

void DG_Log(const char* fmt, ...) {
    FILE* f = fopen("/home/banana/doom.log", "a");
    if (f) {
        va_list ap;
        va_start(ap, fmt);
        vfprintf(f, fmt, ap);
        va_end(ap);
        fclose(f);
    }
}

void DG_Init(void) {
    int fullscreen = 0;
    extern int myargc;
    extern char **myargv;
    for (int i = 1; i < myargc; i++) {
        if (!strcmp(myargv[i], "-fullscreen") || !strcmp(myargv[i], "-fs") || !strcmp(myargv[i], "-f")) {
            fullscreen = 1;
        } else if (!strcmp(myargv[i], "-showcursor") || !strcmp(myargv[i], "-cursor")) {
            s_show_cursor = 1;
        } else if ((!strcmp(myargv[i], "-ms") || !strcmp(myargv[i], "-mousespeed")) && i + 1 < myargc) {
            int spd = atoi(myargv[++i]);
            if (spd > 0 && spd < 100) s_mouse_speed = spd;
        }
    }
    int req_w = fullscreen ? 4000 : WIN_W;
    int req_h = fullscreen ? 4000 : WIN_H;

    DG_Log("DG_Init: opening window %dx%d (fullscreen=%d, mousespeed=%d, showcursor=%d)...\n", req_w, req_h, fullscreen, s_mouse_speed, s_show_cursor);
    if (bwin_open(&s_win, "DOOM - Banana OS", req_w, req_h) != 0) {
        DG_Log("DG_Init ERROR: Failed to open desktop window! (Desktop/GUI not active? Type startx)\n");
        printf("DOOM: Failed to open desktop window (start desktop with startx first)\n");
        exit(1);
    }
    /* Allow user to resize, drag borders or click [Maximize] in title bar */
    bwin_resizable(&s_win, 320, 200);
    /* Enable mouse wheel events */
    bwin_wheel(&s_win);

    /* Hide mouse pointer while DOOM is playing unless requested otherwise */
    if (!s_show_cursor) {
        DG_HideCursor();
        atexit(DG_RestoreCursor);
    } else {
        printf("DOOM: Curseur souris visible (-showcursor active)\n");
    }

    DG_Log("DG_Init: window opened id=%d px=%p w=%d h=%d\n", s_win.id, (void*)s_win.px, s_win.w, s_win.h);
    bwin_clear(&s_win, BANANA_BLACK);
    bwin_update(&s_win);
    DG_Log("DG_Init: window initialized successfully.\n");
}

void DG_PollEvents(void) {
    if (s_win.id < 0) return;

    int has_multi_key = (__banana != NULL && __banana->version >= 14);
    if (has_multi_key) {
        check_multi_keys();
    }

    banana_event_t ev;
    while (bwin_event(&s_win, &ev)) {
        if (ev.type == BANANA_EV_CLOSE) {
            DG_Log("DOOM: Window closed by user.\n");
            DG_RestoreCursor();
            bwin_close(&s_win);
            exit(0);
        } else if (ev.type == BANANA_EV_RESIZE) {
            /* Clear window on resize so aspect ratio borders stay black */
            bwin_clear(&s_win, BANANA_BLACK);
        } else if (ev.type == BANANA_EV_FOCUS) {
            /* Reset mouse coordinates when focus changes */
            s_last_mouse_x = -1;
            s_last_mouse_y = -1;
            if (ev.x && !s_mouse_captured && !s_show_cursor && BANANA_HAS(__banana, win_capture_mouse)) {
                if (bwin_capture_mouse(&s_win, 1) == 0) {
                    s_mouse_captured = 1;
                }
            }
        } else if (ev.type == BANANA_EV_CAPTURE) {
            s_mouse_captured = ev.x;
            if (ev.buttons & 1) s_mouse_buttons |= 1; else s_mouse_buttons &= ~1;
            if (ev.buttons & 2) s_mouse_buttons |= 2; else s_mouse_buttons &= ~2;
            DG_Log("DOOM: Mouse capture state: %d (buttons=%d)\n", s_mouse_captured, ev.buttons);
        } else if (ev.type == BANANA_EV_MOUSE_REL) {
            /* Native 360 relative mouse motion (Banana OS API v20 / Build 167+) */
            if (ev.buttons & 1) s_mouse_buttons |= 1; else s_mouse_buttons &= ~1;
            if (ev.buttons & 2) s_mouse_buttons |= 2; else s_mouse_buttons &= ~2;
            int dx = ev.x * s_mouse_speed;
            DG_SendMouseEvent(s_mouse_buttons, dx, 0);
        } else if (ev.type == BANANA_EV_WHEEL) {
            if (ev.y != 0) {
                DG_ChangeWeapon(ev.y > 0 ? 1 : -1);
            }
        } else if (ev.type == BANANA_EV_KEY) {
            if (!has_multi_key) {
                unsigned char dk = map_key(ev.key);
                if (dk) {
                    press_key(dk);
                }
            } else {
                /* In multi-key mode, only process keys that are not game action keys (e.g. typing) */
                if (!is_action_key(ev.key)) {
                    unsigned char dk = map_key(ev.key);
                    if (dk) {
                        queue_key(1, dk);
                        queue_key(0, dk);
                    }
                }
            }
        } else if (ev.type == BANANA_EV_MOUSE_DOWN) {
            /* If mouse capture was released (e.g. via Escape), click to re-capture! */
            if (!s_mouse_captured && !s_show_cursor && BANANA_HAS(__banana, win_capture_mouse)) {
                if (bwin_capture_mouse(&s_win, 1) == 0) {
                    s_mouse_captured = 1;
                }
            }
            if (ev.button == 1) {
                /* Left click: Fire! Held until mouse release */
                s_mouse_buttons |= 1;
                s_key_down[KEY_FIRE] = 1;
                s_key_expire[KEY_FIRE] = banana_ticks() + 10000000;
                queue_key(1, KEY_FIRE);
                DG_SendMouseEvent(s_mouse_buttons, 0, 0);
            } else if (ev.button == 2) {
                /* Right click: Use / Open door */
                press_key(KEY_USE);
            }
        } else if (ev.type == BANANA_EV_MOUSE_UP) {
            if (ev.button == 1) {
                s_mouse_buttons &= ~1;
                release_key(KEY_FIRE);
                DG_SendMouseEvent(s_mouse_buttons, 0, 0);
            }
        } else if (ev.type == BANANA_EV_MOUSE_MOVE) {
            /* Fallback delta motion when native relative capture is not active */
            if (!s_mouse_captured) {
                if (ev.buttons & 1) s_mouse_buttons |= 1;
                else s_mouse_buttons &= ~1;

                if (s_last_mouse_x >= 0 && s_last_mouse_y >= 0) {
                    int dx = ev.x - s_last_mouse_x;
                    int dy = ev.y - s_last_mouse_y;

                    if (dx > -300 && dx < 300 && dy > -300 && dy < 300) {
                        if (dx != 0) {
                            DG_SendMouseEvent(s_mouse_buttons, dx * s_mouse_speed, 0);
                        }
                    }
                }
                s_last_mouse_x = ev.x;
                s_last_mouse_y = ev.y;
            }
        }
    }

    /* Gamepad support with edge detection */
    banana_gamepad_t pad;
    if (banana_gamepad(0, &pad)) {
        uint32_t pressed  = pad.buttons & ~s_pad_prev;
        uint32_t released = ~pad.buttons & s_pad_prev;
        s_pad_prev = pad.buttons;

        if (pressed  & BANANA_PAD_A)     queue_key(1, KEY_FIRE);
        if (released & BANANA_PAD_A)     queue_key(0, KEY_FIRE);

        if (pressed  & BANANA_PAD_X)     queue_key(1, KEY_USE);
        if (released & BANANA_PAD_X)     queue_key(0, KEY_USE);

        if (pressed  & BANANA_PAD_UP)    queue_key(1, KEY_UPARROW);
        if (released & BANANA_PAD_UP)    queue_key(0, KEY_UPARROW);

        if (pressed  & BANANA_PAD_DOWN)  queue_key(1, KEY_DOWNARROW);
        if (released & BANANA_PAD_DOWN)  queue_key(0, KEY_DOWNARROW);

        if (pressed  & BANANA_PAD_LEFT)  queue_key(1, KEY_LEFTARROW);
        if (released & BANANA_PAD_LEFT)  queue_key(0, KEY_LEFTARROW);

        if (pressed  & BANANA_PAD_RIGHT) queue_key(1, KEY_RIGHTARROW);
        if (released & BANANA_PAD_RIGHT) queue_key(0, KEY_RIGHTARROW);

        if (pressed  & BANANA_PAD_START) queue_key(1, KEY_ENTER);
        if (released & BANANA_PAD_START) queue_key(0, KEY_ENTER);
    }

    /* Expire keys only in fallback mode */
    if (!has_multi_key) {
        check_key_timeouts();
    }
}

void DG_DrawFrame(void) {
    static int s_first_frame = 1;
    if (s_first_frame) {
        s_first_frame = 0;
        DG_Log("DG_DrawFrame: first frame being drawn to screen!\n");
    }

    if (DG_ScreenBuffer && s_win.px && s_win.w > 0 && s_win.h > 0) {
        int dst_w = s_win.w;
        int dst_h = s_win.h;

        if (dst_w == WIN_W && dst_h == WIN_H) {
            /* Fast direct copy when 1:1 */
            memcpy(s_win.px, DG_ScreenBuffer, WIN_W * WIN_H * sizeof(uint32_t));
        } else {
            /* High performance aspect-ratio preserving scaler */
            int scale_w, scale_h;
            if (dst_w * WIN_H <= dst_h * WIN_W) {
                scale_w = dst_w;
                scale_h = (dst_w * WIN_H) / WIN_W;
            } else {
                scale_h = dst_h;
                scale_w = (dst_h * WIN_W) / WIN_H;
            }

            if (scale_w < 1) scale_w = 1;
            if (scale_h < 1) scale_h = 1;

            int ox = (dst_w - scale_w) / 2;
            int oy = (dst_h - scale_h) / 2;

            uint32_t x_ratio = ((uint32_t)WIN_W << 16) / (uint32_t)scale_w;
            uint32_t y_ratio = ((uint32_t)WIN_H << 16) / (uint32_t)scale_h;

            for (int y = 0; y < scale_h; y++) {
                const uint32_t* src_row = DG_ScreenBuffer + ((y * y_ratio) >> 16) * WIN_W;
                uint32_t* dst_row = s_win.px + (oy + y) * dst_w + ox;
                for (int x = 0; x < scale_w; x++) {
                    dst_row[x] = src_row[(x * x_ratio) >> 16];
                }
            }
        }
        bwin_update(&s_win);
    }

    DG_PollEvents();
}


void DG_SleepMs(uint32_t ms) {
    banana_sleep(ms);
}

uint32_t DG_GetTicksMs(void) {
    return banana_ticks();
}

int DG_GetKey(int* pressed, unsigned char* doomKey) {
    if (s_qhead == s_qtail) return 0;
    *pressed = s_key_queue[s_qtail].pressed;
    *doomKey = s_key_queue[s_qtail].key;
    s_qtail = (s_qtail + 1) % KEYQUEUE_SIZE;
    return 1;
}

void DG_SetWindowTitle(const char* title) {
    if (title && s_win.id >= 0) {
        bwin_title(&s_win, title);
    }
}

/* Helper to check if file exists */
static int file_exists(const char* path) {
    FILE* f = fopen(path, "rb");
    if (f) {
        fclose(f);
        return 1;
    }
    return 0;
}

/* system() stub for Banana OS libc */
int system(const char* cmd) {
    (void)cmd;
    return -1;
}

int main(int argc, char** argv) {
    printf("\n======================================================\n");
    printf("=== DOOM Banana OS v1.7 [Multi-Touches & Full WADs] ===\n");
    printf("======================================================\n");
    FILE* logf = fopen("/home/banana/doom.log", "w");
    if (logf) {
        fprintf(logf, "=== DOOM Banana OS Launcher v1.7 ===\n");
        fprintf(logf, "argc: %d\n", argc);
        for (int i = 0; i < argc; i++) fprintf(logf, "argv[%d]: %s\n", i, argv[i]);
        fclose(logf);
    }
    int selected_version = 0; /* 0 = auto, 1 = doom1, 2 = doom2, 3 = doom3 */
    char custom_wad[256] = "";

    for (int i = 1; i < argc; i++) {
        const char* arg = argv[i];
        if (strcmp(arg, "1") == 0 || strcmp(arg, "-1") == 0 || strcasecmp(arg, "doom1") == 0 || strcasecmp(arg, "doom1.wad") == 0) {
            selected_version = 1;
        } else if (strcmp(arg, "2") == 0 || strcmp(arg, "-2") == 0 || strcasecmp(arg, "doom2") == 0 || strcasecmp(arg, "doom2.wad") == 0) {
            selected_version = 2;
        } else if (strcmp(arg, "3") == 0 || strcmp(arg, "-3") == 0 || strcasecmp(arg, "doom3") == 0 || strcasecmp(arg, "doom3.wad") == 0 || strcasecmp(arg, "doom.wad") == 0) {
            selected_version = 3;
        } else if (arg[0] != '-') {
            strncpy(custom_wad, arg, sizeof(custom_wad) - 1);
        }
    }

    char wad_file[256] = "";

    if (custom_wad[0]) {
        strncpy(wad_file, custom_wad, sizeof(wad_file) - 1);
    } else if (selected_version == 1) {
        strcpy(wad_file, "/apps/doom/doom1.wad");
    } else if (selected_version == 2) {
        strcpy(wad_file, "/apps/doom/doom2.wad");
    } else if (selected_version == 3) {
        strcpy(wad_file, "/apps/doom/doom3.wad");
    } else {
        /* Default auto-selection: prefer full Doom 3 / Ultimate Doom, then Doom 2, then Doom 1 */
        if (file_exists("/apps/doom/doom3.wad")) {
            strcpy(wad_file, "/apps/doom/doom3.wad");
        } else if (file_exists("/apps/doom/doom2.wad")) {
            strcpy(wad_file, "/apps/doom/doom2.wad");
        } else if (file_exists("/apps/doom/doom1.wad")) {
            strcpy(wad_file, "/apps/doom/doom1.wad");
        } else {
            const char* candidate_wads[] = {
                "/apps/doom/doom3.wad", "/apps/doom/doom2.wad", "/apps/doom/doom1.wad",
                "doom3.wad", "doom2.wad", "doom1.wad", "doom.wad",
                "/home/banana/doom3.wad", "/home/banana/doom2.wad", "/home/banana/doom1.wad",
                NULL
            };
            for (int i = 0; candidate_wads[i]; i++) {
                if (file_exists(candidate_wads[i])) {
                    strncpy(wad_file, candidate_wads[i], sizeof(wad_file) - 1);
                    break;
                }
            }
        }
    }

    if (!wad_file[0]) {
        strcpy(wad_file, "/apps/doom/doom1.wad");
    }

    FILE* log2 = fopen("/home/banana/doom.log", "a");
    if (log2) {
        fprintf(log2, "Resolved IWAD: %s (exists=%d)\n", wad_file, file_exists(wad_file));
        fclose(log2);
    }

    char* doom_argv[16];
    int doom_argc = 0;

    doom_argv[doom_argc++] = argv[0];
    doom_argv[doom_argc++] = "-iwad";
    doom_argv[doom_argc++] = wad_file;
    for (int i = 1; i < argc && doom_argc < 15; i++) {
        if (argv[i][0] == '-' && strcmp(argv[i], "-1") != 0 && strcmp(argv[i], "-2") != 0 && strcmp(argv[i], "-3") != 0) {
            doom_argv[doom_argc++] = argv[i];
        }
    }
    doom_argv[doom_argc] = NULL;

    printf("DOOM: Starting with WAD: %s\n", wad_file);
    DG_Log("Calling doomgeneric_Create with argc=%d...\n", doom_argc);
    doomgeneric_Create(doom_argc, doom_argv);
    DG_Log("doomgeneric_Create returned! Entering game loop...\n");

    uint32_t frame_count = 0;
    while (1) {
        doomgeneric_Tick();
        frame_count++;
        if (frame_count == 1 || (frame_count % 350 == 0)) {
            DG_Log("doomgeneric_Tick running, frame %u\n", frame_count);
        }
    }

    DG_RestoreCursor();
    return 0;
}

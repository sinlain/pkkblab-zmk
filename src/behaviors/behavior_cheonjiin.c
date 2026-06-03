/*
 * Cheonjiin Korean input behavior for ZMK
 *
 * Behavior:
 * - Immediate input on first tap.
 * - If the same consonant key is tapped again within CJI_REPEAT_TERM_MS:
 *   Backspace previous character, then replace with next candidate.
 * - HID output is queued to a work item so the behavior callback does not sleep
 *   or send multi-step HID reports directly inside the key press handler.
 *
 * Host OS must be in Korean 2-beolsik input mode.
 */

#define DT_DRV_COMPAT sinlain_behavior_cheonjiin

#include <zephyr/device.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include <drivers/behavior.h>

#include <zmk/endpoints.h>
#include <zmk/hid.h>

#include <dt-bindings/zmk/cheonjiin.h>
#include <dt-bindings/zmk/hid_usage_pages.h>
#include <dt-bindings/zmk/keys.h>

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))
#endif

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#if DT_HAS_COMPAT_STATUS_OKAY(DT_DRV_COMPAT)

#define TAP_DELAY_MS 20
#define CJI_REPEAT_TERM_MS 1800
#define CJI_ACTION_QUEUE_SIZE 64
#define CJI_VOWEL_SEQ_MAX 5
#define CJI_VOWEL_OUT_MAX 2

enum cji_kind {
    CJI_KIND_NONE = 0,
    CJI_KIND_CONSONANT,
    CJI_KIND_VOWEL,
    CJI_KIND_DOT,
};

struct cji_state {
    uint32_t last_token;
    uint8_t repeat_count;
    enum cji_kind last_kind;
    int64_t last_tap_time;
    bool pending_dot;

    uint32_t vowel_seq[CJI_VOWEL_SEQ_MAX];
    uint8_t vowel_len;
    bool emitted_vowel;
    uint8_t emitted_vowel_out_len;
};

static struct cji_state state = {
    .last_token = 0,
    .repeat_count = 0,
    .last_kind = CJI_KIND_NONE,
    .last_tap_time = 0,
    .pending_dot = false,
    .vowel_len = 0,
    .emitted_vowel = false,
    .emitted_vowel_out_len = 0,
};

enum cji_action_type {
    CJI_ACTION_TAP = 0,
    CJI_ACTION_SHIFT_TAP,
};

struct cji_action {
    enum cji_action_type type;
    uint32_t keycode;
};

static struct cji_action action_queue[CJI_ACTION_QUEUE_SIZE];
static size_t action_head;
static size_t action_tail;
static size_t action_count;

K_MUTEX_DEFINE(action_mutex);

static void cji_work_handler(struct k_work *work);
K_WORK_DEFINE(cji_work, cji_work_handler);

static int send_report(void) {
    return zmk_endpoints_send_report(HID_USAGE_KEY);
}

static int press_key_now(uint32_t keycode) {
    int err = zmk_hid_press(ZMK_HID_USAGE(HID_USAGE_KEY, keycode));
    if (err < 0) {
        return err;
    }

    return send_report();
}

static int release_key_now(uint32_t keycode) {
    int err = zmk_hid_release(ZMK_HID_USAGE(HID_USAGE_KEY, keycode));
    if (err < 0) {
        return err;
    }

    return send_report();
}

static int tap_key_now(uint32_t keycode) {
    int err = press_key_now(keycode);
    if (err < 0) {
        return err;
    }

    k_sleep(K_MSEC(TAP_DELAY_MS));

    err = release_key_now(keycode);
    if (err < 0) {
        return err;
    }

    k_sleep(K_MSEC(TAP_DELAY_MS));
    return 0;
}

static int tap_shifted_key_now(uint32_t keycode) {
    int err = press_key_now(LEFT_SHIFT);
    if (err < 0) {
        return err;
    }

    k_sleep(K_MSEC(TAP_DELAY_MS));

    err = press_key_now(keycode);
    if (err < 0) {
        release_key_now(LEFT_SHIFT);
        return err;
    }

    k_sleep(K_MSEC(TAP_DELAY_MS));

    err = release_key_now(keycode);
    if (err < 0) {
        release_key_now(LEFT_SHIFT);
        return err;
    }

    k_sleep(K_MSEC(TAP_DELAY_MS));

    err = release_key_now(LEFT_SHIFT);
    if (err < 0) {
        return err;
    }

    k_sleep(K_MSEC(TAP_DELAY_MS));
    return 0;
}

static int enqueue_action(enum cji_action_type type, uint32_t keycode) {
    int err = k_mutex_lock(&action_mutex, K_MSEC(10));
    if (err < 0) {
        return err;
    }

    if (action_count >= CJI_ACTION_QUEUE_SIZE) {
        k_mutex_unlock(&action_mutex);
        return -ENOMEM;
    }

    action_queue[action_tail].type = type;
    action_queue[action_tail].keycode = keycode;
    action_tail = (action_tail + 1) % CJI_ACTION_QUEUE_SIZE;
    action_count++;

    k_mutex_unlock(&action_mutex);
    k_work_submit(&cji_work);

    return 0;
}

static bool dequeue_action(struct cji_action *action) {
    if (k_mutex_lock(&action_mutex, K_MSEC(10)) < 0) {
        return false;
    }

    if (action_count == 0) {
        k_mutex_unlock(&action_mutex);
        return false;
    }

    *action = action_queue[action_head];
    action_head = (action_head + 1) % CJI_ACTION_QUEUE_SIZE;
    action_count--;

    k_mutex_unlock(&action_mutex);
    return true;
}

static void cji_work_handler(struct k_work *work) {
    ARG_UNUSED(work);

    struct cji_action action;

    while (dequeue_action(&action)) {
        switch (action.type) {
        case CJI_ACTION_TAP:
            tap_key_now(action.keycode);
            break;

        case CJI_ACTION_SHIFT_TAP:
            tap_shifted_key_now(action.keycode);
            break;
        }
    }
}

static int tap_key(uint32_t keycode) {
    return enqueue_action(CJI_ACTION_TAP, keycode);
}

static int tap_shifted_key(uint32_t keycode) {
    return enqueue_action(CJI_ACTION_SHIFT_TAP, keycode);
}

static int tap_backspace(void) {
    return tap_key(BACKSPACE);
}

static void reset_state(void) {
    state.last_token = 0;
    state.repeat_count = 0;
    state.last_kind = CJI_KIND_NONE;
    state.last_tap_time = 0;
    state.pending_dot = false;
    state.vowel_len = 0;
    state.emitted_vowel = false;
    state.emitted_vowel_out_len = 0;
}

static bool is_within_term(void) {
    int64_t now = k_uptime_get();

    if (state.last_tap_time == 0) {
        return false;
    }

    return (now - state.last_tap_time) <= CJI_REPEAT_TERM_MS;
}

static void stamp_tap_time(void) {
    state.last_tap_time = k_uptime_get();
}

static int emit_consonant(uint32_t token, uint8_t repeat_count) {
    switch (token) {
    case CJI_G:
        if (repeat_count == 0) {
            return tap_key(R);          /* ㄱ */
        } else if (repeat_count == 1) {
            return tap_key(Z);          /* ㅋ */
        } else {
            return tap_shifted_key(R);  /* ㄲ */
        }

    case CJI_N:
        if (repeat_count == 0) {
            return tap_key(S);          /* ㄴ */
        } else {
            return tap_key(F);          /* ㄹ */
        }

    case CJI_D:
        if (repeat_count == 0) {
            return tap_key(E);          /* ㄷ */
        } else if (repeat_count == 1) {
            return tap_key(X);          /* ㅌ */
        } else {
            return tap_shifted_key(E);  /* ㄸ */
        }

    case CJI_B:
        if (repeat_count == 0) {
            return tap_key(Q);          /* ㅂ */
        } else if (repeat_count == 1) {
            return tap_key(V);          /* ㅍ */
        } else {
            return tap_shifted_key(Q);  /* ㅃ */
        }

    case CJI_S:
        if (repeat_count == 0) {
            return tap_key(T);          /* ㅅ */
        } else if (repeat_count == 1) {
            return tap_key(G);          /* ㅎ */
        } else {
            return tap_shifted_key(T);  /* ㅆ */
        }

    case CJI_J:
        if (repeat_count == 0) {
            return tap_key(W);          /* ㅈ */
        } else if (repeat_count == 1) {
            return tap_key(C);          /* ㅊ */
        } else {
            return tap_shifted_key(W);  /* ㅉ */
        }

    case CJI_O:
        if (repeat_count == 0) {
            return tap_key(D);          /* ㅇ */
        } else {
            return tap_key(A);          /* ㅁ */
        }

    default:
        return -ENOTSUP;
    }
}

static uint8_t max_repeat_count(uint32_t token) {
    switch (token) {
    case CJI_G:
    case CJI_D:
    case CJI_B:
    case CJI_S:
    case CJI_J:
        return 2;

    case CJI_N:
    case CJI_O:
        return 1;

    default:
        return 0;
    }
}

static bool is_consonant_token(uint32_t token) {
    return token == CJI_G || token == CJI_N || token == CJI_D || token == CJI_B ||
           token == CJI_S || token == CJI_J || token == CJI_O;
}

static int handle_consonant(uint32_t token) {
    int err;
    bool repeat = false;

    if (state.last_token == token &&
        state.last_kind == CJI_KIND_CONSONANT &&
        is_within_term()) {
        repeat = true;
    }

    if (repeat) {
        uint8_t max_count = max_repeat_count(token);

        if (state.repeat_count >= max_count) {
            state.repeat_count = 0;
        } else {
            state.repeat_count++;
        }

        err = tap_backspace();
        if (err < 0) {
            return err;
        }
    } else {
        state.repeat_count = 0;
    }

    err = emit_consonant(token, state.repeat_count);
    if (err < 0) {
        return err;
    }

    state.last_token = token;
    state.last_kind = CJI_KIND_CONSONANT;
    state.pending_dot = false;
    state.vowel_len = 0;
    state.emitted_vowel = false;
    state.emitted_vowel_out_len = 0;
    stamp_tap_time();

    return 0;
}

struct cji_output_key {
    uint32_t keycode;
    bool shifted;
};

struct cji_vowel_map {
    uint32_t seq[CJI_VOWEL_SEQ_MAX];
    uint8_t seq_len;
    struct cji_output_key out[CJI_VOWEL_OUT_MAX];
    uint8_t out_len;
};

/*
 * Cheonjiin vowel sequences used by this firmware:
 *
 * Basic:
 * ㅏ = ㅣ + ㆍ
 * ㅓ = ㆍ + ㅣ
 * ㅗ = ㅡ + ㆍ
 * ㅜ = ㆍ + ㅡ
 * ㅡ = ㅡ
 * ㅣ = ㅣ
 *
 * Extended:
 * ㅑ = ㅣ + ㆍ + ㆍ
 * ㅕ = ㆍ + ㆍ + ㅣ
 * ㅛ = ㅡ + ㆍ + ㆍ
 * ㅠ = ㆍ + ㆍ + ㅡ
 * ㅐ = ㅣ + ㆍ + ㅣ
 * ㅒ = ㅣ + ㆍ + ㆍ + ㅣ
 * ㅔ = ㆍ + ㅣ + ㅣ
 * ㅖ = ㆍ + ㆍ + ㅣ + ㅣ
 * ㅘ = ㅡ + ㆍ + ㅣ + ㆍ
 * ㅙ = ㅡ + ㆍ + ㅣ + ㆍ + ㅣ
 * ㅚ = ㅡ + ㆍ + ㅣ
 * ㅝ = ㆍ + ㅡ + ㆍ + ㅣ
 * ㅞ = ㆍ + ㅡ + ㆍ + ㅣ + ㅣ
 * ㅟ = ㆍ + ㅡ + ㅣ
 * ㅢ = ㅡ + ㅣ
 */
static const struct cji_vowel_map vowel_maps[] = {
    { .seq = {CJI_I},                                  .seq_len = 1, .out = {{L, false}},                 .out_len = 1 }, /* ㅣ */
    { .seq = {CJI_EU},                                 .seq_len = 1, .out = {{M, false}},                 .out_len = 1 }, /* ㅡ */
    { .seq = {CJI_I, CJI_DOT},                         .seq_len = 2, .out = {{K, false}},                 .out_len = 1 }, /* ㅏ */
    { .seq = {CJI_DOT, CJI_I},                         .seq_len = 2, .out = {{J, false}},                 .out_len = 1 }, /* ㅓ */
    { .seq = {CJI_EU, CJI_DOT},                        .seq_len = 2, .out = {{H, false}},                 .out_len = 1 }, /* ㅗ */
    { .seq = {CJI_DOT, CJI_EU},                        .seq_len = 2, .out = {{N, false}},                 .out_len = 1 }, /* ㅜ */

    { .seq = {CJI_I, CJI_DOT, CJI_DOT},                .seq_len = 3, .out = {{I, false}},                 .out_len = 1 }, /* ㅑ */
    { .seq = {CJI_DOT, CJI_DOT, CJI_I},                .seq_len = 3, .out = {{U, false}},                 .out_len = 1 }, /* ㅕ */
    { .seq = {CJI_EU, CJI_DOT, CJI_DOT},               .seq_len = 3, .out = {{Y, false}},                 .out_len = 1 }, /* ㅛ */
    { .seq = {CJI_DOT, CJI_DOT, CJI_EU},               .seq_len = 3, .out = {{B, false}},                 .out_len = 1 }, /* ㅠ */

    { .seq = {CJI_I, CJI_DOT, CJI_I},                  .seq_len = 3, .out = {{O, false}},                 .out_len = 1 }, /* ㅐ */
    { .seq = {CJI_I, CJI_DOT, CJI_DOT, CJI_I},         .seq_len = 4, .out = {{O, true}},                  .out_len = 1 }, /* ㅒ */
    { .seq = {CJI_DOT, CJI_I, CJI_I},                  .seq_len = 3, .out = {{P, false}},                 .out_len = 1 }, /* ㅔ */
    { .seq = {CJI_DOT, CJI_DOT, CJI_I, CJI_I},         .seq_len = 4, .out = {{P, true}},                  .out_len = 1 }, /* ㅖ */

    { .seq = {CJI_EU, CJI_DOT, CJI_I, CJI_DOT},        .seq_len = 4, .out = {{H, false}, {K, false}},     .out_len = 2 }, /* ㅘ */
    { .seq = {CJI_EU, CJI_DOT, CJI_I, CJI_DOT, CJI_I}, .seq_len = 5, .out = {{H, false}, {O, false}},     .out_len = 2 }, /* ㅙ */
    { .seq = {CJI_EU, CJI_DOT, CJI_I},                 .seq_len = 3, .out = {{H, false}, {L, false}},     .out_len = 2 }, /* ㅚ */

    { .seq = {CJI_DOT, CJI_EU, CJI_DOT, CJI_I},        .seq_len = 4, .out = {{N, false}, {J, false}},     .out_len = 2 }, /* ㅝ */
    { .seq = {CJI_DOT, CJI_EU, CJI_DOT, CJI_I, CJI_I}, .seq_len = 5, .out = {{N, false}, {P, false}},     .out_len = 2 }, /* ㅞ */
    { .seq = {CJI_DOT, CJI_EU, CJI_I},                 .seq_len = 3, .out = {{N, false}, {L, false}},     .out_len = 2 }, /* ㅟ */

    { .seq = {CJI_EU, CJI_I},                          .seq_len = 2, .out = {{M, false}, {L, false}},     .out_len = 2 }, /* ㅢ */
};

static bool vowel_seq_matches(const struct cji_vowel_map *map) {
    if (map->seq_len != state.vowel_len) {
        return false;
    }

    for (uint8_t i = 0; i < state.vowel_len; i++) {
        if (map->seq[i] != state.vowel_seq[i]) {
            return false;
        }
    }

    return true;
}

static bool vowel_seq_is_prefix(const struct cji_vowel_map *map) {
    if (state.vowel_len > map->seq_len) {
        return false;
    }

    for (uint8_t i = 0; i < state.vowel_len; i++) {
        if (map->seq[i] != state.vowel_seq[i]) {
            return false;
        }
    }

    return true;
}

static const struct cji_vowel_map *find_vowel_map(void) {
    for (uint8_t i = 0; i < ARRAY_SIZE(vowel_maps); i++) {
        if (vowel_seq_matches(&vowel_maps[i])) {
            return &vowel_maps[i];
        }
    }

    return NULL;
}

static bool has_vowel_prefix(void) {
    for (uint8_t i = 0; i < ARRAY_SIZE(vowel_maps); i++) {
        if (vowel_seq_is_prefix(&vowel_maps[i])) {
            return true;
        }
    }

    return false;
}

static int emit_vowel_map(const struct cji_vowel_map *map) {
    int err;

    for (uint8_t i = 0; i < map->out_len; i++) {
        if (map->out[i].shifted) {
            err = tap_shifted_key(map->out[i].keycode);
        } else {
            err = tap_key(map->out[i].keycode);
        }

        if (err < 0) {
            return err;
        }
    }

    return 0;
}

static int erase_emitted_vowel(void) {
    int err;
    uint8_t erase_count = state.emitted_vowel_out_len;

    if (erase_count == 0) {
        erase_count = 1;
    }

    for (uint8_t i = 0; i < erase_count; i++) {
        err = tap_backspace();
        if (err < 0) {
            return err;
        }
    }

    return 0;
}

static void reset_vowel_sequence(void) {
    state.vowel_len = 0;
    state.emitted_vowel = false;
    state.emitted_vowel_out_len = 0;
    state.pending_dot = false;
}

static void append_vowel_token(uint32_t token) {
    if (state.vowel_len < CJI_VOWEL_SEQ_MAX) {
        state.vowel_seq[state.vowel_len++] = token;
    }
}

static int start_new_vowel_sequence(uint32_t token) {
    const struct cji_vowel_map *map;
    int err;

    reset_vowel_sequence();
    append_vowel_token(token);

    map = find_vowel_map();
    if (map != NULL) {
        err = emit_vowel_map(map);
        if (err < 0) {
            return err;
        }

        state.emitted_vowel = true;
        state.emitted_vowel_out_len = map->out_len;
        state.last_kind = CJI_KIND_VOWEL;
    } else {
        state.emitted_vowel = false;
        state.emitted_vowel_out_len = 0;
        state.last_kind = CJI_KIND_DOT;
    }

    state.last_token = token;
    state.repeat_count = 0;
    state.pending_dot = (map == NULL);
    stamp_tap_time();

    return 0;
}

static int handle_vowel_token(uint32_t token) {
    int err;
    const struct cji_vowel_map *map;

    bool continuing =
        (state.last_kind == CJI_KIND_VOWEL || state.last_kind == CJI_KIND_DOT) &&
        is_within_term();

    if (!continuing) {
        return start_new_vowel_sequence(token);
    }

    append_vowel_token(token);

    map = find_vowel_map();

    if (map == NULL && !has_vowel_prefix()) {
        return start_new_vowel_sequence(token);
    }

    if (map != NULL) {
        if (state.emitted_vowel) {
            err = erase_emitted_vowel();
            if (err < 0) {
                return err;
            }
        }

        err = emit_vowel_map(map);
        if (err < 0) {
            return err;
        }

        state.emitted_vowel = true;
        state.emitted_vowel_out_len = map->out_len;
        state.last_kind = CJI_KIND_VOWEL;
    } else {
        state.emitted_vowel = false;
        state.emitted_vowel_out_len = 0;
        state.last_kind = CJI_KIND_DOT;
    }

    state.last_token = token;
    state.repeat_count = 0;
    state.pending_dot = (map == NULL);
    stamp_tap_time();

    return 0;
}

static int handle_vowel_i(void) {
    return handle_vowel_token(CJI_I);
}

static int handle_vowel_dot(void) {
    return handle_vowel_token(CJI_DOT);
}

static int handle_vowel_eu(void) {
    return handle_vowel_token(CJI_EU);
}

static int handle_token(uint32_t token) {
    switch (token) {
    case CJI_I:
        return handle_vowel_i();

    case CJI_DOT:
        return handle_vowel_dot();

    case CJI_EU:
        return handle_vowel_eu();

    case CJI_BSPC:
        reset_state();
        return tap_key(BACKSPACE);

    case CJI_SPACE:
        reset_state();
        return tap_key(SPACE);

    case CJI_ENTER:
        reset_state();
        return tap_key(ENTER);

    case CJI_RESET:
        reset_state();
        return 0;

    default:
        if (is_consonant_token(token)) {
            return handle_consonant(token);
        }

        return -ENOTSUP;
    }
}

static int on_cji_binding_pressed(struct zmk_behavior_binding *binding,
                                  struct zmk_behavior_binding_event event) {
    ARG_UNUSED(event);

    uint32_t token = binding->param1;
    return handle_token(token);
}

static int on_cji_binding_released(struct zmk_behavior_binding *binding,
                                   struct zmk_behavior_binding_event event) {
    ARG_UNUSED(binding);
    ARG_UNUSED(event);

    return 0;
}

static const struct behavior_driver_api behavior_cheonjiin_driver_api = {
    .binding_pressed = on_cji_binding_pressed,
    .binding_released = on_cji_binding_released,
};

static int behavior_cheonjiin_init(const struct device *dev) {
    ARG_UNUSED(dev);
    return 0;
}

#define CJI_INST(n)                                                                               \
    BEHAVIOR_DT_INST_DEFINE(n, behavior_cheonjiin_init, NULL, NULL, NULL, POST_KERNEL,             \
                            CONFIG_KERNEL_INIT_PRIORITY_DEFAULT, &behavior_cheonjiin_driver_api);

DT_INST_FOREACH_STATUS_OKAY(CJI_INST)

#endif

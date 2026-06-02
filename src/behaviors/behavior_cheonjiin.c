/*
 * Cheonjiin Korean input behavior for ZMK
 *
 * MVP behavior:
 * - Converts Cheonjiin key tokens into 2-beolsik Korean IME key strokes.
 * - Host OS must be in Korean 2-beolsik input mode.
 */

#define DT_DRV_COMPAT sinlain_behavior_cheonjiin

#include <zephyr/device.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include <drivers/behavior.h>

#include <zmk/behavior.h>
#include <zmk/endpoints.h>
#include <zmk/hid.h>

#include <dt-bindings/zmk/cheonjiin.h>
#include <dt-bindings/zmk/hid_usage_pages.h>
#include <dt-bindings/zmk/keys.h>

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#if DT_HAS_COMPAT_STATUS_OKAY(DT_DRV_COMPAT)

#define TAP_DELAY_MS 8

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
    bool pending_dot;
};

static struct cji_state state = {
    .last_token = 0,
    .repeat_count = 0,
    .last_kind = CJI_KIND_NONE,
    .pending_dot = false,
};

static int send_report(void) {
    return zmk_endpoint_send_report(HID_USAGE_KEY);
}

static int press_key(uint32_t keycode) {
    int err = zmk_hid_press(ZMK_HID_USAGE(HID_USAGE_KEY, keycode));
    if (err < 0) {
        return err;
    }

    return send_report();
}

static int release_key(uint32_t keycode) {
    int err = zmk_hid_release(ZMK_HID_USAGE(HID_USAGE_KEY, keycode));
    if (err < 0) {
        return err;
    }

    return send_report();
}

static int tap_key(uint32_t keycode) {
    int err = press_key(keycode);
    if (err < 0) {
        return err;
    }

    k_sleep(K_MSEC(TAP_DELAY_MS));

    err = release_key(keycode);
    if (err < 0) {
        return err;
    }

    k_sleep(K_MSEC(TAP_DELAY_MS));
    return 0;
}

static int tap_shifted_key(uint32_t keycode) {
    int err = press_key(LEFT_SHIFT);
    if (err < 0) {
        return err;
    }

    k_sleep(K_MSEC(TAP_DELAY_MS));

    err = press_key(keycode);
    if (err < 0) {
        release_key(LEFT_SHIFT);
        return err;
    }

    k_sleep(K_MSEC(TAP_DELAY_MS));

    err = release_key(keycode);
    if (err < 0) {
        release_key(LEFT_SHIFT);
        return err;
    }

    k_sleep(K_MSEC(TAP_DELAY_MS));

    err = release_key(LEFT_SHIFT);
    if (err < 0) {
        return err;
    }

    k_sleep(K_MSEC(TAP_DELAY_MS));
    return 0;
}

static int tap_backspace(void) {
    return tap_key(BACKSPACE);
}

static void reset_state(void) {
    state.last_token = 0;
    state.repeat_count = 0;
    state.last_kind = CJI_KIND_NONE;
    state.pending_dot = false;
}

/*
 * 2-beolsik Korean IME mapping:
 *
 * ㄱ r, ㄲ Shift+r, ㅋ z
 * ㄴ s, ㄹ f
 * ㄷ e, ㄸ Shift+e, ㅌ x
 * ㅂ q, ㅃ Shift+q, ㅍ v
 * ㅅ t, ㅆ Shift+t, ㅎ g
 * ㅈ w, ㅉ Shift+w, ㅊ c
 * ㅇ d, ㅁ a
 *
 * ㅏ k, ㅓ j, ㅗ h, ㅜ n, ㅡ m, ㅣ l
 */

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

static bool is_consonant_token(uint32_t token) {
    return token == CJI_G || token == CJI_N || token == CJI_D || token == CJI_B ||
           token == CJI_S || token == CJI_J || token == CJI_O;
}

static int handle_consonant(uint32_t token) {
    int err;

    if (state.last_token == token && state.last_kind == CJI_KIND_CONSONANT) {
        state.repeat_count++;

        if (state.repeat_count > 2) {
            state.repeat_count = 0;
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

    return 0;
}

static int handle_vowel_i(void) {
    int err;

    if (state.pending_dot && state.last_token == CJI_DOT) {
        /* ㆍ + ㅣ = ㅓ */
        err = tap_key(J);
        reset_state();
        return err;
    }

    err = tap_key(L); /* ㅣ */
    if (err < 0) {
        return err;
    }

    state.last_token = CJI_I;
    state.last_kind = CJI_KIND_VOWEL;
    state.pending_dot = false;

    return 0;
}

static int handle_vowel_eu(void) {
    int err;

    if (state.pending_dot && state.last_token == CJI_DOT) {
        /* ㆍ + ㅡ = ㅜ */
        err = tap_key(N);
        reset_state();
        return err;
    }

    err = tap_key(M); /* ㅡ */
    if (err < 0) {
        return err;
    }

    state.last_token = CJI_EU;
    state.last_kind = CJI_KIND_VOWEL;
    state.pending_dot = false;

    return 0;
}

static int handle_vowel_dot(void) {
    int err;

    if (state.last_token == CJI_I && state.last_kind == CJI_KIND_VOWEL) {
        /* ㅣ + ㆍ = ㅏ */
        err = tap_backspace();
        if (err < 0) {
            return err;
        }

        err = tap_key(K);
        reset_state();
        return err;
    }

    if (state.last_token == CJI_EU && state.last_kind == CJI_KIND_VOWEL) {
        /* ㅡ + ㆍ = ㅗ */
        err = tap_backspace();
        if (err < 0) {
            return err;
        }

        err = tap_key(H);
        reset_state();
        return err;
    }

    /*
     * Standalone ㆍ has no direct 2-beolsik key.
     * Keep it pending until the next vowel key.
     */
    state.last_token = CJI_DOT;
    state.last_kind = CJI_KIND_DOT;
    state.pending_dot = true;
    state.repeat_count = 0;

    return 0;
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

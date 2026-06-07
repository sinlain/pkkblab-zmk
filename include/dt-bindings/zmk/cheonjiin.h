#pragma once

/*
 * Cheonjiin input tokens
 *
 * Vowels:
 * 1 = ㅣ
 * 2 = ㆍ
 * 3 = ㅡ
 *
 * Consonants:
 * 10 = ㄱ/ㅋ/ㄲ
 * 11 = ㄴ/ㄹ
 * 12 = ㄷ/ㅌ/ㄸ
 * 13 = ㅂ/ㅍ/ㅃ
 * 14 = ㅅ/ㅎ/ㅆ
 * 15 = ㅈ/ㅊ/ㅉ
 * 16 = ㅇ/ㅁ
 */

#define CJI_I      1
#define CJI_DOT    2
#define CJI_EU     3

#define CJI_G      10
#define CJI_N      11
#define CJI_D      12
#define CJI_B      13
#define CJI_S      14
#define CJI_J      15
#define CJI_O      16

#define CJI_BSPC   30
#define CJI_SPACE  31
#define CJI_ENTER  32
#define CJI_RESET  33

/* English multi-tap tokens */
#define CJI_ENG_ABC   50  /* a b c */
#define CJI_ENG_DEF   51  /* d e f */
#define CJI_ENG_GHI   52  /* g h i */
#define CJI_ENG_JKL   53  /* j k l */
#define CJI_ENG_MNO   54  /* m n o */
#define CJI_ENG_PQRS  55  /* p q r s */
#define CJI_ENG_TUV   56  /* t u v */
#define CJI_ENG_WXYZ  57  /* w x y z */

/* Symbol / operator multi-tap tokens */
#define CJI_SYM_1     70  /* _ ^ # $ / */
#define CJI_SYM_2     71  /* * " ' ( ) */
#define CJI_SYM_3     72  /* , ? ! */
#define CJI_NUM_OPS   73  /* + - * % */

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
 * 4 = ㄱ/ㅋ/ㄲ
 * 5 = ㄴ/ㄹ
 * 6 = ㄷ/ㅌ/ㄸ
 * 7 = ㅂ/ㅍ/ㅃ
 * 8 = ㅅ/ㅎ/ㅆ
 * 9 = ㅈ/ㅊ/ㅉ
 * 0 = ㅇ/ㅁ
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

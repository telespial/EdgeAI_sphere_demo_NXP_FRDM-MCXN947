#include "text5x7.h"

#include <stddef.h>

#include "par_lcd_s035.h"

/* Each glyph is 7 rows, 5 bits wide, MSB-first in the low 5 bits (bit 4..0). */
static const uint8_t GLYPH_SPACE[7] = {0, 0, 0, 0, 0, 0, 0};
static const uint8_t GLYPH_COLON[7] = {0x00, 0x04, 0x04, 0x00, 0x04, 0x04, 0x00};

static const uint8_t GLYPH_0[7] = {0x0E, 0x11, 0x13, 0x15, 0x19, 0x11, 0x0E};
static const uint8_t GLYPH_1[7] = {0x04, 0x0C, 0x04, 0x04, 0x04, 0x04, 0x0E};
static const uint8_t GLYPH_2[7] = {0x0E, 0x11, 0x01, 0x02, 0x04, 0x08, 0x1F};
static const uint8_t GLYPH_3[7] = {0x1F, 0x02, 0x04, 0x02, 0x01, 0x11, 0x0E};
static const uint8_t GLYPH_4[7] = {0x02, 0x06, 0x0A, 0x12, 0x1F, 0x02, 0x02};
static const uint8_t GLYPH_5[7] = {0x1F, 0x10, 0x1E, 0x01, 0x01, 0x11, 0x0E};
static const uint8_t GLYPH_6[7] = {0x06, 0x08, 0x10, 0x1E, 0x11, 0x11, 0x0E};
static const uint8_t GLYPH_7[7] = {0x1F, 0x01, 0x02, 0x04, 0x08, 0x08, 0x08};
static const uint8_t GLYPH_8[7] = {0x0E, 0x11, 0x11, 0x0E, 0x11, 0x11, 0x0E};
static const uint8_t GLYPH_9[7] = {0x0E, 0x11, 0x11, 0x0F, 0x01, 0x02, 0x0C};

static const uint8_t GLYPH_A[7] = {0x0E, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11};
static const uint8_t GLYPH_B[7] = {0x1E, 0x11, 0x11, 0x1E, 0x11, 0x11, 0x1E};
static const uint8_t GLYPH_C[7] = {0x0E, 0x11, 0x10, 0x10, 0x10, 0x11, 0x0E};
static const uint8_t GLYPH_D[7] = {0x1E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x1E};
static const uint8_t GLYPH_E[7] = {0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x1F};
static const uint8_t GLYPH_H[7] = {0x11, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11};
static const uint8_t GLYPH_I[7] = {0x0E, 0x04, 0x04, 0x04, 0x04, 0x04, 0x0E};
static const uint8_t GLYPH_K[7] = {0x11, 0x12, 0x14, 0x18, 0x14, 0x12, 0x11};
static const uint8_t GLYPH_N[7] = {0x11, 0x19, 0x15, 0x13, 0x11, 0x11, 0x11};
static const uint8_t GLYPH_R[7] = {0x1E, 0x11, 0x11, 0x1E, 0x14, 0x12, 0x11};
static const uint8_t GLYPH_S[7] = {0x0F, 0x10, 0x10, 0x0E, 0x01, 0x01, 0x1E};
static const uint8_t GLYPH_U[7] = {0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E};
static const uint8_t GLYPH_LPAREN[7] = {0x02, 0x04, 0x08, 0x08, 0x08, 0x04, 0x02};
static const uint8_t GLYPH_RPAREN[7] = {0x08, 0x04, 0x02, 0x02, 0x02, 0x04, 0x08};

static const uint8_t *edgeai_glyph5x7(char c)
{
    switch (c)
    {
        case '0': return GLYPH_0;
        case '1': return GLYPH_1;
        case '2': return GLYPH_2;
        case '3': return GLYPH_3;
        case '4': return GLYPH_4;
        case '5': return GLYPH_5;
        case '6': return GLYPH_6;
        case '7': return GLYPH_7;
        case '8': return GLYPH_8;
        case '9': return GLYPH_9;

        case 'A': return GLYPH_A;
        case 'B': return GLYPH_B;
        case 'C': return GLYPH_C;
        case 'D': return GLYPH_D;
        case 'E': return GLYPH_E;
        case 'H': return GLYPH_H;
        case 'I': return GLYPH_I;
        case 'K': return GLYPH_K;
        case 'N': return GLYPH_N;
        case 'R': return GLYPH_R;
        case 'S': return GLYPH_S;
        case 'U': return GLYPH_U;
        case 'c': return GLYPH_C;

        case ':': return GLYPH_COLON;
        case '(': return GLYPH_LPAREN;
        case ')': return GLYPH_RPAREN;
        case ' ': return GLYPH_SPACE;
        default: return GLYPH_SPACE;
    }
}

static void edgeai_draw_char5x7_scaled(int32_t x, int32_t y, int32_t scale, char c, uint16_t color)
{
    const uint8_t *g = edgeai_glyph5x7(c);
    if (scale < 1) scale = 1;

    for (int32_t row = 0; row < 7; row++)
    {
        uint8_t bits = g[row];
        for (int32_t col = 0; col < 5; col++)
        {
            if (bits & (1u << (4 - col)))
            {
                int32_t x0 = x + col * scale;
                int32_t y0 = y + row * scale;
                par_lcd_s035_fill_rect(x0, y0, x0 + scale - 1, y0 + scale - 1, color);
            }
        }
    }
}

int32_t edgeai_text5x7_width(int32_t scale, const char *s)
{
    if (!s) return 0;
    int32_t n = 0;
    while (s[n]) n++;
    if (n == 0) return 0;
    return n * (5 + 1) * scale - 1 * scale;
}

void edgeai_text5x7_draw_scaled(int32_t x, int32_t y, int32_t scale, const char *s, uint16_t rgb565)
{
    if (!s) return;
    int32_t cx = x;
    while (*s)
    {
        edgeai_draw_char5x7_scaled(cx, y, scale, *s, rgb565);
        cx += (5 + 1) * scale;
        s++;
    }
}

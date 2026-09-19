/* =====================================================================
 *  FILE: src/assets.c
 *  PROJECT: NEON CITY ULTRA v4
 *  DESCRIPTION: 20 embedded 1024x1024 textures (linked from .S files)
 * ===================================================================== */

#include <ppu-types.h>

#define DECL_TEX(N) \
    extern const u8 _binary_texture##N##_png_start[]; \
    extern const u8 _binary_texture##N##_png_end[];

DECL_TEX(00) DECL_TEX(01) DECL_TEX(02) DECL_TEX(03) DECL_TEX(04)
DECL_TEX(05) DECL_TEX(06) DECL_TEX(07) DECL_TEX(08) DECL_TEX(09)
DECL_TEX(10) DECL_TEX(11) DECL_TEX(12) DECL_TEX(13) DECL_TEX(14)
DECL_TEX(15) DECL_TEX(16) DECL_TEX(17) DECL_TEX(18) DECL_TEX(19)

typedef struct {
    const u8 *data;
    const u8 *end;
    u32 size;
} EmbeddedTexture;

static EmbeddedTexture g_tex[20];

#define REG_TEX(I, N) do { \
    g_tex[I].data = _binary_texture##N##_png_start; \
    g_tex[I].end  = _binary_texture##N##_png_end; \
} while(0)

void assets_init(void) {
    REG_TEX(0,  00); REG_TEX(1,  01); REG_TEX(2,  02); REG_TEX(3,  03);
    REG_TEX(4,  04); REG_TEX(5,  05); REG_TEX(6,  06); REG_TEX(7,  07);
    REG_TEX(8,  08); REG_TEX(9,  09); REG_TEX(10, 10); REG_TEX(11, 11);
    REG_TEX(12, 12); REG_TEX(13, 13); REG_TEX(14, 14); REG_TEX(15, 15);
    REG_TEX(16, 16); REG_TEX(17, 17); REG_TEX(18, 18); REG_TEX(19, 19);

    for (int i = 0; i < 20; i++)
        g_tex[i].size = (u32)(g_tex[i].end - g_tex[i].data);
}

u32  assets_count(void)      { return 20; }
const u8* assets_data(int i) { return (i>=0 && i<20) ? g_tex[i].data : 0; }
u32  assets_size(int i)      { return (i>=0 && i<20) ? g_tex[i].size : 0; }
/* =====================================================================
 *  FILE: src/assets.c
 *  PROJECT: NEON CITY ULTRA v4
 *  DESCRIPTION: Declares external texture symbols embedded in the binary
 *    The textures are real 1024x1024 images used by the game
 * ===================================================================== */

#include <ppu-types.h>

/* ============ External texture symbols from objcopy ============ */
extern const u8 _binary_texture00_png_start[];
extern const u8 _binary_texture00_png_end[];
extern const u8 _binary_texture01_png_start[];
extern const u8 _binary_texture01_png_end[];
extern const u8 _binary_texture02_png_start[];
extern const u8 _binary_texture02_png_end[];
extern const u8 _binary_texture03_png_start[];
extern const u8 _binary_texture03_png_end[];
extern const u8 _binary_texture04_png_start[];
extern const u8 _binary_texture04_png_end[];
extern const u8 _binary_texture05_png_start[];
extern const u8 _binary_texture05_png_end[];
extern const u8 _binary_texture06_png_start[];
extern const u8 _binary_texture06_png_end[];
extern const u8 _binary_texture07_png_start[];
extern const u8 _binary_texture07_png_end[];
extern const u8 _binary_texture08_png_start[];
extern const u8 _binary_texture08_png_end[];
extern const u8 _binary_texture09_png_start[];
extern const u8 _binary_texture09_png_end[];
extern const u8 _binary_texture10_png_start[];
extern const u8 _binary_texture10_png_end[];
extern const u8 _binary_texture11_png_start[];
extern const u8 _binary_texture11_png_end[];
extern const u8 _binary_texture12_png_start[];
extern const u8 _binary_texture12_png_end[];
extern const u8 _binary_texture13_png_start[];
extern const u8 _binary_texture13_png_end[];
extern const u8 _binary_texture14_png_start[];
extern const u8 _binary_texture14_png_end[];
extern const u8 _binary_texture15_png_start[];
extern const u8 _binary_texture15_png_end[];
extern const u8 _binary_texture16_png_start[];
extern const u8 _binary_texture16_png_end[];
extern const u8 _binary_texture17_png_start[];
extern const u8 _binary_texture17_png_end[];
extern const u8 _binary_texture18_png_start[];
extern const u8 _binary_texture18_png_end[];
extern const u8 _binary_texture19_png_start[];
extern const u8 _binary_texture19_png_end[];

/* ============ Texture descriptor ============ */
typedef struct {
    const u8 *data;
    const u8 *end;
    u32 size;
} EmbeddedTexture;

static EmbeddedTexture g_textures[20];

void assets_init(void){
    g_textures[0].data  = _binary_texture00_png_start;
    g_textures[0].end   = _binary_texture00_png_end;
    g_textures[1].data  = _binary_texture01_png_start;
    g_textures[1].end   = _binary_texture01_png_end;
    g_textures[2].data  = _binary_texture02_png_start;
    g_textures[2].end   = _binary_texture02_png_end;
    g_textures[3].data  = _binary_texture03_png_start;
    g_textures[3].end   = _binary_texture03_png_end;
    g_textures[4].data  = _binary_texture04_png_start;
    g_textures[4].end   = _binary_texture04_png_end;
    g_textures[5].data  = _binary_texture05_png_start;
    g_textures[5].end   = _binary_texture05_png_end;
    g_textures[6].data  = _binary_texture06_png_start;
    g_textures[6].end   = _binary_texture06_png_end;
    g_textures[7].data  = _binary_texture07_png_start;
    g_textures[7].end   = _binary_texture07_png_end;
    g_textures[8].data  = _binary_texture08_png_start;
    g_textures[8].end   = _binary_texture08_png_end;
    g_textures[9].data  = _binary_texture09_png_start;
    g_textures[9].end   = _binary_texture09_png_end;
    g_textures[10].data = _binary_texture10_png_start;
    g_textures[10].end  = _binary_texture10_png_end;
    g_textures[11].data = _binary_texture11_png_start;
    g_textures[11].end  = _binary_texture11_png_end;
    g_textures[12].data = _binary_texture12_png_start;
    g_textures[12].end  = _binary_texture12_png_end;
    g_textures[13].data = _binary_texture13_png_start;
    g_textures[13].end  = _binary_texture13_png_end;
    g_textures[14].data = _binary_texture14_png_start;
    g_textures[14].end  = _binary_texture14_png_end;
    g_textures[15].data = _binary_texture15_png_start;
    g_textures[15].end  = _binary_texture15_png_end;
    g_textures[16].data = _binary_texture16_png_start;
    g_textures[16].end  = _binary_texture16_png_end;
    g_textures[17].data = _binary_texture17_png_start;
    g_textures[17].end  = _binary_texture17_png_end;
    g_textures[18].data = _binary_texture18_png_start;
    g_textures[18].end  = _binary_texture18_png_end;
    g_textures[19].data = _binary_texture19_png_start;
    g_textures[19].end  = _binary_texture19_png_end;

    for (int i = 0; i < 20; i++){
        g_textures[i].size = (u32)(g_textures[i].end - g_textures[i].data);
    }
}

u32 assets_count(void){ return 20; }

const u8* assets_data(int i){
    if (i < 0 || i >= 20) return 0;
    return g_textures[i].data;
}

u32 assets_size(int i){
    if (i < 0 || i >= 20) return 0;
    return g_textures[i].size;
}
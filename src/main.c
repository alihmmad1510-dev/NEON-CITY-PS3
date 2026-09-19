/* =====================================================================
 *  FILE: src/main.c
 *  PROJECT: NEON CITY ULTRA v3
 *  DESCRIPTION: PS3 Homebrew 3D 360° car driving game
 *  BUILD: make all   (PSL1GHT + tiny3d)
 * ===================================================================== */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <malloc.h>

#include <ppu-types.h>
#include <sys/process.h>
#include <sysutil/sysutil.h>
#include <sysmodule/sysmodule.h>
#include <io/pad.h>
#include <sys/time.h>

#include <tiny3d.h>
#include <libfont.h>

SYS_PROCESS_PARAM(1001, 0x100000)

/* Weak stubs للدوال الناقصة */
__attribute__((weak)) void tiny3d_TextureFormat(int f){ (void)f; }
__attribute__((weak)) void tiny3d_TextureEnable(void){ }
__attribute__((weak)) void tiny3d_TextureDisable(void){ }

#ifndef TINY3D_TEX_FORMAT_A8R8G8B8
#define TINY3D_TEX_FORMAT_A8R8G8B8  0x85
#endif

/* ===================== CONFIG ===================== */
#define SCR_W 1280
#define SCR_H 720
#define FOV   640.0f

#define MAX_OBJ  450
#define MAX_AI   25
#define MAX_PAR  150

#define BT_TOWER   0
#define BT_HOUSE   1
#define BT_VILLA   2
#define BT_SHOP    3
#define BT_MALL    4
#define BT_ARENA   5
#define BT_TREE    6
#define BT_GAS     7
#define BT_CAFE    8

/* ===================== TEXTURE TYPES ===================== */
typedef struct { u16 width; u16 height; u32 *bmp_out; } LocalPngData;
typedef struct { LocalPngData png; u16 w, h; int ok; u32 *pixels; } Tex;

static Tex tex_player;
static Tex tex_car_red, tex_car_blue, tex_car_yel, tex_car_grn, tex_car_prp;
static Tex tex_tower, tex_tower2, tex_house, tex_villa, tex_shop, tex_mall;
static Tex tex_arena, tex_gas, tex_cafe;
static Tex tex_tree, tex_tree2, tex_road;

/* ===================== STATE ===================== */
static int      g_running = 1;
static padInfo  g_padInfo;
static padData  g_padData;
static int      g_padReady = 0;
static u8       g_prevL3 = 0, g_prevR3 = 0;

static float p_x = 0, p_z = 0, p_heading = 0, p_speed = 0, p_boost = 1.0f;
static float cam_yaw = 0, cam_pitch = 0.35f, cam_dist = 15.0f;
static float g_timeOfDay = 0.20f;
static float g_shake = 0.0f;
static int   g_carTex = 0;
static int   g_score = 0;
static float g_msgTimer = 0.0f;
static char  g_msg[64] = "";

static float o_x[MAX_OBJ], o_z[MAX_OBJ], o_w[MAX_OBJ], o_h[MAX_OBJ], o_d[MAX_OBJ];
static int   o_type[MAX_OBJ];
static int   num_obj = 0;

static float a_x[MAX_AI], a_z[MAX_AI], a_spd[MAX_AI];
static int   a_ax[MAX_AI], a_dir[MAX_AI], a_tex[MAX_AI];

static float prt_x[MAX_PAR], prt_y[MAX_PAR], prt_z[MAX_PAR];
static float prt_vx[MAX_PAR], prt_vy[MAX_PAR], prt_vz[MAX_PAR];
static int   prt_life[MAX_PAR];
static u32   prt_col[MAX_PAR];
static int   num_prt = 0;

/* ===================== PAD ===================== */
#define BTN_SELECT   0x0001
#define BTN_L3       0x0002
#define BTN_R3       0x0004
#define BTN_START    0x0008
#define BTN_UP       0x0010
#define BTN_RIGHT    0x0020
#define BTN_DOWN     0x0040
#define BTN_LEFT     0x0080
#define BTN_L2       0x0100
#define BTN_R2       0x0200
#define BTN_L1       0x0400
#define BTN_R1       0x0800
#define BTN_TRIANGLE 0x1000
#define BTN_CIRCLE   0x2000
#define BTN_CROSS    0x4000
#define BTN_SQUARE   0x8000

static u16 pad_btns(void){ u8*p=(u8*)&g_padData; return (u16)(p[2]|(p[3]<<8)); }
static int pad_lx(void){ return ((s8*)&g_padData)[6]; }
static int pad_ly(void){ return ((s8*)&g_padData)[7]; }
static int pad_rx(void){ return ((s8*)&g_padData)[4]; }
static int pad_ry(void){ return ((s8*)&g_padData)[5]; }

/* ===================== RNG ===================== */
static unsigned int rng_s = 0xC0FFEE42u;
static unsigned int rng(void){
    rng_s ^= rng_s << 13; rng_s ^= rng_s >> 17; rng_s ^= rng_s << 5;
    return rng_s;
}
static float clampf(float v,float a,float b){ return v<a?a:(v>b?b:v); }

/* ===================== TEXTURE HELPERS ===================== */
static Tex tex_make(int w, int h){
    Tex t;
    t.w = w; t.h = h; t.ok = 0;
    t.pixels = (u32*)memalign(16, w*h*4);
    memset(t.pixels, 0, w*h*4);
    return t;
}
static inline void px_set(Tex *t, int x, int y, u32 c){
    if (x < 0 || y < 0 || x >= t->w || y >= t->h) return;
    t->pixels[y * t->w + x] = c;
}
static void px_rect(Tex *t, int x0, int y0, int x1, int y1, u32 c){
    for (int y = y0; y <= y1; y++)
        for (int x = x0; x <= x1; x++)
            px_set(t, x, y, c);
}
static void px_circle(Tex *t, int cx, int cy, int r, u32 c){
    for (int y = -r; y <= r; y++)
        for (int x = -r; x <= r; x++)
            if (x*x + y*y <= r*r) px_set(t, cx+x, cy+y, c);
}

/* ===================== SPRITE: PERSON ===================== */
static void draw_person(Tex *t, u32 shirt, u32 pants, u32 skin, u32 hair){
    int W=t->w, H=t->h, cx=W/2;
    int headCY=H/8, headR=W/6;

    px_circle(t, cx, headCY, headR, skin);
    for (int y=headCY-headR; y<headCY-headR/3; y++)
        for (int x=cx-headR; x<=cx+headR; x++)
            if ((x-cx)*(x-cx)+(y-headCY)*(y-headCY)<=headR*headR)
                px_set(t,x,y,hair);
    for (int y=headCY-headR+2; y<headCY-headR/2; y++)
        for (int x=cx-headR+3; x<=cx+headR-3; x++)
            if ((x-cx)*(x-cx)+(y-headCY)*(y-headCY)<=(headR-3)*(headR-3))
                if ((x+y)%7==0) px_set(t,x,y, ((hair&0xFEFEFE)>>1)|0xFF000000u);

    int eyeY = headCY + 1;
    px_rect(t, cx-headR/2-1, eyeY-1, cx-headR/2+2, eyeY+2, 0xFFFFFFFFu);
    px_rect(t, cx+headR/2-2, eyeY-1, cx+headR/2+1, eyeY+2, 0xFFFFFFFFu);
    px_rect(t, cx-headR/2,   eyeY,   cx-headR/2+1, eyeY+1, 0xFF000000u);
    px_rect(t, cx+headR/2-1, eyeY,   cx+headR/2,   eyeY+1, 0xFF000000u);
    px_rect(t, cx-2, headCY+headR/2, cx+2, headCY+headR/2, 0xFF8B3030u);
    px_rect(t, cx-3, headCY+headR-1, cx+3, headCY+headR+2, skin);

    int bTop=headCY+headR+2, bBot=H*3/5, bW=W/5;
    px_rect(t, cx-bW, bTop, cx+bW, bBot, shirt);
    px_rect(t, cx-bW+1, bTop, cx+bW-1, bTop+1, 0xFFEEEEEEu);
    for (int y=bTop+4; y<bBot; y+=6) px_set(t, cx, y, 0xFF222222u);
    px_rect(t, cx-bW, bTop+2, cx-bW+2, bBot, ((shirt&0xFEFEFE)>>1)|0xFF000000u);

    px_rect(t, cx-bW-3, bTop+2, cx-bW-1, bBot-4, shirt);
    px_rect(t, cx+bW+1, bTop+2, cx+bW+3, bBot-4, shirt);
    px_rect(t, cx-bW-3, bBot-4, cx-bW-1, bBot-1, skin);
    px_rect(t, cx+bW+1, bBot-4, cx+bW+3, bBot-1, skin);

    int lW=W/8;
    px_rect(t, cx-4-lW, bBot+1, cx-4, H-3, pants);
    px_rect(t, cx+4, bBot+1, cx+4+lW, H-3, pants);
    px_rect(t, cx+4, bBot+1, cx+4+2, H-3, ((pants&0xFEFEFE)>>1)|0xFF000000u);
    px_rect(t, cx-5-lW, H-4, cx-3, H-1, 0xFF1A1A1Au);
    px_rect(t, cx+3, H-4, cx+5+lW, H-1, 0xFF1A1A1Au);
    px_set(t, cx-4-lW, H-4, 0xFF444444u);
    px_set(t, cx+4, H-4, 0xFF444444u);
}

/* ===================== SPRITE: CAR ===================== */
static void draw_car(Tex *t, u32 body, u32 glass){
    int W=t->w, H=t->h;
    px_rect(t, 2, H/2, W-3, H*3/4, body);
    px_rect(t, W/5, H/3, W*4/5, H/2, body);
    px_rect(t, 2, H/2, W-3, H/2+3, ((body&0xFEFEFE)>>1)|0xFF000000u);
    px_rect(t, 2, H*3/4-3, W-3, H*3/4, ((body&0xFEFEFE)>>1)|0xFF000000u);

    px_rect(t, W/5+3, H/3+3, W/2-2, H/2-3, glass);
    px_rect(t, W/2+2, H/3+3, W*4/5-3, H/2-3, glass);
    px_rect(t, W/5+5, H/3+5, W/5+9, H/2-5, 0xFF7090B0u);
    px_rect(t, W/2+4, H/3+5, W/2+8, H/2-5, 0xFF7090B0u);
    px_rect(t, W/2-1, H/3+3, W/2+1, H/2-3, body);

    px_rect(t, 3, H/2-1, W-4, H/2, 0xFFFFFFFFu);
    px_rect(t, 3, H/2+3, W/7, H/2+8, 0xFFFFF8C0u);
    px_rect(t, 4, H/2+4, W/7-1, H/2+6, 0xFFFFFFFFu);
    px_rect(t, W-W/7, H/2+3, W-4, H/2+8, 0xFFFF3020u);
    px_rect(t, W-W/7, H/2+4, W-5, H/2+6, 0xFFFF8080u);
    px_rect(t, W/4, H/2+9, W*3/4, H/2+11, 0xFF101010u);
    for (int x=W/4+2; x<W*3/4; x+=4) px_set(t, x, H/2+10, 0xFF404040u);

    px_circle(t, W/5, H*3/4-1, 7, 0xFF000000u);
    px_circle(t, W/5, H*3/4-1, 5, 0xFF2A2A2Au);
    px_circle(t, W/5, H*3/4-1, 3, 0xFF808890u);
    px_circle(t, W/5, H*3/4-1, 1, 0xFFFFFFFFu);
    px_circle(t, W*4/5, H*3/4-1, 7, 0xFF000000u);
    px_circle(t, W*4/5, H*3/4-1, 5, 0xFF2A2A2Au);
    px_circle(t, W*4/5, H*3/4-1, 3, 0xFF808890u);
    px_circle(t, W*4/5, H*3/4-1, 1, 0xFFFFFFFFu);
    px_rect(t, 4, H*3/4+2, W-5, H*3/4+4, 0x80000000u);
}

/* ===================== SPRITE: TOWER ===================== */
static void draw_tower(Tex *t, u32 base, u32 win_on, u32 win_off){
    int W=t->w, H=t->h;
    px_rect(t, 0, 0, W-1, H-1, base);
    px_rect(t, 0, 0, W-1, 4, 0xFF1A1A22u);
    px_rect(t, 0, 0, 2, H-1, ((base&0xFEFEFE)>>1)|0xFF000000u);
    px_rect(t, W-3, 0, W-1, H-1, ((base&0xFEFEFE)>>1)|0xFF000000u);

    int cols = 5, rows = 16;
    int mx = 6, my = 6;
    int ww = (W - 2*mx) / cols;
    int wh = (H - 2*my) / rows;
    for (int r=0; r<rows; r++){
        for (int c=0; c<cols; c++){
            int x0 = mx + c*ww + 1;
            int y0 = my + r*wh + 1;
            int x1 = x0 + ww - 3;
            int y1 = y0 + wh - 3;
            u32 col = ((r*7 + c*3) % 5 == 0) ? win_on : win_off;
            px_rect(t, x0, y0, x1, y1, col);
            if (col == win_off) px_rect(t, x0, y0, x1, y0, 0xFF3A4050u);
        }
    }
    int gy = H - 20;
    px_rect(t, 0, gy, W-1, H-1, 0xFF303038u);
    for (int x=4; x<W-4; x+=8) px_rect(t, x, gy+3, x+5, H-4, 0xFF9EC9E8u);
}

/* ===================== SPRITE: HOUSE ===================== */
static void draw_house(Tex *t){
    int W=t->w, H=t->h;
    for (int y=0; y<H/4; y++){
        int w = ((H/4 - y) * W) / (H/4);
        int x0 = (W-w)/2;
        int x1 = x0 + w;
        px_rect(t, x0, y, x1, y, 0xFF6A2818u);
    }
    for (int y=H/6; y<H/4; y++){
        int w = ((H/4 - y) * W) / (H/4);
        int x0 = (W-w)/2;
        px_rect(t, x0, y, x0 + w/3, y, 0xFF8A4830u);
    }
    px_rect(t, 4, H/4, W-5, H-4, 0xFFE0C088u);
    int wy0 = H*2/5, wy1 = H*3/5;
    px_rect(t, W/6, wy0, W/3, wy1, 0xFF9EC9E8u);
    px_rect(t, W*2/3, wy0, W*5/6, wy1, 0xFF9EC9E8u);
    px_rect(t, W/6, (wy0+wy1)/2, W/3, (wy0+wy1)/2, 0xFF404040u);
    px_rect(t, W*2/3, (wy0+wy1)/2, W*5/6, (wy0+wy1)/2, 0xFF404040u);
    px_rect(t, (W/6 + W/3)/2, wy0, (W/6 + W/3)/2, wy1, 0xFF404040u);
    px_rect(t, (W*2/3 + W*5/6)/2, wy0, (W*2/3 + W*5/6)/2, wy1, 0xFF404040u);
    px_rect(t, W/2-7, H*3/4, W/2+7, H-4, 0xFF5A3018u);
    px_rect(t, W/2-5, H*3/4+2, W/2+5, H-6, 0xFF7A4830u);
    px_rect(t, W/2+3, H*7/8, W/2+5, H*7/8+1, 0xFFFFD700u);
    px_rect(t, 4, H/4, W-5, H/4+1, 0x60000000u);
    px_rect(t, W*3/4, H/8, W*3/4+6, H/4, 0xFF4A2010u);
}

/* ===================== SPRITE: VILLA ===================== */
static void draw_villa(Tex *t){
    int W=t->w, H=t->h;
    u32 wall = 0xFFEFE8D8;
    u32 roof = 0xFF303038;
    px_rect(t, 0, 0, W-1, 8, roof);
    px_rect(t, 0, 8, W-1, 10, 0xFF202028u);
    px_rect(t, 2, 10, W-3, H-6, wall);
    px_rect(t, 2, 10, 6, H-6, 0xFFB0A890u);
    px_rect(t, W-7, 10, W-3, H-6, 0xFFB0A890u);

    int wy0 = 22, wy1 = H - 30;
    for (int i=0; i<4; i++){
        int x0 = 12 + i * (W-24)/4;
        int x1 = x0 + (W-24)/4 - 6;
        px_rect(t, x0, wy0, x1, wy1, 0xFF9EC9E8u);
        for (int x=x0; x<=x1; x++){ px_set(t,x,wy0,0xFF404040u); px_set(t,x,wy1,0xFF404040u); }
        for (int y=wy0; y<=wy1; y++){ px_set(t,x0,y,0xFF404040u); px_set(t,x1,y,0xFF404040u); }
        px_rect(t, x0+2, wy0+2, x0+5, wy1-2, 0xFFD0E8F8u);
    }
    px_rect(t, W/2-14, H-38, W/2+14, H-6, 0xFF6A4A30u);
    px_rect(t, W/2-11, H-35, W/2-1, H-9, 0xFF9EC9E8u);
    px_rect(t, W/2+1, H-35, W/2+11, H-9, 0xFF9EC9E8u);
    px_rect(t, 0, H-6, W-1, H-1, 0xFF3A6A32u);
    for (int x=0; x<W; x+=6) px_set(t, x, H-6, 0xFF2A4A22u);
}

/* ===================== SPRITE: SHOP ===================== */
static void draw_shop(Tex *t, u32 wall, u32 sign){
    int W=t->w, H=t->h;
    px_rect(t, 0, 0, W-1, H-1, wall);
    px_rect(t, 0, 0, W-1, 10, 0xFF202020u);
    px_rect(t, 0, 10, W-1, 12, 0xFF404040u);
    px_rect(t, 6, 16, W-7, 32, sign);
    px_rect(t, 8, 18, W-9, 30, ((sign&0xFEFEFE)>>1)|0xFF000000u);
    for (int x=12; x<W-12; x+=6) px_rect(t, x, 22, x+3, 27, 0xFFFFFFFFu);
    px_rect(t, 6, 36, W-7, H-3, 0xFF9EC9E8u);
    for (int x=6; x<=W-7; x++){ px_set(t,x,36,0xFF404040u); px_set(t,x,H-3,0xFF404040u); }
    for (int y=36; y<=H-3; y++){ px_set(t,6,y,0xFF404040u); px_set(t,W-7,y,0xFF404040u); }
    px_rect(t, W/2-2, 36, W/2+2, H-3, 0xFF404040u);
    px_rect(t, 8, 38, 12, H-5, 0xFFD0E8F8u);
}

/* ===================== SPRITE: MALL ===================== */
static void draw_mall(Tex *t){
    int W=t->w, H=t->h;
    px_rect(t, 0, 0, W-1, H-1, 0xFF3A3A4Au);
    px_rect(t, 0, 0, W-1, 12, 0xFF202028u);
    px_rect(t, 0, 12, W-1, 14, 0xFF40404Au);
    px_rect(t, 20, 18, W-20, 36, 0xFFFF33AAu);
    px_rect(t, 22, 20, W-22, 34, 0xFFFF66CCu);
    for (int i=0; i<4; i++){
        int x = W/2 - 32 + i*16;
        px_rect(t, x, 22, x+10, 32, 0xFFFFFFFFu);
    }
    for (int r=0; r<3; r++){
        for (int c=0; c<6; c++){
            int x0 = 8 + c*(W-16)/6;
            int y0 = 44 + r*(H-90)/3;
            int x1 = x0 + (W-16)/6 - 5;
            int y1 = y0 + (H-90)/3 - 5;
            px_rect(t, x0, y0, x1, y1, 0xFFA0D0F0u);
            for (int x=x0; x<=x1; x++){ px_set(t,x,y0,0xFF303030u); px_set(t,x,y1,0xFF303030u); }
            for (int y=y0; y<=y1; y++){ px_set(t,x0,y,0xFF303030u); px_set(t,x1,y,0xFF303030u); }
            px_rect(t, x0+2, y0+2, x0+5, y1-2, 0xFFD0E8F8u);
        }
    }
    px_rect(t, W/2-20, H-40, W/2+20, H-3, 0xFF202020u);
    px_rect(t, W/2-17, H-37, W/2-1, H-6, 0xFF90C0E0u);
    px_rect(t, W/2+1, H-37, W/2+17, H-6, 0xFF90C0E0u);
    px_rect(t, W/2-1, H-40, W/2+1, H-3, 0xFF101010u);
}

/* ===================== SPRITE: ARENA ===================== */
static void draw_arena(Tex *t){
    int W=t->w, H=t->h;
    int cx=W/2, cy=H/2;
    px_rect(t, 0, 0, W-1, H-1, 0xFF2A3A4Au);
    for (int r=W/2; r>W/3; r-=2){
        u32 c = (r % 8 == 0) ? 0xFF605868u : 0xFF505060u;
        for (int y=-r; y<=r; y++){
            for (int x=-r; x<=r; x++){
                int d2 = x*x + y*y;
                if (d2 <= r*r && d2 > (r-2)*(r-2)) px_set(t, cx+x, cy+y, c);
            }
        }
    }
    for (int a=0; a<360; a+=6){
        for (int r=W/3+4; r<W/2-4; r+=8){
            int x = cx + (int)(cosf(a*3.14159f/180.0f) * r);
            int y = cy + (int)(sinf(a*3.14159f/180.0f) * r);
            if (x>=0 && x<W && y>=0 && y<H){
                u32 col;
                switch((a/6 + r/8) % 5){
                    case 0: col = 0xFFFF5555u; break;
                    case 1: col = 0xFFFFFF55u; break;
                    case 2: col = 0xFF55FF55u; break;
                    case 3: col = 0xFF55AAFFu; break;
                    default: col = 0xFFFF55FFu;
                }
                px_set(t, x, y, col);
            }
        }
    }
    for (int y=-W/3; y<=W/3; y++)
        for (int x=-W/3; x<=W/3; x++)
            if (x*x + y*y <= (W/3)*(W/3)) px_set(t, cx+x, cy+y, 0xFF2A8A30u);
    for (int y=-W/3; y<=W/3; y++)
        px_set(t, cx, cy+y, 0xFFFFFFFFu);
    for (int a=0; a<360; a+=5){
        int x = cx + (int)(cosf(a*3.14159f/180.0f) * 12);
        int y = cy + (int)(sinf(a*3.14159f/180.0f) * 12);
        px_set(t, x, y, 0xFFFFFFFFu);
    }
}

/* ===================== SPRITE: GAS ===================== */
static void draw_gas(Tex *t){
    int W=t->w, H=t->h;
    px_rect(t, 0, 0, W-1, H-1, 0xFF303038u);
    px_rect(t, 4, 4, W-5, 14, 0xFFFFCC00u);
    px_rect(t, 4, 14, W-5, 16, 0xFF222222u);
    px_rect(t, 6, 16, 10, H-20, 0xFF2A2A2Eu);
    px_rect(t, W-11, 16, W-7, H-20, 0xFF2A2A2Eu);
    for (int i=0; i<3; i++){
        int x = 20 + i*(W-50)/3;
        px_rect(t, x, 30, x+14, H-15, 0xFF333338u);
        px_rect(t, x+2, 33, x+12, 45, 0xFF90D090u);
        px_rect(t, x+5, 50, x+9, H-20, 0xFF222222u);
    }
    px_rect(t, W-40, H-40, W-6, H-6, 0xFF222225u);
    px_rect(t, W-38, H-38, W-8, H-8, 0xFFFFCC00u);
}

/* ===================== SPRITE: CAFE ===================== */
static void draw_cafe(Tex *t){
    int W=t->w, H=t->h;
    px_rect(t, 0, 0, W-1, H-1, 0xFF8B6035u);
    for (int i=0; i<W; i+=8){
        u32 col = ((i/8) % 2) ? 0xFFE03030u : 0xFFFFFFFFu;
        px_rect(t, i, 8, i+7, 24, col);
    }
    px_rect(t, 0, 4, W-1, 8, 0xFF4A2818u);
    px_rect(t, 0, 24, W-1, 26, 0x60000000u);
    px_rect(t, 6, 30, W-7, H-25, 0xFFE0F0FFu);
    for (int x=6; x<W-7; x+=16) px_rect(t, x, 30, x+1, H-25, 0xFF5A3820u);
    px_rect(t, 6, H-30, W-7, H-27, 0xFF4A2818u);
    px_rect(t, W/2-8, H-25, W/2+8, H-3, 0xFF4A2818u);
    px_rect(t, W/2-6, H-23, W/2+6, H-5, 0xFF6A4028u);
    px_rect(t, W/2-8, 0, W/2+8, 6, 0xFFFFFFFFu);
    px_rect(t, W/2-6, 2, W/2+6, 4, 0xFF8B6035u);
}

/* ===================== SPRITE: TREE ===================== */
static void draw_tree(Tex *t, int variant){
    int W=t->w, H=t->h, cx=W/2;
    px_rect(t, cx-4, H*2/3, cx+4, H-2, 0xFF4A2E1Au);
    px_rect(t, cx-4, H*2/3, cx-2, H-2, 0xFF3A2010u);
    px_rect(t, cx+2, H*2/3, cx+4, H-2, 0xFF5A3A20u);
    for (int y=H*2/3; y<H-2; y+=4) px_set(t, cx-1, y, 0xFF2A1008u);
    px_rect(t, cx-8, H*2/3, cx-4, H*2/3+3, 0xFF4A2E1Au);
    px_rect(t, cx+4, H*2/3, cx+8, H*2/3+3, 0xFF4A2E1Au);

    u32 dark=0xFF15481Cu, mid=0xFF2A7030u, light=0xFF4A9848u, bright=0xFF6AC868u;

    if (variant == 0){
        px_circle(t, cx, H/4, W/3, dark);
        px_circle(t, cx-6, H/3, W/4, mid);
        px_circle(t, cx+6, H/3, W/4, mid);
        px_circle(t, cx, H/5, W/4, light);
        px_circle(t, cx-4, H/3+2, W/5, light);
        px_circle(t, cx+4, H/3+2, W/5, light);
        px_circle(t, cx-2, H/5-2, W/6, bright);
        px_circle(t, cx+3, H/4+2, W/6, bright);
    } else {
        for (int i=0; i<5; i++){
            int y = 10 + i*18;
            int w = W/3 - i*3;
            if (w < 4) break;
            for (int dy=0; dy<20; dy++){
                int ww = w - (dy * w / 20);
                px_rect(t, cx-ww, y+dy, cx+ww, y+dy, (dy < 5) ? light : mid);
            }
        }
    }
}

/* ===================== SPRITE: ROAD ===================== */
static void draw_road(Tex *t){
    int W=t->w, H=t->h;
    px_rect(t, 0, 0, W-1, H-1, 0xFF383840u);
    for (int y=0; y<H; y+=8) px_rect(t, W/2-1, y, W/2+1, y+4, 0xFFFFCC00u);
    px_rect(t, 4, 0, 5, H-1, 0xFFFFFFFFu);
    px_rect(t, W-6, 0, W-5, H-1, 0xFFFFFFFFu);
    for (int i=0;i<20;i++){
        int x=rng()%W, y=rng()%H;
        int len = 3 + rng()%8;
        for (int j=0; j<len; j++) px_set(t, x+j, y+j/2, 0xFF202028u);
    }
    for (int i=0;i<60;i++){ px_set(t, rng()%W, rng()%H, 0xFF2A2A30u); }
}

/* ===================== TEXTURE UPLOAD ===================== */
static void tex_upload(Tex *t){
    t->png.width  = t->w;
    t->png.height = t->h;
    t->png.bmp_out = t->pixels;
    tiny3d_TextureOffset(&t->png);
    t->ok = 1;
}

static void textures_init(void){
    tex_player   = tex_make(64, 128);
    tex_car_red  = tex_make(128, 64);
    tex_car_blue = tex_make(128, 64);
    tex_car_yel  = tex_make(128, 64);
    tex_car_grn  = tex_make(128, 64);
    tex_car_prp  = tex_make(128, 64);
    tex_tower    = tex_make(128, 256);
    tex_tower2   = tex_make(128, 256);
    tex_house    = tex_make(96, 128);
    tex_villa    = tex_make(180, 100);
    tex_shop     = tex_make(96, 100);
    tex_mall     = tex_make(220, 140);
    tex_arena    = tex_make(280, 280);
    tex_gas      = tex_make(140, 120);
    tex_cafe     = tex_make(100, 110);
    tex_tree     = tex_make(72, 140);
    tex_tree2    = tex_make(72, 140);
    tex_road     = tex_make(64, 64);

    draw_person(&tex_player, 0xFF2A7FD0u, 0xFF23252Bu, 0xFFD8A87Cu, 0xFF2A1A0Au);
    draw_car(&tex_car_red,  0xFFC0392Bu, 0xFF304050u);
    draw_car(&tex_car_blue, 0xFF2874A6u, 0xFF304050u);
    draw_car(&tex_car_yel,  0xFFD4AC0Du, 0xFF304050u);
    draw_car(&tex_car_grn,  0xFF27AE60u, 0xFF304050u);
    draw_car(&tex_car_prp,  0xFF8E44ADu, 0xFF304050u);
    draw_tower(&tex_tower, 0xFF2A2A38u, 0xFFFFEE88u, 0xFF1A2030u);
    draw_tower(&tex_tower2, 0xFF35404Eu, 0xFFFFEE88u, 0xFF1A2030u);
    draw_house(&tex_house);
    draw_villa(&tex_villa);
    draw_shop(&tex_shop, 0xFFEFEFEFu, 0xFFFFE080u);
    draw_mall(&tex_mall);
    draw_arena(&tex_arena);
    draw_gas(&tex_gas);
    draw_cafe(&tex_cafe);
    draw_tree(&tex_tree, 0);
    draw_tree(&tex_tree2, 1);
    draw_road(&tex_road);

    tex_upload(&tex_player);
    tex_upload(&tex_car_red);
    tex_upload(&tex_car_blue);
    tex_upload(&tex_car_yel);
    tex_upload(&tex_car_grn);
    tex_upload(&tex_car_prp);
    tex_upload(&tex_tower);
    tex_upload(&tex_tower2);
    tex_upload(&tex_house);
    tex_upload(&tex_villa);
    tex_upload(&tex_shop);
    tex_upload(&tex_mall);
    tex_upload(&tex_arena);
    tex_upload(&tex_gas);
    tex_upload(&tex_cafe);
    tex_upload(&tex_tree);
    tex_upload(&tex_tree2);
    tex_upload(&tex_road);
}

/* ===================== 3D PROJECTION ===================== */
typedef struct { float x,y,z; int vis; } SP;

static void project(float wx,float wy,float wz,SP*out){
    float cx = p_x - sinf(cam_yaw)*cam_dist*cosf(cam_pitch);
    float cy = 4.0f + sinf(cam_pitch)*cam_dist;
    float cz = p_z - cosf(cam_yaw)*cam_dist*cosf(cam_pitch);
    cx += (rng()%100 - 50) * 0.01f * g_shake;
    cy += (rng()%100 - 50) * 0.01f * g_shake;

    float dx = wx-cx, dy = wy-cy, dz = wz-cz;
    float c1=cosf(-cam_yaw), s1=sinf(-cam_yaw);
    float rx = dx*c1 - dz*s1;
    float rz = dx*s1 + dz*c1;
    float cp=cosf(-cam_pitch), sp=sinf(-cam_pitch);
    float ry = dy*cp - rz*sp;
    float rz2= dy*sp + rz*cp;
    if (rz2 < 0.5f){ out->vis=0; out->x=0; out->y=0; out->z=0; return; }
    out->x = SCR_W*0.5f + (rx/rz2)*FOV;
    out->y = SCR_H*0.5f - (ry/rz2)*FOV;
    out->z = rz2;
    out->vis = 1;
}

/* ===================== DRAW PRIMITIVES ===================== */
static void draw_quad2d(float x1,float y1,float x2,float y2,
                        float x3,float y3,float x4,float y4,u32 col){
    tiny3d_SetPolygon(TINY3D_QUADS);
    tiny3d_VertexPos(x1,y1,0); tiny3d_VertexColor(col);
    tiny3d_VertexPos(x2,y2,0);
    tiny3d_VertexPos(x3,y3,0);
    tiny3d_VertexPos(x4,y4,0);
    tiny3d_End();
}
static void draw_rect2d(float x,float y,float w,float h,u32 col){
    draw_quad2d(x,y,x+w,y,x+w,y+h,x,y+h,col);
}

static void draw_tex_face(Tex *t,
    float x1,float y1,float z1,float u1,float v1,
    float x2,float y2,float z2,float u2,float v2,
    float x3,float y3,float z3,float u3,float v3,
    float x4,float y4,float z4,float u4,float v4, u32 col)
{
    SP pa,pb,pc,pd;
    project(x1,y1,z1,&pa); project(x2,y2,z2,&pb);
    project(x3,y3,z3,&pc); project(x4,y4,z4,&pd);
    if (!pa.vis||!pb.vis||!pc.vis||!pd.vis) return;
    if (t && t->ok){
        tiny3d_TextureOffset(&t->png);
        tiny3d_TextureFormat(TINY3D_TEX_FORMAT_A8R8G8B8);
        tiny3d_TextureEnable();
        tiny3d_SetPolygon(TINY3D_QUADS);
        tiny3d_VertexPos(pa.x,pa.y,pa.z); tiny3d_VertexColor(col); tiny3d_VertexTexture2(u1,v1);
        tiny3d_VertexPos(pb.x,pb.y,pb.z); tiny3d_VertexTexture2(u2,v2);
        tiny3d_VertexPos(pc.x,pc.y,pc.z); tiny3d_VertexTexture2(u3,v3);
        tiny3d_VertexPos(pd.x,pd.y,pd.z); tiny3d_VertexTexture2(u4,v4);
        tiny3d_End();
        tiny3d_TextureDisable();
    } else {
        draw_quad2d(pa.x,pa.y,pb.x,pb.y,pc.x,pc.y,pd.x,pd.y,col);
    }
}

/* ===================== GROUND PLATE + SHADOW ===================== */
static void draw_ground_plate(float cx, float cz, float w, float d, u32 col){
    float hw=w*0.5f, hd=d*0.5f;
    draw_tex_face(NULL,
        cx-hw, 0.005f, cz-hd,  0,0,
        cx+hw, 0.005f, cz-hd,  0,0,
        cx+hw, 0.005f, cz+hd,  0,0,
        cx-hw, 0.005f, cz+hd,  0,0, col);
}
static void draw_shadow(float cx, float cz, float w, float d){
    draw_ground_plate(cx, cz, w*1.25f, d*1.25f, 0x30000000u);
    draw_ground_plate(cx, cz, w*1.12f, d*1.12f, 0x50000000u);
    draw_ground_plate(cx, cz, w,      d,      0x80000000u);
}

/* ===================== BILLBOARD ===================== */
static void draw_billboard(Tex *t, float wx, float wy, float wz,
                            float w, float h, u32 col){
    float fx=wx-p_x, fz=wz-p_z;
    float len=sqrtf(fx*fx+fz*fz);
    if (len<0.001f){ fx=0; fz=-1; len=1; }
    fx/=len; fz/=len;
    float rx=-fz, rz=fx, hw=w*0.5f;
    float ax=wx-rx*hw, az=wz-rz*hw;
    float bx=wx+rx*hw, bz=wz+rz*hw;
    draw_tex_face(t,
        ax, wy,    az,  0.0f, 1.0f,
        bx, wy,    bz,  1.0f, 1.0f,
        bx, wy+h,  bz,  1.0f, 0.0f,
        ax, wy+h,  az,  0.0f, 0.0f, col);
}

/* ===================== 3D BOX ===================== */
static void draw_3d_box(float cx,float cy,float cz,float w,float h,float depth,
                        u32 col, Tex *tex){
    float hw=w*0.5f, hd=depth*0.5f;
    float x0=cx-hw, x1=cx+hw, z0=cz-hd, z1=cz+hd;
    float y0=cy, y1=cy+h;
    u32 top  = col;
    u32 side = ((col & 0xFEFEFE) >> 1) | 0xFF000000u;
    u32 dark = ((col & 0xFCFCFC) >> 2) | 0xFF000000u;
    SP pa,pb,pc,pd;

    project(x0,y1,z0,&pa); project(x1,y1,z0,&pb);
    project(x1,y1,z1,&pc); project(x0,y1,z1,&pd);
    if (pa.vis&&pb.vis&&pc.vis&&pd.vis)
        draw_quad2d(pa.x,pa.y,pb.x,pb.y,pc.x,pc.y,pd.x,pd.y,top);

    project(x0,y0,z1,&pa); project(x1,y0,z1,&pb);
    project(x1,y1,z1,&pc); project(x0,y1,z1,&pd);
    if (pa.vis&&pb.vis&&pc.vis&&pd.vis)
        draw_quad2d(pa.x,pa.y,pb.x,pb.y,pc.x,pc.y,pd.x,pd.y,side);

    project(x1,y0,z0,&pa); project(x0,y0,z0,&pb);
    project(x0,y1,z0,&pc); project(x1,y1,z0,&pd);
    if (pa.vis&&pb.vis&&pc.vis&&pd.vis)
        draw_quad2d(pa.x,pa.y,pb.x,pb.y,pc.x,pc.y,pd.x,pd.y,dark);

    project(x0,y0,z0,&pa); project(x0,y0,z1,&pb);
    project(x0,y1,z1,&pc); project(x0,y1,z0,&pd);
    if (pa.vis&&pb.vis&&pc.vis&&pd.vis)
        draw_quad2d(pa.x,pa.y,pb.x,pb.y,pc.x,pc.y,pd.x,pd.y,dark);

    if (tex && tex->ok){
        draw_tex_face(tex,
            x0, y0, z0,  0.0f, 1.0f,
            x1, y0, z0,  1.0f, 1.0f,
            x1, y1, z0,  1.0f, 0.0f,
            x0, y1, z0,  0.0f, 0.0f, 0xFFFFFFFFu);
    } else {
        project(x0,y0,z0,&pa); project(x1,y0,z0,&pb);
        project(x1,y1,z0,&pc); project(x0,y1,z0,&pd);
        if (pa.vis&&pb.vis&&pc.vis&&pd.vis)
            draw_quad2d(pa.x,pa.y,pb.x,pb.y,pc.x,pc.y,pd.x,pd.y,side);
    }
}

static void draw_ground_tex(float cx, float cz, float size, Tex *t){
    float h = size*0.5f;
    draw_tex_face(t,
        cx-h, 0, cz-h,  0.0f, 0.0f,
        cx+h, 0, cz-h,  1.0f, 0.0f,
        cx+h, 0, cz+h,  1.0f, 1.0f,
        cx-h, 0, cz+h,  0.0f, 1.0f, 0xFFFFFFFFu);
}

/* ===================== CITY GENERATION ===================== */
static void add_obj(int type, float x, float z, float w, float h, float d){
    if (num_obj >= MAX_OBJ) return;
    o_x[num_obj] = x; o_z[num_obj] = z;
    o_w[num_obj] = w; o_h[num_obj] = h; o_d[num_obj] = d;
    o_type[num_obj] = type;
    num_obj++;
}

static void gen_city(void){
    num_obj = 0;
    int bI = (int)(p_x/60.0f);
    int bJ = (int)(p_z/60.0f);

    for (int i=-4;i<=4;i++){
        for (int j=-4;j<=4;j++){
            int gi=bI+i, gj=bJ+j;
            float cx=(gi+0.5f)*60.0f, cz=(gj+0.5f)*60.0f;

            unsigned int h = ((unsigned int)(gi&0xFFFF)*73856093u) ^
                             ((unsigned int)(gj&0xFFFF)*19349663u);
            if (!h) h=1;

            if (gi==0 && gj==0){
                add_obj(BT_VILLA, cx, cz, 22.0f, 11.0f, 18.0f);
                add_obj(BT_TREE, cx-25, cz-18, 9, 14, 9);
                add_obj(BT_TREE, cx+25, cz-18, 9, 14, 9);
                add_obj(BT_TREE, cx-25, cz+18, 9, 14, 9);
                add_obj(BT_TREE, cx+25, cz+18, 9, 14, 9);
                continue;
            }
            if (gi==3 && gj==0){
                add_obj(BT_ARENA, cx, cz, 44, 16, 44);
                continue;
            }
            if (gi==0 && gj==3){
                add_obj(BT_MALL, cx, cz, 42, 24, 30);
                continue;
            }
            if (gi==-2 && gj==-2){
                add_obj(BT_GAS, cx, cz, 22, 7, 16);
                continue;
            }
            if (gi==-3 && gj==2){
                add_obj(BT_CAFE, cx, cz, 16, 10, 14);
                continue;
            }
            if ((gi*3 + gj*5) % 7 == 0){
                for (int k=0;k<5;k++){
                    float tx=cx+((k%2)-1)*15.0f;
                    float tz=cz+((k/2)-1)*15.0f;
                    add_obj(BT_TREE, tx, tz, 7, 13, 7);
                }
                continue;
            }

            int typ = h % 7;
            if (typ == 0 || typ == 1){
                add_obj(BT_TOWER, cx, cz, 16+(h%8), 32+(h%40), 16+(h%8));
            } else if (typ == 2 || typ == 3){
                for (int k=0;k<3;k++){
                    float sx = cx + (k-1)*15.0f;
                    add_obj(BT_SHOP, sx, cz, 11, 9, 13);
                }
            } else if (typ == 4){
                add_obj(BT_HOUSE, cx, cz, 15, 9, 15);
            } else if (typ == 5){
                add_obj(BT_CAFE, cx, cz, 14, 10, 12);
            } else {
                add_obj(BT_HOUSE, cx-8, cz-8, 13, 9, 13);
                add_obj(BT_HOUSE, cx+8, cz+8, 13, 9, 13);
            }
        }
    }
}

static Tex* obj_tex(int type){
    switch(type){
        case BT_TOWER: return &tex_tower;
        case BT_HOUSE: return &tex_house;
        case BT_VILLA: return &tex_villa;
        case BT_SHOP:  return &tex_shop;
        case BT_MALL:  return &tex_mall;
        case BT_ARENA: return &tex_arena;
        case BT_TREE:  return &tex_tree;
        case BT_GAS:   return &tex_gas;
        case BT_CAFE:  return &tex_cafe;
    }
    return &tex_tower;
}

/* ===================== AI CARS ===================== */
static void init_ai(void){
    for (int i=0;i<MAX_AI;i++){
        a_ax[i] = rng() & 1;
        a_dir[i] = (rng() & 1) ? 1 : -1;
        a_spd[i] = 18.0f + (rng() % 22);
        a_tex[i] = i % 5;
        if (a_ax[i]==0){
            a_x[i] = -280.0f + (rng()%560);
            a_z[i] = ((int)(rng()%10)-5)*60.0f + 15.0f*a_dir[i];
        } else {
            a_x[i] = ((int)(rng()%10)-5)*60.0f + 15.0f*a_dir[i];
            a_z[i] = -280.0f + (rng()%560);
        }
    }
}
static void update_ai(float dt){
    for (int i=0;i<MAX_AI;i++){
        if (a_ax[i]==0){
            a_x[i] += a_spd[i]*a_dir[i]*dt;
            if (a_x[i]>p_x+280 || a_x[i]<p_x-280){
                a_x[i] = p_x + (rng()%560) - 280;
                a_z[i] = p_z + ((int)(rng()%10)-5)*60.0f + 15.0f;
            }
        } else {
            a_z[i] += a_spd[i]*a_dir[i]*dt;
            if (a_z[i]>p_z+280 || a_z[i]<p_z-280){
                a_z[i] = p_z + (rng()%560) - 280;
                a_x[i] = p_x + ((int)(rng()%10)-5)*60.0f + 15.0f;
            }
        }
    }
}
static Tex* ai_tex(int i){
    switch(i){
        case 0: return &tex_car_red;
        case 1: return &tex_car_blue;
        case 2: return &tex_car_yel;
        case 3: return &tex_car_grn;
        default: return &tex_car_prp;
    }
}

/* ===================== PARTICLES ===================== */
static void spawn_particle(float x,float y,float z,float vx,float vy,float vz,u32 col){
    if (num_prt >= MAX_PAR) return;
    prt_x[num_prt]=x; prt_y[num_prt]=y; prt_z[num_prt]=z;
    prt_vx[num_prt]=vx; prt_vy[num_prt]=vy; prt_vz[num_prt]=vz;
    prt_life[num_prt]=40;
    prt_col[num_prt]=col;
    num_prt++;
}
static void update_particles(float dt){
    for (int i = num_prt-1; i >= 0; i--){
        prt_x[i]+=prt_vx[i]*dt; prt_y[i]+=prt_vy[i]*dt; prt_z[i]+=prt_vz[i]*dt;
        prt_vy[i] -= 8.0f*dt;
        prt_life[i]--;
        if (prt_life[i]<=0 || prt_y[i] < -2.0f){
            prt_x[i]=prt_x[num_prt-1]; prt_y[i]=prt_y[num_prt-1]; prt_z[i]=prt_z[num_prt-1];
            prt_vx[i]=prt_vx[num_prt-1]; prt_vy[i]=prt_vy[num_prt-1]; prt_vz[i]=prt_vz[num_prt-1];
            prt_life[i]=prt_life[num_prt-1]; prt_col[i]=prt_col[num_prt-1];
            num_prt--;
        }
    }
}

/* ===================== COLLISION ===================== */
static int hit_obj(float x,float z,float r, int* out_type){
    for (int i=0;i<num_obj;i++){
        if (o_type[i] == BT_TREE) continue;
        float hw = o_w[i]*0.5f + r;
        float hd = o_d[i]*0.5f + r;
        if (fabsf(x-o_x[i])<hw && fabsf(z-o_z[i])<hd){
            if (out_type) *out_type = o_type[i];
            return 1;
        }
    }
    return 0;
}

/* ===================== UPDATE ===================== */
static void show_msg(const char* m){
    strncpy(g_msg, m, 63); g_msg[63]=0;
    g_msgTimer = 2.0f;
}

static void update(float dt){
    if (!g_padReady) return;
    u16 btn = pad_btns();
    int lx = pad_lx(), ly = pad_ly();
    int rx = pad_rx(), ry = pad_ry();

    cam_yaw += (rx/128.0f)*2.6f*dt;
    cam_pitch = clampf(cam_pitch + (ry/128.0f)*1.5f*dt, -0.1f, 1.3f);
    if (btn & BTN_R2) cam_dist += 15.0f*dt;
    if (btn & BTN_L2) cam_dist -= 15.0f*dt;
    cam_dist = clampf(cam_dist, 6.0f, 35.0f);

    u8 l3 = (btn & BTN_L3) ? 1 : 0;
    if (l3 && !g_prevL3){
        g_carTex = (g_carTex + 1) % 5;
        const char *names[] = {"RED","BLUE","YELLOW","GREEN","PURPLE"};
        char buf[64];
        sprintf(buf, "Car color: %s", names[g_carTex]);
        show_msg(buf);
    }
    g_prevL3 = l3;

    u8 r3 = (btn & BTN_R3) ? 1 : 0;
    if (r3 && !g_prevR3){
        p_x = 0; p_z = 0; p_speed = 0; p_heading = 0;
        show_msg("Teleport to Villa");
    }
    g_prevR3 = r3;

    float thr = -(ly/128.0f);
    float st  = -(lx/128.0f);
    if (btn & BTN_UP)    thr = 1.0f;
    if (btn & BTN_DOWN)  thr = -1.0f;
    if (btn & BTN_LEFT)  st = 1.0f;
    if (btn & BTN_RIGHT) st = -1.0f;

    if ((btn & BTN_R1) && p_boost > 0.0f){
        p_speed += 45.0f*dt;
        p_boost -= 0.45f*dt;
        float bx = p_x - sinf(p_heading)*3.0f;
        float bz = p_z - cosf(p_heading)*3.0f;
        for (int k=0;k<2;k++){
            spawn_particle(
                bx+(rng()%100-50)*0.05f,
                0.5f+(rng()%100)*0.02f,
                bz+(rng()%100-50)*0.05f,
                (rng()%100-50)*0.03f - sinf(p_heading)*3.0f,
                (rng()%100)*0.02f,
                (rng()%100-50)*0.03f - cosf(p_heading)*3.0f,
                (k%2) ? 0xFFFF8800u : 0xFFFFDD00u);
        }
    } else {
        p_boost = clampf(p_boost + 0.15f*dt, 0.0f, 1.0f);
    }

    if (btn & BTN_L1){
        p_speed *= (1.0f - 3.5f*dt);
        g_shake = 0.5f;
    }

    p_speed += thr*32.0f*dt;
    if (fabsf(thr) < 0.1f) p_speed *= (1.0f - 1.3f*dt);
    p_speed = clampf(p_speed, -20.0f, 65.0f);

    if (fabsf(p_speed) > 0.5f){
        float sgn = p_speed > 0 ? 1.0f : -1.0f;
        float gain = clampf(fabsf(p_speed)/15.0f, 0.3f, 1.0f);
        p_heading += st * 2.2f * gain * dt * sgn;
    }

    float vx = sinf(p_heading)*p_speed*dt;
    float vz = cosf(p_heading)*p_speed*dt;
    float nx = p_x + vx, nz = p_z + vz;
    int   t_type = -1;

    if (!hit_obj(nx, p_z, 1.8f, &t_type)) p_x = nx;
    else {
        p_speed *= 0.15f; g_shake = 0.8f;
        if (t_type == BT_SHOP || t_type == BT_MALL){
            g_score += 10; show_msg("Shopped! +10");
        } else if (t_type == BT_GAS){
            p_boost = 1.0f; show_msg("Refueled!");
        } else if (t_type == BT_CAFE){
            g_score += 5; show_msg("Coffee! +5");
        }
    }
    if (!hit_obj(p_x, nz, 1.8f, &t_type)) p_z = nz;
    else {
        p_speed *= 0.15f; g_shake = 0.8f;
        if (t_type == BT_SHOP || t_type == BT_MALL){
            g_score += 10; show_msg("Shopped! +10");
        } else if (t_type == BT_GAS){
            p_boost = 1.0f; show_msg("Refueled!");
        } else if (t_type == BT_CAFE){
            g_score += 5; show_msg("Coffee! +5");
        }
    }
    if (fabsf(p_speed) > 40.0f) g_shake = 0.3f;

    g_shake *= (1.0f - 5.0f*dt);
    if (g_shake < 0.01f) g_shake = 0.0f;

    g_timeOfDay += dt/240.0f;
    if (g_timeOfDay > 1.0f) g_timeOfDay -= 1.0f;

    if (g_msgTimer > 0.0f){
        g_msgTimer -= dt;
        if (g_msgTimer < 0.0f) g_msgTimer = 0.0f;
    }

    update_ai(dt);
    update_particles(dt);
}

/* ===================== RENDER ===================== */
static u32 sky_col_top(void){
    float sY = sinf((g_timeOfDay - 0.25f) * 6.2831853f);
    float d = sY > 0 ? sY : 0;
    u8 r = (u8)(8 + d*40);
    u8 g = (u8)(15 + d*100);
    u8 b = (u8)(35 + d*170);
    return 0xFF000000u | ((u32)r<<16) | ((u32)g<<8) | b;
}
static u32 sky_col_mid(void){
    float sY = sinf((g_timeOfDay - 0.25f) * 6.2831853f);
    float d = sY > 0 ? sY : 0;
    u8 r = (u8)(30 + d*80);
    u8 g = (u8)(50 + d*130);
    u8 b = (u8)(80 + d*160);
    return 0xFF000000u | ((u32)r<<16) | ((u32)g<<8) | b;
}

static void render(void){
    u32 skyTop = sky_col_top();
    u32 skyMid = sky_col_mid();
    float sY = sinf((g_timeOfDay - 0.25f) * 6.2831853f);
    u32 horizon = (sY > 0) ? 0xFFFFBB88u : 0xFF2A2A4Au;

    tiny3d_Clear(skyTop, 0xFFFFFFFF);
    draw_rect2d(0, 0, SCR_W, SCR_H*0.30f, skyTop);
    draw_rect2d(0, SCR_H*0.30f, SCR_W, SCR_H*0.20f, skyMid);
    draw_rect2d(0, SCR_H*0.50f, SCR_W, SCR_H*0.08f, horizon);
    draw_rect2d(0, SCR_H*0.58f, SCR_W, 4, 0xFFFFCC88u);

    /* sun / moon */
    if (sY > 0){
        int sx = (int)(SCR_W * 0.65f);
        int sy = (int)(SCR_H * 0.30f - sY * SCR_H * 0.20f);
        for (int y=-40; y<=40; y++)
            for (int x=-40; x<=40; x++)
                if (x*x + y*y <= 40*40){
                    int px = sx + x, py = sy + y;
                    if (px >= 0 && px < SCR_W && py >= 0 && py < SCR_H)
                        draw_rect2d(px, py, 2, 2, 0xFFFFEEB0u);
                }
    } else {
        int sx = (int)(SCR_W * 0.65f);
        int sy = (int)(SCR_H * 0.30f + sY * SCR_H * 0.30f);
        for (int y=-30; y<=30; y++)
            for (int x=-30; x<=30; x++)
                if (x*x + y*y <= 30*30){
                    int px = sx + x, py = sy + y;
                    if (px >= 0 && px < SCR_W && py >= 0 && py < SCR_H)
                        draw_rect2d(px, py, 2, 2, 0xFFEEEEFFu);
                }
    }

    static float last_x = 1e9f, last_z = 1e9f;
    if (fabsf(p_x-last_x) > 20.0f || fabsf(p_z-last_z) > 20.0f){
        gen_city();
        last_x = p_x; last_z = p_z;
    }

    /* ground */
    int bI = (int)(p_x/60.0f), bJ = (int)(p_z/60.0f);
    for (int i=-4;i<=4;i++)
        for (int j=-4;j<=4;j++)
            draw_ground_tex((bI+i)*60.0f, (bJ+j)*60.0f, 60.0f, &tex_road);

    /* sort objects by distance (painter's algorithm) */
    int ord[MAX_OBJ];
    float dist[MAX_OBJ];
    for (int i=0;i<num_obj;i++){
        ord[i]=i;
        float dx=o_x[i]-p_x, dz=o_z[i]-p_z;
        dist[i]=dx*dx+dz*dz;
    }
    for (int i=0;i<num_obj-1;i++)
        for (int j=0;j<num_obj-1-i;j++)
            if (dist[ord[j]] < dist[ord[j+1]]){
                int t=ord[j]; ord[j]=ord[j+1]; ord[j+1]=t;
            }

    for (int k=0;k<num_obj;k++){
        int i = ord[k];
        Tex *tt = obj_tex(o_type[i]);
        if (o_type[i] == BT_TREE){
            draw_shadow(o_x[i], o_z[i], o_w[i]*0.5f, o_d[i]*0.5f);
            /* ثابت على الأرض - cy = 0.02f */
            draw_billboard(tt, o_x[i], 0.02f, o_z[i], o_w[i], o_h[i], 0xFFFFFFFFu);
        } else {
            draw_shadow(o_x[i], o_z[i], o_w[i]*1.15f, o_d[i]*1.15f);
            u32 bc = 0xFFFFFFFFu;
            if (o_type[i] == BT_TOWER) bc = 0xFF404050u;
            else if (o_type[i] == BT_SHOP) bc = 0xFFEFEFEFu;
            else if (o_type[i] == BT_MALL) bc = 0xFF3A3A4Au;
            else if (o_type[i] == BT_ARENA) bc = 0xFF2A3A4Au;
            else if (o_type[i] == BT_GAS) bc = 0xFF303038u;
            else if (o_type[i] == BT_CAFE) bc = 0xFF8B6035u;
            /* cy = 0.01f يخلي المبنى ملزوق بالأرض */
            draw_3d_box(o_x[i], 0.01f, o_z[i], o_w[i], o_h[i], o_d[i], bc, tt);
        }
    }

    /* AI cars */
    for (int i=0;i<MAX_AI;i++){
        Tex *tt = ai_tex(a_tex[i]);
        draw_shadow(a_x[i], a_z[i], 4.0f, 5.5f);
        draw_billboard(tt, a_x[i], 0.15f, a_z[i], 5.5f, 2.8f, 0xFFFFFFFFu);
    }

    /* player car */
    Tex *pcar = &tex_car_red;
    switch(g_carTex){
        case 1: pcar = &tex_car_blue; break;
        case 2: pcar = &tex_car_yel;  break;
        case 3: pcar = &tex_car_grn;  break;
        case 4: pcar = &tex_car_prp;  break;
    }
    draw_shadow(p_x, p_z, 5.0f, 6.5f);
    draw_billboard(pcar, p_x, 0.15f, p_z, 6.0f, 3.0f, 0xFFFFFFFFu);

    /* particles */
    for (int i=0;i<num_prt;i++)
        draw_billboard(&tex_road, prt_x[i], prt_y[i], prt_z[i], 0.5f, 0.5f, prt_col[i]);

    /* ===================== HUD ===================== */
    SetFontSize(34, 34);
    SetFontColor(0xFF00D4FFu, 0x00000000);
    DrawString(24, 48, "NEON CITY");

    SetFontSize(13, 13);
    SetFontColor(0xFFFF6B35u, 0x00000000);
    DrawString(26, 74, "U L T R A   v 3");

    char buf[128];
    sprintf(buf, "%d KM/H", (int)(fabsf(p_speed)*5.4f));
    SetFontSize(32, 32);
    SetFontColor(0xFFFFCC00u, 0x00000000);
    DrawString(24, SCR_H - 105, buf);

    SetFontSize(13, 13);
    SetFontColor(0xFFFF6B35u, 0x00000000);
    DrawString(24, SCR_H - 72, "BOOST");
    draw_rect2d(100, SCR_H - 75, 220, 16, 0xFF222222u);
    draw_rect2d(100, SCR_H - 75, p_boost*220.0f, 16, 0xFFFF6B35u);
    draw_rect2d(100, SCR_H - 75, 220, 1, 0xFFFFFFFFu);
    draw_rect2d(100, SCR_H - 60, 220, 1, 0xFFFFFFFFu);

    SetFontSize(17, 17);
    SetFontColor(0xFF2ECC71u, 0x00000000);
    sprintf(buf, "SCORE: %d", g_score);
    DrawString(24, SCR_H - 135, buf);

    if (g_msgTimer > 0.0f){
        SetFontSize(22, 22);
        SetFontColor(0xFFFFFFFFu, 0xA0000000u);
        int w = (int)strlen(g_msg)*11;
        DrawString(SCR_W/2 - w/2, 140, g_msg);
    }

    /* minimap */
    {
        int mx = SCR_W - 210, my = 20, ms = 190;
        draw_rect2d(mx-3, my-3, ms+6, ms+6, 0xFFFFCC00u);
        draw_rect2d(mx, my, ms, ms, 0xCC000818u);
        draw_rect2d(mx-1, my-1, ms+2, ms+2, 0xFF000000u);
        float sc = (float)ms / 320.0f;
        float cxm = mx + ms*0.5f, cym = my + ms*0.5f;
        for (int i=0;i<num_obj;i++){
            float dx=(o_x[i]-p_x)*sc, dz=(o_z[i]-p_z)*sc;
            if (fabsf(dx)>ms*0.5f || fabsf(dz)>ms*0.5f) continue;
            u32 col = 0xFF5A6878u;
            if (o_type[i]==BT_VILLA) col=0xFF3CDC78u;
            else if (o_type[i]==BT_ARENA) col=0xFFFF8844u;
            else if (o_type[i]==BT_MALL) col=0xFFFF33AAu;
            else if (o_type[i]==BT_SHOP) col=0xFFFFFF44u;
            else if (o_type[i]==BT_CAFE) col=0xFF8B6035u;
            else if (o_type[i]==BT_TREE) col=0xFF2E7A30u;
            else if (o_type[i]==BT_GAS) col=0xFFFFCC00u;
            float hs = o_w[i]*0.5f*sc;
            draw_rect2d(cxm+dx-hs, cym+dz-hs, hs*2, hs*2, col);
        }
        for (int i=0;i<MAX_AI;i++){
            float dx=(a_x[i]-p_x)*sc, dz=(a_z[i]-p_z)*sc;
            if (fabsf(dx)>ms*0.5f || fabsf(dz)>ms*0.5f) continue;
            draw_rect2d(cxm+dx-2, cym+dz-2, 4, 4, 0xFFDC7864u);
        }
        draw_rect2d(cxm-5, cym-5, 10, 10, 0xFFFFCC00u);
        draw_rect2d(cxm-3, cym-3, 6, 6, 0xFFFFFFFFu);
        SetFontSize(12, 12);
        SetFontColor(0xFFFF5555u, 0x00000000);
        DrawString(mx+ms-14, my+15, "N");
    }

    /* clock */
    int hours = (int)((g_timeOfDay*24.0f) + 6.0f) % 24;
    int minutes = (int)((((g_timeOfDay*24.0f) + 6.0f) - (float)hours) * 60.0f);
    sprintf(buf, "%02d:%02d", hours, minutes);
    SetFontSize(22, 22);
    SetFontColor(0xFFFFE080u, 0x00000000);
    DrawString(SCR_W - 140, SCR_H - 45, buf);

    SetFontSize(11, 11);
    SetFontColor(0xFFBBBBBBu, 0x00000000);
    DrawString(24, SCR_H - 22,
        "L:drive  R:cam  R1:nitro  L1:brake  L3:color  R3:reset  SEL:quit");
}

/* ===================== MAIN ===================== */
static void sysutil_cb(u64 s, u64 p, void *u){
    (void)p; (void)u;
    if (s == SYSUTIL_EXIT_GAME) g_running = 0;
}

int main(int argc, char *argv[]){
    (void)argc; (void)argv;
    sysModuleLoad(SYSMODULE_FS);
    ioPadInit(7);
    sysUtilRegisterCallback(SYSUTIL_EVENT_SLOT0, sysutil_cb, NULL);
    if (tiny3d_Init(1024*1024) != 0) return 1;
    ResetFont();
    textures_init();
    p_x = 0; p_z = 0; p_heading = 0;
    gen_city();
    init_ai();
    struct timeval tv; gettimeofday(&tv, NULL);
    rng_s = (unsigned int)tv.tv_usec;
    show_msg("Welcome to Neon City!");

    while (g_running){
        sysUtilCheckCallback();
        g_padReady = 0;
        if (ioPadGetInfo(&g_padInfo) == 0 && g_padInfo.status[0]){
            if (ioPadGetData(0, &g_padData) == 0) g_padReady = 1;
        }
        if (g_padReady && (pad_btns() & BTN_SELECT)) g_running = 0;
        update(1.0f/60.0f);
        render();
        tiny3d_Flip();
    }
    ioPadEnd();
    tiny3d_Exit();
    return 0;
}
/* =====================================================================
 *  FILE: src/main.c
 *  PROJECT: NEON CITY ULTRA v4 — Unity-Style Edition
 *  DESCRIPTION: PS3 Homebrew 3D 360° car driving game
 *    - Loading screen with progress bar
 *    - Main menu with animated background
 *    - Bloom/glow effects
 *    - Animated clouds + weather (rain)
 *    - Advanced particle system (exhaust, dust, sparks)
 *    - Clean HUD
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

__attribute__((weak)) void tiny3d_TextureFormat(int f){ (void)f; }
__attribute__((weak)) void tiny3d_TextureEnable(void){ }
__attribute__((weak)) void tiny3d_TextureDisable(void){ }
#ifndef TINY3D_TEX_FORMAT_A8R8G8B8
#define TINY3D_TEX_FORMAT_A8R8G8B8  0x85
#endif

#define SCR_W 1280
#define SCR_H 720
#define FOV   640.0f

#define MAX_OBJ  450
#define MAX_AI   25
#define MAX_PAR  200
#define MAX_CLD  12

#define BT_TOWER   0
#define BT_HOUSE   1
#define BT_VILLA   2
#define BT_SHOP    3
#define BT_MALL    4
#define BT_ARENA   5
#define BT_TREE    6
#define BT_GAS     7
#define BT_CAFE    8

typedef struct { u16 width; u16 height; u32 *bmp_out; } LocalPngData;
typedef struct { LocalPngData png; u16 w, h; int ok; u32 *pixels; } Tex;

static Tex tex_player;
static Tex tex_car_red, tex_car_blue, tex_car_yel, tex_car_grn, tex_car_prp;
static Tex tex_tower, tex_tower2, tex_house, tex_villa, tex_shop, tex_mall;
static Tex tex_arena, tex_gas, tex_cafe;
static Tex tex_tree, tex_tree2, tex_road;

/* Game state */
typedef enum { ST_LOADING=0, ST_MENU, ST_PLAYING, ST_PAUSED } GameState;
static GameState g_state = ST_LOADING;
static float g_loadProgress = 0.0f;
static int   g_menuSel = 0;
static float g_menuTimer = 0.0f;

static int      g_running = 1;
static padInfo  g_padInfo;
static padData  g_padData;
static int      g_padReady = 0;
static u8       g_prevL3 = 0, g_prevR3 = 0;
static u8       g_prevCross = 0, g_prevStart = 0, g_prevUp = 0, g_prevDown = 0;
static u8       g_prevSelect = 0;

static float p_x = 0, p_z = 0, p_heading = 0, p_speed = 0, p_boost = 1.0f;
static float cam_yaw = 0, cam_pitch = 0.35f, cam_dist = 15.0f;
static float g_timeOfDay = 0.20f;
static float g_shake = 0.0f;
static int   g_carTex = 0;
static int   g_score = 0;
static float g_msgTimer = 0.0f;
static char  g_msg[64] = "";

/* Weather */
static int   g_raining = 0;
static float g_rainTimer = 0.0f;

/* Clouds */
static float cloud_x[MAX_CLD], cloud_z[MAX_CLD], cloud_y[MAX_CLD], cloud_sz[MAX_CLD];
static int   num_clouds = 0;

static float o_x[MAX_OBJ], o_z[MAX_OBJ], o_w[MAX_OBJ], o_h[MAX_OBJ], o_d[MAX_OBJ];
static int   o_type[MAX_OBJ];
static int   num_obj = 0;

static float a_x[MAX_AI], a_z[MAX_AI], a_spd[MAX_AI];
static int   a_ax[MAX_AI], a_dir[MAX_AI], a_tex[MAX_AI];

/* Particle types */
#define PT_NITRO   0
#define PT_DUST    1
#define PT_SPARK   2
#define PT_SMOKE   3
#define PT_RAIN    4

static float prt_x[MAX_PAR], prt_y[MAX_PAR], prt_z[MAX_PAR];
static float prt_vx[MAX_PAR], prt_vy[MAX_PAR], prt_vz[MAX_PAR];
static int   prt_life[MAX_PAR];
static u32   prt_col[MAX_PAR];
static int   prt_type[MAX_PAR];
static int   num_prt = 0;

/* Pad */
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

static void draw_person(Tex *t, u32 shirt, u32 pants, u32 skin, u32 hair){
    int W=t->w, H=t->h, cx=W/2;
    int headCY=H/8, headR=W/6;
    px_circle(t, cx, headCY, headR, skin);
    for (int y=headCY-headR; y<headCY-headR/3; y++)
        for (int x=cx-headR; x<=cx+headR; x++)
            if ((x-cx)*(x-cx)+(y-headCY)*(y-headCY)<=headR*headR)
                px_set(t,x,y,hair);
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
    px_rect(t, cx-bW-3, bTop+2, cx-bW-1, bBot-4, shirt);
    px_rect(t, cx+bW+1, bTop+2, cx+bW+3, bBot-4, shirt);
    px_rect(t, cx-bW-3, bBot-4, cx-bW-1, bBot-1, skin);
    px_rect(t, cx+bW+1, bBot-4, cx+bW+3, bBot-1, skin);
    int lW=W/8;
    px_rect(t, cx-4-lW, bBot+1, cx-4, H-3, pants);
    px_rect(t, cx+4, bBot+1, cx+4+lW, H-3, pants);
    px_rect(t, cx-5-lW, H-4, cx-3, H-1, 0xFF1A1A1Au);
    px_rect(t, cx+3, H-4, cx+5+lW, H-1, 0xFF1A1A1Au);
}

static void draw_car(Tex *t, u32 body, u32 glass){
    int W=t->w, H=t->h;
    px_rect(t, 2, H/2, W-3, H*3/4, body);
    px_rect(t, W/5, H/3, W*4/5, H/2, body);
    px_rect(t, 2, H/2, W-3, H/2+3, ((body&0xFEFEFE)>>1)|0xFF000000u);
    px_rect(t, W/5+3, H/3+3, W/2-2, H/2-3, glass);
    px_rect(t, W/2+2, H/3+3, W*4/5-3, H/2-3, glass);
    px_rect(t, W/5+5, H/3+5, W/5+9, H/2-5, 0xFF7090B0u);
    px_rect(t, W/2+4, H/3+5, W/2+8, H/2-5, 0xFF7090B0u);
    px_rect(t, W/2-1, H/3+3, W/2+1, H/2-3, body);
    px_rect(t, 3, H/2-1, W-4, H/2, 0xFFFFFFFFu);
    px_rect(t, 3, H/2+3, W/7, H/2+8, 0xFFFFF8C0u);
    px_rect(t, W-W/7, H/2+3, W-4, H/2+8, 0xFFFF3020u);
    px_rect(t, W/4, H/2+9, W*3/4, H/2+11, 0xFF101010u);
    px_circle(t, W/5, H*3/4-1, 7, 0xFF000000u);
    px_circle(t, W/5, H*3/4-1, 5, 0xFF2A2A2Au);
    px_circle(t, W/5, H*3/4-1, 3, 0xFF808890u);
    px_circle(t, W/5, H*3/4-1, 1, 0xFFFFFFFFu);
    px_circle(t, W*4/5, H*3/4-1, 7, 0xFF000000u);
    px_circle(t, W*4/5, H*3/4-1, 5, 0xFF2A2A2Au);
    px_circle(t, W*4/5, H*3/4-1, 3, 0xFF808890u);
    px_circle(t, W*4/5, H*3/4-1, 1, 0xFFFFFFFFu);
}

static void draw_tower(Tex *t, u32 base, u32 win_on, u32 win_off){
    int W=t->w, H=t->h;
    px_rect(t, 0, 0, W-1, H-1, base);
    px_rect(t, 0, 0, W-1, 4, 0xFF1A1A22u);
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
        }
    }
}

static void draw_house(Tex *t){
    int W=t->w, H=t->h;
    for (int y=0; y<H/4; y++){
        int w = ((H/4 - y) * W) / (H/4);
        int x0 = (W-w)/2;
        px_rect(t, x0, y, x0+w, y, 0xFF6A2818u);
    }
    px_rect(t, 4, H/4, W-5, H-4, 0xFFE0C088u);
    px_rect(t, W/6, H*2/5, W/3, H*3/5, 0xFF9EC9E8u);
    px_rect(t, W*2/3, H*2/5, W*5/6, H*3/5, 0xFF9EC9E8u);
    px_rect(t, W/2-7, H*3/4, W/2+7, H-4, 0xFF5A3018u);
}

static void draw_villa(Tex *t){
    int W=t->w, H=t->h;
    px_rect(t, 0, 0, W-1, 8, 0xFF303038u);
    px_rect(t, 2, 10, W-3, H-6, 0xFFEFE8D8u);
    px_rect(t, 2, 10, 6, H-6, 0xFFB0A890u);
    px_rect(t, W-7, 10, W-3, H-6, 0xFFB0A890u);
    for (int i=0; i<4; i++){
        int x0 = 12 + i * (W-24)/4;
        int x1 = x0 + (W-24)/4 - 6;
        px_rect(t, x0, 22, x1, H-30, 0xFF9EC9E8u);
        for (int x=x0; x<=x1; x++){ px_set(t,x,22,0xFF404040u); px_set(t,x,H-30,0xFF404040u); }
    }
    px_rect(t, W/2-14, H-38, W/2+14, H-6, 0xFF6A4A30u);
    px_rect(t, 0, H-6, W-1, H-1, 0xFF3A6A32u);
}

static void draw_shop(Tex *t, u32 wall, u32 sign){
    int W=t->w, H=t->h;
    px_rect(t, 0, 0, W-1, H-1, wall);
    px_rect(t, 0, 0, W-1, 10, 0xFF202020u);
    px_rect(t, 6, 16, W-7, 32, sign);
    px_rect(t, 6, 36, W-7, H-3, 0xFF9EC9E8u);
    for (int x=6; x<=W-7; x++){ px_set(t,x,36,0xFF404040u); px_set(t,x,H-3,0xFF404040u); }
    for (int y=36; y<=H-3; y++){ px_set(t,6,y,0xFF404040u); px_set(t,W-7,y,0xFF404040u); }
}

static void draw_mall(Tex *t){
    int W=t->w, H=t->h;
    px_rect(t, 0, 0, W-1, H-1, 0xFF3A3A4Au);
    px_rect(t, 0, 0, W-1, 12, 0xFF202028u);
    px_rect(t, 20, 18, W-20, 36, 0xFFFF33AAu);
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
        }
    }
    px_rect(t, W/2-20, H-40, W/2+20, H-3, 0xFF202020u);
    px_rect(t, W/2-17, H-37, W/2-1, H-6, 0xFF90C0E0u);
    px_rect(t, W/2+1, H-37, W/2+17, H-6, 0xFF90C0E0u);
}

static void draw_arena(Tex *t){
    int W=t->w, H=t->h;
    int cx=W/2, cy=H/2;
    px_rect(t, 0, 0, W-1, H-1, 0xFF2A3A4Au);
    for (int r=W/2; r>W/3; r-=2){
        u32 c = (r % 8 == 0) ? 0xFF605868u : 0xFF505060u;
        for (int y=-r; y<=r; y++)
            for (int x=-r; x<=r; x++){
                int d2 = x*x + y*y;
                if (d2 <= r*r && d2 > (r-2)*(r-2)) px_set(t, cx+x, cy+y, c);
            }
    }
    for (int y=-W/3; y<=W/3; y++)
        for (int x=-W/3; x<=W/3; x++)
            if (x*x + y*y <= (W/3)*(W/3)) px_set(t, cx+x, cy+y, 0xFF2A8A30u);
    for (int y=-W/3; y<=W/3; y++) px_set(t, cx, cy+y, 0xFFFFFFFFu);
}

static void draw_gas(Tex *t){
    int W=t->w, H=t->h;
    px_rect(t, 0, 0, W-1, H-1, 0xFF303038u);
    px_rect(t, 4, 4, W-5, 14, 0xFFFFCC00u);
    px_rect(t, 4, 14, W-5, 16, 0xFF222222u);
    for (int i=0; i<3; i++){
        int x = 20 + i*(W-50)/3;
        px_rect(t, x, 30, x+14, H-15, 0xFF333338u);
        px_rect(t, x+2, 33, x+12, 45, 0xFF90D090u);
    }
}

static void draw_cafe(Tex *t){
    int W=t->w, H=t->h;
    px_rect(t, 0, 0, W-1, H-1, 0xFF8B6035u);
    for (int i=0; i<W; i+=8){
        u32 col = ((i/8) % 2) ? 0xFFE03030u : 0xFFFFFFFFu;
        px_rect(t, i, 8, i+7, 24, col);
    }
    px_rect(t, 0, 4, W-1, 8, 0xFF4A2818u);
    px_rect(t, 6, 30, W-7, H-25, 0xFFE0F0FFu);
    px_rect(t, W/2-8, H-25, W/2+8, H-3, 0xFF4A2818u);
}

static void draw_tree(Tex *t, int variant){
    int W=t->w, H=t->h, cx=W/2;
    px_rect(t, cx-4, H*2/3, cx+4, H-2, 0xFF4A2E1Au);
    u32 dark=0xFF15481Cu, mid=0xFF2A7030u, light=0xFF4A9848u, bright=0xFF6AC868u;
    if (variant == 0){
        px_circle(t, cx, H/4, W/3, dark);
        px_circle(t, cx-6, H/3, W/4, mid);
        px_circle(t, cx+6, H/3, W/4, mid);
        px_circle(t, cx, H/5, W/4, light);
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

static void draw_road(Tex *t){
    int W=t->w, H=t->h;
    px_rect(t, 0, 0, W-1, H-1, 0xFF383840u);
    for (int y=0; y<H; y+=8) px_rect(t, W/2-1, y, W/2+1, y+4, 0xFFFFCC00u);
    px_rect(t, 4, 0, 5, H-1, 0xFFFFFFFFu);
    px_rect(t, W-6, 0, W-5, H-1, 0xFFFFFFFFu);
}

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

/* ===================== BLOOM ===================== */
/* Glow around a point in screen space */
static void draw_glow(float sx, float sy, float radius, u32 col){
    /* layered transparent circles */
    for (int layer = 4; layer >= 1; layer--){
        float r = radius * (float)layer / 4.0f;
        u8 a = (u8)(20 + (5 - layer) * 15);
        u32 c = ((u32)a << 24) | (col & 0xFFFFFF);
        /* draw a filled circle as many horizontal strips */
        for (int dy = -(int)r; dy <= (int)r; dy += 3){
            float w = sqrtf(r*r - dy*dy);
            draw_rect2d(sx - w, sy + dy, w*2, 3, c);
        }
    }
}

/* ===================== GROUND SHADOW ===================== */
static void draw_ground_plate(float cx, float cz, float w, float d, u32 col){
    float hw=w*0.5f, hd=d*0.5f;
    draw_tex_face(NULL,
        cx-hw, 0.005f, cz-hd,  0,0,
        cx+hw, 0.005f, cz-hd,  0,0,
        cx+hw, 0.005f, cz+hd,  0,0,
        cx-hw, 0.005f, cz+hd,  0,0, col);
}
static void draw_shadow(float cx, float cz, float w, float d){
    draw_ground_plate(cx, cz, w*1.30f, d*1.30f, 0x28000000u);
    draw_ground_plate(cx, cz, w*1.15f, d*1.15f, 0x48000000u);
    draw_ground_plate(cx, cz, w*1.00f, d*1.00f, 0x80000000u);
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

/* ===================== CITY ===================== */
static void add_obj(int type, float x, float z, float w, float h, float d){
    if (num_obj >= MAX_OBJ) return;
    o_x[num_obj]=x; o_z[num_obj]=z; o_w[num_obj]=w; o_h[num_obj]=h; o_d[num_obj]=d;
    o_type[num_obj]=type;
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
            if (gi==3 && gj==0){ add_obj(BT_ARENA, cx, cz, 44, 16, 44); continue; }
            if (gi==0 && gj==3){ add_obj(BT_MALL, cx, cz, 42, 24, 30); continue; }
            if (gi==-2 && gj==-2){ add_obj(BT_GAS, cx, cz, 22, 7, 16); continue; }
            if (gi==-3 && gj==2){ add_obj(BT_CAFE, cx, cz, 16, 10, 14); continue; }
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

/* ===================== AI ===================== */
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
static void spawn_particle(int type, float x,float y,float z,
                            float vx,float vy,float vz,u32 col,int life){
    if (num_prt >= MAX_PAR) return;
    prt_x[num_prt]=x; prt_y[num_prt]=y; prt_z[num_prt]=z;
    prt_vx[num_prt]=vx; prt_vy[num_prt]=vy; prt_vz[num_prt]=vz;
    prt_life[num_prt]=life;
    prt_col[num_prt]=col;
    prt_type[num_prt]=type;
    num_prt++;
}
static void update_particles(float dt){
    for (int i = num_prt-1; i >= 0; i--){
        prt_x[i]+=prt_vx[i]*dt; prt_y[i]+=prt_vy[i]*dt; prt_z[i]+=prt_vz[i]*dt;
        if (prt_type[i] == PT_RAIN)
            prt_vy[i] -= 2.0f*dt;
        else
            prt_vy[i] -= 6.0f*dt;
        prt_life[i]--;
        if (prt_life[i]<=0 || prt_y[i] < -2.0f){
            prt_x[i]=prt_x[num_prt-1]; prt_y[i]=prt_y[num_prt-1]; prt_z[i]=prt_z[num_prt-1];
            prt_vx[i]=prt_vx[num_prt-1]; prt_vy[i]=prt_vy[num_prt-1]; prt_vz[i]=prt_vz[num_prt-1];
            prt_life[i]=prt_life[num_prt-1]; prt_col[i]=prt_col[num_prt-1];
            prt_type[i]=prt_type[num_prt-1];
            num_prt--;
        }
    }
}

/* ===================== CLOUDS ===================== */
static void init_clouds(void){
    num_clouds = 0;
    for (int i=0; i<MAX_CLD; i++){
        cloud_x[num_clouds] = (float)(rng()%1600) - 800.0f;
        cloud_y[num_clouds] = 25.0f + (rng()%30);
        cloud_z[num_clouds] = (float)(rng()%1600) - 800.0f;
        cloud_sz[num_clouds] = 15.0f + (rng()%25);
        num_clouds++;
    }
}
static void update_clouds(float dt){
    for (int i=0; i<num_clouds; i++){
        cloud_x[i] += dt * 2.0f;
        if (cloud_x[i] > p_x + 900.0f){
            cloud_x[i] = p_x - 900.0f;
            cloud_z[i] = p_z + (float)(rng()%1600) - 800.0f;
            cloud_y[i] = 25.0f + (rng()%30);
            cloud_sz[i] = 15.0f + (rng()%25);
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

/* ===================== MESSAGES ===================== */
static void show_msg(const char* m){
    strncpy(g_msg, m, 63); g_msg[63]=0;
    g_msgTimer = 2.0f;
}

/* ===================== UPDATE GAME ===================== */
static void update_game(float dt){
    if (!g_padReady) return;
    u16 btn = pad_btns();
    int lx = pad_lx(), ly = pad_ly();
    int rx = pad_rx(), ry = pad_ry();

    cam_yaw += (rx/128.0f)*2.6f*dt;
    cam_pitch = clampf(cam_pitch + (ry/128.0f)*1.5f*dt, -0.1f, 1.3f);
    if (btn & BTN_R2) cam_dist += 15.0f*dt;
    if (btn & BTN_L2) cam_dist -= 15.0f*dt;
    cam_dist = clampf(cam_dist, 6.0f, 35.0f);

    /* L3: change car color */
    u8 l3 = (btn & BTN_L3) ? 1 : 0;
    if (l3 && !g_prevL3){
        g_carTex = (g_carTex + 1) % 5;
        const char *names[] = {"RED","BLUE","YELLOW","GREEN","PURPLE"};
        char buf[64];
        sprintf(buf, "Car color: %s", names[g_carTex]);
        show_msg(buf);
    }
    g_prevL3 = l3;

    /* R3: teleport */
    u8 r3 = (btn & BTN_R3) ? 1 : 0;
    if (r3 && !g_prevR3){
        p_x = 0; p_z = 0; p_speed = 0; p_heading = 0;
        show_msg("Teleport to Villa");
    }
    g_prevR3 = r3;

    /* Start: pause */
    u8 st = (btn & BTN_START) ? 1 : 0;
    if (st && !g_prevStart){ g_state = ST_PAUSED; }
    g_prevStart = st;

    float thr = -(ly/128.0f);
    float stt = -(lx/128.0f);
    if (btn & BTN_UP)    thr = 1.0f;
    if (btn & BTN_DOWN)  thr = -1.0f;
    if (btn & BTN_LEFT)  stt = 1.0f;
    if (btn & BTN_RIGHT) stt = -1.0f;

    /* boost + nitro particles */
    if ((btn & BTN_R1) && p_boost > 0.0f){
        p_speed += 45.0f*dt;
        p_boost -= 0.45f*dt;
        float bx = p_x - sinf(p_heading)*3.0f;
        float bz = p_z - cosf(p_heading)*3.0f;
        for (int k=0;k<3;k++){
            spawn_particle(PT_NITRO,
                bx+(rng()%100-50)*0.05f,
                0.5f+(rng()%100)*0.02f,
                bz+(rng()%100-50)*0.05f,
                (rng()%100-50)*0.03f - sinf(p_heading)*3.0f,
                (rng()%100)*0.02f,
                (rng()%100-50)*0.03f - cosf(p_heading)*3.0f,
                (k%2) ? 0xFFFF8800u : 0xFFFFDD00u,
                30 + rng()%20);
        }
    } else {
        p_boost = clampf(p_boost + 0.15f*dt, 0.0f, 1.0f);
    }

    /* brake + dust particles */
    if (btn & BTN_L1){
        p_speed *= (1.0f - 3.5f*dt);
        g_shake = 0.5f;
        if (fabsf(p_speed) > 10.0f && (rng()%3==0)){
            spawn_particle(PT_DUST,
                p_x + (rng()%100-50)*0.05f,
                0.2f,
                p_z + (rng()%100-50)*0.05f,
                (rng()%100-50)*0.05f, 1.0f, (rng()%100-50)*0.05f,
                0xFFCCCCCCu, 20 + rng()%15);
        }
    }

    p_speed += thr*32.0f*dt;
    if (fabsf(thr) < 0.1f) p_speed *= (1.0f - 1.3f*dt);
    p_speed = clampf(p_speed, -20.0f, 65.0f);

    if (fabsf(p_speed) > 0.5f){
        float sgn = p_speed > 0 ? 1.0f : -1.0f;
        float gain = clampf(fabsf(p_speed)/15.0f, 0.3f, 1.0f);
        p_heading += stt * 2.2f * gain * dt * sgn;
    }

    /* exhaust smoke */
    if (fabsf(p_speed) > 2.0f && (rng()%4==0)){
        float bx = p_x - sinf(p_heading)*2.5f;
        float bz = p_z - cosf(p_heading)*2.5f;
        spawn_particle(PT_SMOKE,
            bx, 0.3f, bz,
            (rng()%100-50)*0.02f, 0.5f, (rng()%100-50)*0.02f,
            0x80888888u, 25);
    }

    float vx = sinf(p_heading)*p_speed*dt;
    float vz = cosf(p_heading)*p_speed*dt;
    float nx = p_x + vx, nz = p_z + vz;
    int   t_type = -1;

    if (!hit_obj(nx, p_z, 1.8f, &t_type)) p_x = nx;
    else {
        p_speed *= 0.15f; g_shake = 0.8f;
        for (int k=0;k<8;k++){
            spawn_particle(PT_SPARK,
                nx, 0.5f, p_z,
                (rng()%100-50)*0.1f, (rng()%100)*0.1f, (rng()%100-50)*0.1f,
                0xFFFFDD00u, 15);
        }
        if (t_type == BT_SHOP || t_type == BT_MALL){ g_score += 10; show_msg("Shopped! +10"); }
        else if (t_type == BT_GAS){ p_boost = 1.0f; show_msg("Refueled!"); }
        else if (t_type == BT_CAFE){ g_score += 5; show_msg("Coffee! +5"); }
    }
    if (!hit_obj(p_x, nz, 1.8f, &t_type)) p_z = nz;
    else {
        p_speed *= 0.15f; g_shake = 0.8f;
        for (int k=0;k<8;k++){
            spawn_particle(PT_SPARK,
                p_x, 0.5f, nz,
                (rng()%100-50)*0.1f, (rng()%100)*0.1f, (rng()%100-50)*0.1f,
                0xFFFFDD00u, 15);
        }
        if (t_type == BT_SHOP || t_type == BT_MALL){ g_score += 10; show_msg("Shopped! +10"); }
        else if (t_type == BT_GAS){ p_boost = 1.0f; show_msg("Refueled!"); }
        else if (t_type == BT_CAFE){ g_score += 5; show_msg("Coffee! +5"); }
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

    /* weather */
    g_rainTimer += dt;
    if (g_rainTimer > 40.0f){
        g_rainTimer = 0.0f;
        g_raining = !g_raining;
    }
    if (g_raining && (rng()%2==0)){
        for (int k=0;k<6;k++){
            spawn_particle(PT_RAIN,
                p_x + (rng()%100 - 50)*0.5f,
                20.0f + (rng()%10),
                p_z + (rng()%100 - 50)*0.5f,
                0.0f, -30.0f, 0.0f,
                0x8066AACC, 30);
        }
    }

    update_ai(dt);
    update_clouds(dt);
    update_particles(dt);
}

/* ===================== SKY ===================== */
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

/* ===================== RENDER WORLD ===================== */
static void render_world(void){
    /* sky */
    u32 skyTop = sky_col_top();
    u32 skyMid = sky_col_mid();
    float sY = sinf((g_timeOfDay - 0.25f) * 6.2831853f);
    u32 horizon = (sY > 0) ? 0xFFFFBB88u : 0xFF2A2A4Au;

    tiny3d_Clear(skyTop, 0xFFFFFFFF);
    draw_rect2d(0, 0, SCR_W, SCR_H*0.30f, skyTop);
    draw_rect2d(0, SCR_H*0.30f, SCR_W, SCR_H*0.20f, skyMid);
    draw_rect2d(0, SCR_H*0.50f, SCR_W, SCR_H*0.08f, horizon);
    draw_rect2d(0, SCR_H*0.58f, SCR_W, 4, 0xFFFFCC88u);

    /* sun / moon with bloom */
    if (sY > 0){
        int sx = (int)(SCR_W * 0.65f);
        int sy = (int)(SCR_H * 0.30f - sY * SCR_H * 0.20f);
        draw_glow(sx, sy, 120, 0xFFEECC88u);
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
        draw_glow(sx, sy, 90, 0xFFCCDDFFu);
        for (int y=-30; y<=30; y++)
            for (int x=-30; x<=30; x++)
                if (x*x + y*y <= 30*30){
                    int px = sx + x, py = sy + y;
                    if (px >= 0 && px < SCR_W && py >= 0 && py < SCR_H)
                        draw_rect2d(px, py, 2, 2, 0xFFEEEEFFu);
                }
    }

    /* regenerate city on move */
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

    /* clouds (billboards) */
    for (int i=0; i<num_clouds; i++){
        float dx = cloud_x[i]-p_x, dz = cloud_z[i]-p_z;
        if (dx*dx + dz*dz > 900.0f*900.0f) continue;
        draw_billboard(&tex_road, cloud_x[i], cloud_y[i], cloud_z[i],
                       cloud_sz[i], cloud_sz[i]*0.4f, 0x80FFFFFFu);
    }

    /* sort objects by distance */
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

    /* car headlights bloom at night */
    if (sY < 0){
        float hlx = p_x + sinf(p_heading)*2.5f;
        float hlz = p_z + cosf(p_heading)*2.5f;
        SP sp;
        project(hlx, 0.8f, hlz, &sp);
        if (sp.vis) draw_glow(sp.x, sp.y, 60, 0xFFFFEEAAu);
    }

    draw_billboard(pcar, p_x, 0.15f, p_z, 6.0f, 3.0f, 0xFFFFFFFFu);

    /* particles */
    for (int i=0;i<num_prt;i++){
        float sz = 0.5f;
        u32 col = prt_col[i];
        if (prt_type[i] == PT_NITRO) sz = 0.7f;
        else if (prt_type[i] == PT_DUST) sz = 1.2f;
        else if (prt_type[i] == PT_SPARK) sz = 0.25f;
        else if (prt_type[i] == PT_SMOKE) sz = 1.5f;
        else if (prt_type[i] == PT_RAIN) sz = 0.15f;
        draw_billboard(&tex_road, prt_x[i], prt_y[i], prt_z[i], sz, sz, col);
    }

    /* rain overlay */
    if (g_raining){
        for (int i=0; i<8; i++){
            int rx = (int)((rng()%SCR_W));
            draw_rect2d(rx, 0, 2, SCR_H, 0x15AACCEEu);
        }
    }
}

/* ===================== HUD ===================== */
static void draw_hud(void){
    char buf[128];

    /* top-left panel */
    draw_rect2d(15, 15, 260, 70, 0x80000000u);
    draw_rect2d(15, 15, 260, 3, 0xFF00D4FFu);

    SetFontSize(28, 28);
    SetFontColor(0xFF00D4FFu, 0x00000000);
    DrawString(25, 45, "NEON CITY");

    SetFontSize(12, 12);
    SetFontColor(0xFFFF6B35u, 0x00000000);
    DrawString(27, 68, "U L T R A   v 4");

    /* speed panel */
    draw_rect2d(15, SCR_H - 130, 240, 115, 0x80000000u);
    draw_rect2d(15, SCR_H - 130, 3, 115, 0xFFFFCC00u);

    sprintf(buf, "%d", (int)(fabsf(p_speed)*5.4f));
    SetFontSize(44, 44);
    SetFontColor(0xFFFFCC00u, 0x00000000);
    DrawString(30, SCR_H - 95, buf);

    SetFontSize(14, 14);
    SetFontColor(0xFFFFE080u, 0x00000000);
    DrawString(140, SCR_H - 95, "KM/H");

    /* boost bar */
    SetFontSize(12, 12);
    SetFontColor(0xFFFF6B35u, 0x00000000);
    DrawString(30, SCR_H - 60, "BOOST");
    draw_rect2d(90, SCR_H - 63, 150, 12, 0xFF222222u);
    draw_rect2d(90, SCR_H - 63, (int)(p_boost*150.0f), 12, 0xFFFF6B35u);
    draw_rect2d(90, SCR_H - 63, 150, 1, 0xFFFFFFFFu);

    /* score */
    SetFontSize(14, 14);
    SetFontColor(0xFF2ECC71u, 0x00000000);
    sprintf(buf, "SCORE: %d", g_score);
    DrawString(30, SCR_H - 35, buf);

    /* message */
    if (g_msgTimer > 0.0f){
        int w = (int)strlen(g_msg)*12;
        draw_rect2d(SCR_W/2 - w/2 - 20, 110, w + 40, 40, 0x80000000u);
        SetFontSize(22, 22);
        SetFontColor(0xFFFFFFFFu, 0x00000000);
        DrawString(SCR_W/2 - w/2, 138, g_msg);
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
            else if (o_type[i]==BT_TREE) col=0xFF2E7A30u;
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
    draw_rect2d(SCR_W - 130, SCR_H - 55, 100, 30, 0x80000000u);
    SetFontSize(20, 20);
    SetFontColor(0xFFFFE080u, 0x00000000);
    DrawString(SCR_W - 125, SCR_H - 32, buf);

    /* hints */
    SetFontSize(11, 11);
    SetFontColor(0xFFBBBBBBu, 0x00000000);
    DrawString(24, SCR_H - 8, "L:drive  R:cam  R1:nitro  L1:brake  L3:color  R3:reset  START:pause");
}

/* ===================== LOADING SCREEN ===================== */
static void draw_loading(float dt){
    g_loadProgress += dt * 0.35f;
    if (g_loadProgress > 1.0f) g_loadProgress = 1.0f;

    tiny3d_Clear(0xFF05101Fu, 0xFFFFFFFF);

    /* background grid */
    for (int i = 0; i < 20; i++){
        int y = 40 + i*36;
        draw_rect2d(0, y, SCR_W, 1, 0x3000D4FFu);
    }

    /* glow behind title */
    draw_glow(SCR_W/2, 200, 250, 0xFF00AAFFu);

    SetFontSize(80, 80);
    SetFontColor(0xFF00D4FFu, 0x00000000);
    DrawString(SCR_W/2 - 280, 220, "NEON CITY");

    SetFontSize(28, 28);
    SetFontColor(0xFFFF6B35u, 0x00000000);
    DrawString(SCR_W/2 - 100, 300, "U L T R A   v 4");

    /* progress bar */
    int bw = 600;
    int bx = SCR_W/2 - bw/2;
    int by = 480;

    draw_rect2d(bx-3, by-3, bw+6, 36, 0xFF00D4FFu);
    draw_rect2d(bx, by, bw, 30, 0xFF222222u);

    int fill = (int)(g_loadProgress * bw);
    draw_rect2d(bx, by, fill, 30, 0xFFFF6B35u);
    draw_rect2d(bx, by, fill, 10, 0xFFFFD700u);

    char buf[64];
    sprintf(buf, "%d%%", (int)(g_loadProgress * 100.0f));
    SetFontSize(20, 20);
    SetFontColor(0xFFFFFFFFu, 0x00000000);
    DrawString(SCR_W/2 - 20, by + 25, buf);

    SetFontSize(14, 14);
    SetFontColor(0xFFAAAAAAu, 0x00000000);
    DrawString(SCR_W/2 - 130, 570, "Loading textures and world...");

    if (g_loadProgress >= 1.0f){
        g_state = ST_MENU;
        g_menuSel = 0;
        g_menuTimer = 0.0f;
    }
}

/* ===================== MAIN MENU ===================== */
static void draw_menu(float dt){
    g_menuTimer += dt;

    /* animated sky background */
    u32 skyTop = 0xFF05101Fu;
    u32 skyMid = 0xFF1A4070u;
    tiny3d_Clear(skyTop, 0xFFFFFFFF);
    draw_rect2d(0, 0, SCR_W, SCR_H*0.5f, skyTop);
    draw_rect2d(0, SCR_H*0.5f, SCR_W, SCR_H*0.5f, 0xFF0A1525u);

    /* moving stars */
    for (int i=0; i<80; i++){
        int sx = (i*173 + (int)(g_menuTimer*30)) % SCR_W;
        int sy = (i*67) % (SCR_H/2);
        u8 b = 100 + (i%5)*30;
        draw_rect2d(sx, sy, 2, 2, 0xFF000000u | ((u32)b<<16) | ((u32)b<<8) | b);
    }

    /* perspective grid */
    for (int i=-10; i<=10; i++){
        float x1 = SCR_W/2.0f + i*30;
        float x2 = SCR_W/2.0f + i*200;
        for (int t=0; t<20; t++){
            float y0 = SCR_H*0.5f + t*t*0.8f + 20;
            float y1 = SCR_H*0.5f + (t+1)*(t+1)*0.8f + 20;
            if (y0 >= SCR_H) break;
            float xa = x1 + (x2-x1)*(float)t/20.0f;
            float xb = x1 + (x2-x1)*(float)(t+1)/20.0f;
            draw_rect2d(xa, y0, 1, y1-y0, 0x2000AAFFu);
            (void)xb;
        }
    }
    for (int i=0; i<10; i++){
        float y = SCR_H*0.5f + i*i*3.5f + 20;
        if (y >= SCR_H) break;
        draw_rect2d(0, y, SCR_W, 1, 0x3000D4FFu);
    }

    /* glow title */
    draw_glow(SCR_W/2, 180, 300, 0xFF00AAFFu);

    SetFontSize(90, 90);
    SetFontColor(0xFF00D4FFu, 0x00000000);
    DrawString(SCR_W/2 - 310, 210, "NEON CITY");

    SetFontSize(28, 28);
    SetFontColor(0xFFFF6B35u, 0x00000000);
    DrawString(SCR_W/2 - 110, 290, "U L T R A   v 4");

    /* menu items */
    const char *items[] = { "START GAME", "INSTRUCTIONS", "EXIT" };
    int num_items = 3;
    for (int i=0; i<num_items; i++){
        int y = 420 + i*70;
        int x = SCR_W/2 - 180;

        int selected = (i == g_menuSel);
        if (selected){
            float pulse = 0.5f + sinf(g_menuTimer*4.0f)*0.5f;
            u8 a = (u8)(150 + pulse*105);
            u32 bg = ((u32)a << 24) | 0x00D4FF;
            draw_rect2d(x-20, y-20, 400, 60, bg);
            draw_rect2d(x-20, y-20, 400, 3, 0xFFFFD700u);
            draw_rect2d(x-20, y+37, 400, 3, 0xFFFFD700u);
        } else {
            draw_rect2d(x-20, y-20, 400, 60, 0x60000000u);
        }

        SetFontSize(32, 32);
        if (selected)
            SetFontColor(0xFFFFFFFFu, 0x00000000);
        else
            SetFontColor(0xFFAAAAAAu, 0x00000000);
        DrawString(x, y+14, items[i]);
    }

    /* footer */
    SetFontSize(12, 12);
    SetFontColor(0xFF888888u, 0x00000000);
    DrawString(SCR_W/2 - 180, SCR_H - 40, "D-PAD: navigate     CROSS: select");
}

/* ===================== PAUSE ===================== */
static void draw_pause(void){
    draw_rect2d(0, 0, SCR_W, SCR_H, 0xA0000000u);

    int x = SCR_W/2 - 250;
    draw_rect2d(x, 180, 500, 360, 0xFF101820u);
    draw_rect2d(x, 180, 500, 4, 0xFF00D4FFu);
    draw_rect2d(x, 536, 500, 4, 0xFF00D4FFu);

    SetFontSize(40, 40);
    SetFontColor(0xFF00D4FFu, 0x00000000);
    DrawString(x+130, 240, "PAUSED");

    const char *items[] = { "RESUME", "QUIT TO MENU" };
    for (int i=0; i<2; i++){
        int y = 320 + i*70;
        int selected = (i == g_menuSel);
        if (selected){
            draw_rect2d(x+40, y-15, 420, 55, 0x6000D4FFu);
            draw_rect2d(x+40, y-15, 420, 2, 0xFFFFD700u);
        }
        SetFontSize(28, 28);
        if (selected) SetFontColor(0xFFFFFFFFu, 0x00000000);
        else          SetFontColor(0xFFAAAAAAu, 0x00000000);
        DrawString(x+60, y+18, items[i]);
    }
}

/* ===================== UPDATE MENU ===================== */
static void update_menu(float dt){
    if (!g_padReady) return;
    u16 btn = pad_btns();

    u8 up   = (btn & BTN_UP) ? 1 : 0;
    u8 down = (btn & BTN_DOWN) ? 1 : 0;
    u8 cross = (btn & BTN_CROSS) ? 1 : 0;

    if (up && !g_prevUp){ g_menuSel--; if (g_menuSel < 0) g_menuSel = 2; }
    g_prevUp = up;

    if (down && !g_prevDown){ g_menuSel++; if (g_menuSel > 2) g_menuSel = 0; }
    g_prevDown = down;

    if (cross && !g_prevCross){
        if (g_menuSel == 0){
            /* reset game */
            p_x = 0; p_z = 0; p_heading = 0; p_speed = 0; p_boost = 1.0f;
            g_score = 0;
            gen_city();
            init_ai();
            init_clouds();
            num_prt = 0;
            show_msg("Welcome to Neon City!");
            g_state = ST_PLAYING;
        } else if (g_menuSel == 1){
            show_msg("Use L-Stick to drive, R-Stick to look");
            g_msgTimer = 4.0f;
        } else {
            g_running = 0;
        }
    }
    g_prevCross = cross;
    (void)dt;
}

static void update_pause(float dt){
    if (!g_padReady) return;
    u16 btn = pad_btns();
    u8 up   = (btn & BTN_UP) ? 1 : 0;
    u8 down = (btn & BTN_DOWN) ? 1 : 0;
    u8 cross = (btn & BTN_CROSS) ? 1 : 0;
    u8 st    = (btn & BTN_START) ? 1 : 0;

    if (up && !g_prevUp){ g_menuSel--; if (g_menuSel < 0) g_menuSel = 1; }
    g_prevUp = up;
    if (down && !g_prevDown){ g_menuSel++; if (g_menuSel > 1) g_menuSel = 0; }
    g_prevDown = down;

    if (st && !g_prevStart){ g_state = ST_PLAYING; g_prevStart = st; return; }
    g_prevStart = st;

    if (cross && !g_prevCross){
        if (g_menuSel == 0){ g_state = ST_PLAYING; }
        else { g_state = ST_MENU; g_menuSel = 0; }
    }
    g_prevCross = cross;
    (void)dt;
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
    gen_city();
    init_ai();
    init_clouds();

    struct timeval tv; gettimeofday(&tv, NULL);
    rng_s = (unsigned int)tv.tv_usec;

    while (g_running){
        sysUtilCheckCallback();
        g_padReady = 0;
        if (ioPadGetInfo(&g_padInfo) == 0 && g_padInfo.status[0]){
            if (ioPadGetData(0, &g_padData) == 0) g_padReady = 1;
        }

        if (g_padReady && (pad_btns() & BTN_SELECT) && g_state == ST_MENU){
            g_running = 0;
        }

        float dt = 1.0f / 60.0f;

        switch (g_state){
            case ST_LOADING:
                draw_loading(dt);
                break;

            case ST_MENU:
                update_menu(dt);
                draw_menu(dt);
                break;

            case ST_PLAYING:
                update_game(dt);
                render_world();
                draw_hud();
                break;

            case ST_PAUSED:
                render_world();
                draw_hud();
                draw_pause();
                update_pause(dt);
                break;
        }

        tiny3d_Flip();
    }

    ioPadEnd();
    tiny3d_Exit();
    return 0;
}
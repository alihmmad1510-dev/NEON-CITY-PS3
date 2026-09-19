/* =====================================================================
 *   NEON CITY: ULTRA - PS3 Homebrew
 *   Procedural Textures + Procedural Audio + Full 3D
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
#include <audio/audio.h>

#include <tiny3d.h>
#include <libfont.h>

SYS_PROCESS_PARAM(1001, 0x100000)

/* ============================================================
 *  ضمان تعريف الدوال الناقصة (weak stubs)
 *  لو المكتبة عندها النسخ الحقيقية → تُستخدم هي
 *  لو مش عندها → بتاعتنا الفارغة تُستخدم
 * ============================================================ */
__attribute__((weak)) void tiny3d_TextureFormat(int f)           { (void)f; }
__attribute__((weak)) void tiny3d_TextureEnable(void)            { }
__attribute__((weak)) void tiny3d_TextureDisable(void)           { }
__attribute__((weak)) void tiny3d_TextureFilter(int a, int b)    { (void)a; (void)b; }
__attribute__((weak)) void tiny3d_TextureWrap(int a, int b)      { (void)a; (void)b; }
__attribute__((weak)) void tiny3d_TextureBlend(int b)            { (void)b; }

/* ثوابت الـ texture — لو مش موجودة نعرّفها */
#ifndef TINY3D_TEX_FORMAT_A8R8G8B8
#define TINY3D_TEX_FORMAT_A8R8G8B8  0x85
#endif
#ifndef TINY3D_TEX_FILTER_LINEAR
#define TINY3D_TEX_FILTER_LINEAR    1
#endif
#ifndef TINY3D_TEX_WRAP_CLAMP
#define TINY3D_TEX_WRAP_CLAMP       1
#endif

/* ===================== Config ===================== */
#define SCR_W 1280
#define SCR_H 720
#define FOV   620.0f

#define MAX_BLD 120
#define MAX_AI  12
#define MAX_TREE 60
#define MAX_PAR 80

/* ===================== Audio (raw u32) ===================== */
static u32   g_audioPort = 0;
static int   g_audioOK   = 0;
static float g_enginePhase = 0.0f;

/* ===================== Texture ===================== */
typedef struct {
    u16 width;
    u16 height;
    u32 *bmp_out;
} LocalPngData;

typedef struct {
    LocalPngData png;
    u16 w, h;
    int ok;
    u32 *pixels;
} Tex;

static Tex tex_player, tex_car_red, tex_car_blue, tex_car_yel;
static Tex tex_tree, tex_building, tex_road, tex_ped;

/* ===================== Game State ===================== */
static int      g_running = 1;
static padInfo  g_padInfo;
static padData  g_padData;
static int      g_padReady = 0;

static float p_x = 0, p_z = 0, p_heading = 0, p_speed = 0, p_boost = 1.0f;
static float cam_yaw = 0, cam_pitch = 0.35f, cam_dist = 12.0f;
static float g_timeOfDay = 0.20f;
static float g_shake = 0.0f;

/* Buildings */
static float b_x[MAX_BLD], b_z[MAX_BLD], b_w[MAX_BLD], b_h[MAX_BLD], b_d[MAX_BLD];
static u32   b_col[MAX_BLD];
static int   num_bld = 0;

/* Trees */
static float t_x[MAX_TREE], t_z[MAX_TREE];
static int   num_tree = 0;

/* AI */
static float a_x[MAX_AI], a_z[MAX_AI], a_spd[MAX_AI];
static int   a_ax[MAX_AI], a_dir[MAX_AI];
static int   a_tex[MAX_AI];

/* Particles */
static float prt_x[MAX_PAR], prt_y[MAX_PAR], prt_z[MAX_PAR];
static float prt_vx[MAX_PAR], prt_vy[MAX_PAR], prt_vz[MAX_PAR];
static int   prt_life[MAX_PAR];
static u32   prt_col[MAX_PAR];
static int   num_prt = 0;

/* ===================== Pad raw ===================== */
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

/* ===================== Utils ===================== */
static float clampf(float v,float a,float b){ return v<a?a:(v>b?b:v); }

/* =====================================================================
 *  PROCEDURAL TEXTURES
 * ===================================================================== */
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

static void tex_draw_person(Tex *t, u32 shirt, u32 pants, u32 skin, u32 hair){
    int W = t->w, H = t->h;
    int cx = W/2;
    int headCY = H/8;
    int headR  = W/6;

    px_circle(t, cx, headCY, headR, skin);
    for (int y = headCY-headR; y < headCY-headR/3; y++)
        for (int x = cx-headR; x <= cx+headR; x++)
            if ((x-cx)*(x-cx)+(y-headCY)*(y-headCY) <= headR*headR)
                px_set(t, x, y, hair);
    px_rect(t, cx-headR/2, headCY, cx-headR/2+1, headCY+1, 0xFF000000u);
    px_rect(t, cx+headR/2-1, headCY, cx+headR/2, headCY+1, 0xFF000000u);
    px_rect(t, cx-2, headCY+headR/2, cx+2, headCY+headR/2, 0xFF8B3030u);

    int bTop = headCY + headR + 2;
    int bBot = H*3/5;
    int bW   = W/5;
    px_rect(t, cx-bW, bTop, cx+bW, bBot, shirt);
    px_rect(t, cx-bW+1, bTop, cx+bW-1, bTop+1, 0xFFDDDDDDu);
    for (int y = bTop+4; y < bBot; y += 7) px_set(t, cx, y, 0xFF333333u);

    px_rect(t, cx-bW-3, bTop+2, cx-bW-1, bBot-4, shirt);
    px_rect(t, cx+bW+1, bTop+2, cx+bW+3, bBot-4, shirt);
    px_rect(t, cx-bW-3, bBot-4, cx-bW-1, bBot-1, skin);
    px_rect(t, cx+bW+1, bBot-4, cx+bW+3, bBot-1, skin);

    int lW = W/8;
    px_rect(t, cx-4-lW, bBot+1, cx-4, H-3, pants);
    px_rect(t, cx+4, bBot+1, cx+4+lW, H-3, pants);
    px_rect(t, cx-5-lW, H-4, cx-3, H-1, 0xFF1A1A1Au);
    px_rect(t, cx+3, H-4, cx+5+lW, H-1, 0xFF1A1A1Au);
}

static void tex_draw_car(Tex *t, u32 body, u32 glass){
    int W = t->w, H = t->h;
    px_rect(t, 2, H/2, W-3, H*3/4, body);
    px_rect(t, W/5, H/3, W*4/5, H/2, body);
    px_rect(t, W/5+3, H/3+3, W/2-2, H/2-3, glass);
    px_rect(t, W/2+2, H/3+3, W*4/5-3, H/2-3, glass);
    px_rect(t, W/2-1, H/3+3, W/2+1, H/2-3, body);
    px_rect(t, 3, H/2-1, W-4, H/2, 0xFFFFFFFFu);
    px_rect(t, 3, H/2+3, W/7, H/2+7, 0xFFFFF8C0u);
    px_rect(t, W-W/7, H/2+3, W-4, H/2+7, 0xFFFF3020u);
    px_rect(t, W/4, H/2+6, W*3/4, H/2+8, 0xFF101010u);
    px_circle(t, W/5, H*3/4-1, 6, 0xFF0A0A0Au);
    px_circle(t, W/5, H*3/4-1, 3, 0xFF808890u);
    px_circle(t, W*4/5, H*3/4-1, 6, 0xFF0A0A0Au);
    px_circle(t, W*4/5, H*3/4-1, 3, 0xFF808890u);
}

static void tex_draw_tree(Tex *t){
    int W = t->w, H = t->h;
    int cx = W/2;
    px_rect(t, cx-3, H*2/3, cx+3, H-2, 0xFF4A2E1Au);
    for (int y = H*2/3; y < H-2; y += 5) px_set(t, cx-1, y, 0xFF3A2010u);
    px_circle(t, cx, H/4, W/3, 0xFF1A5A20u);
    px_circle(t, cx-8, H/3, W/4, 0xFF2E7A30u);
    px_circle(t, cx+8, H/3, W/4, 0xFF2E7A30u);
    px_circle(t, cx, H/5, W/4, 0xFF4AA050u);
    px_circle(t, cx-4, H/3+2, W/5, 0xFF4AA050u);
    px_circle(t, cx+4, H/3+2, W/5, 0xFF4AA050u);
}

static void tex_draw_building(Tex *t, u32 wall, u32 trim){
    int W = t->w, H = t->h;
    px_rect(t, 4, 4, W-5, H-5, wall);
    for (int x=4; x<W-4; x++){ px_set(t,x,4,trim); px_set(t,x,H-5,trim); }
    for (int y=4; y<H-4; y++){ px_set(t,4,y,trim); px_set(t,W-5,y,trim); }

    int cols = 4, rows = 5;
    int mx = 12, my = 12;
    int ww = (W - 2*mx) / cols - 5;
    int wh = (H - 2*my) / rows - 5;
    for (int r = 0; r < rows; r++){
        for (int c = 0; c < cols; c++){
            int x0 = mx + c*(ww+5);
            int y0 = my + r*(wh+5);
            u32 col = ((r+c)%3 == 0) ? 0xFFFFE080u : 0xFF1A2030u;
            px_rect(t, x0, y0, x0+ww, y0+wh, col);
            for (int x=x0; x<=x0+ww; x++){ px_set(t,x,y0,trim); px_set(t,x,y0+wh,trim); }
            for (int y=y0; y<=y0+wh; y++){ px_set(t,x0,y,trim); px_set(t,x0+ww,y,trim); }
        }
    }
    int dw = W/6, dh = H/6;
    int dx = W/2 - dw/2;
    int dy = H - 5 - dh;
    px_rect(t, dx, dy, dx+dw, H-5, 0xFF3A2010u);
    px_rect(t, dx+2, dy+2, dx+dw-2, H-7, 0xFF5A3A20u);
}

static void tex_draw_road(Tex *t){
    int W = t->w, H = t->h;
    px_rect(t, 0, 0, W-1, H-1, 0xFF303030u);
    px_rect(t, W/2-1, 0, W/2+1, H-1, 0xFFFFCC00u);
    for (int i=0;i<30;i++){
        int x = rng()%W, y = rng()%H;
        px_rect(t, x, y, x+3, y+1, 0xFF1A1A1Au);
    }
}

/* رفع texture على GPU باستخدام tiny3d_TextureOffset */
static void tex_upload(Tex *t){
    /* نجهز pngData-like struct */
    t->png.width   = t->w;
    t->png.height  = t->h;
    t->png.bmp_out = t->pixels;

    /* نستخدم دالة tiny3d الرسمية للـ upload */
    /* tiny3d_TextureOffset بتاخد void* — بنبعت pointer لـ png */
    tiny3d_TextureOffset(&t->png);
    t->ok = 1;
}

static void textures_init(void){
    tex_player    = tex_make(64, 128);
    tex_car_red   = tex_make(128, 64);
    tex_car_blue  = tex_make(128, 64);
    tex_car_yel   = tex_make(128, 64);
    tex_tree      = tex_make(64, 128);
    tex_building  = tex_make(128, 192);
    tex_road      = tex_make(64, 64);
    tex_ped       = tex_make(64, 128);

    tex_draw_person(&tex_player,   0xFF2A7FD0u, 0xFF23252Bu, 0xFFD8A87Cu, 0xFF2A1A0Au);
    tex_draw_person(&tex_ped,      0xFFB03A2Eu, 0xFF1A1A1Au, 0xFFD8A87Cu, 0xFF1A0A0Au);
    tex_draw_car(&tex_car_red,     0xFFC0392Bu, 0xFF304050u);
    tex_draw_car(&tex_car_blue,    0xFF2874A6u, 0xFF304050u);
    tex_draw_car(&tex_car_yel,     0xFFD4AC0Du, 0xFF304050u);
    tex_draw_tree(&tex_tree);
    tex_draw_building(&tex_building, 0xFF8B7355u, 0xFF404040u);
    tex_draw_road(&tex_road);

    tex_upload(&tex_player);
    tex_upload(&tex_ped);
    tex_upload(&tex_car_red);
    tex_upload(&tex_car_blue);
    tex_upload(&tex_car_yel);
    tex_upload(&tex_tree);
    tex_upload(&tex_building);
    tex_upload(&tex_road);
}

/* ===================== 3D Projection ===================== */
typedef struct { float x,y,z; int vis; } SP;

static void project(float wx,float wy,float wz,SP*out){
    float cx = p_x - sinf(cam_yaw)*cam_dist*cosf(cam_pitch);
    float cy = 4.0f + sinf(cam_pitch)*cam_dist;
    float cz = p_z - cosf(cam_yaw)*cam_dist*cosf(cam_pitch);
    cx += (rng()%100 - 50) * 0.01f * g_shake;
    cy += (rng()%100 - 50) * 0.01f * g_shake;

    float dx = wx-cx, dy = wy-cy, dz = wz-cz;
    float cy1 = cosf(-cam_yaw), sy1 = sinf(-cam_yaw);
    float rx = dx*cy1 - dz*sy1;
    float rz = dx*sy1 + dz*cy1;
    float cp = cosf(-cam_pitch), sp = sinf(-cam_pitch);
    float ry = dy*cp - rz*sp;
    float rz2 = dy*sp + rz*cp;
    if (rz2 < 0.5f){ out->vis=0; out->x=0; out->y=0; out->z=0; return; }
    out->x = SCR_W*0.5f + (rx/rz2)*FOV;
    out->y = SCR_H*0.5f - (ry/rz2)*FOV;
    out->z = rz2;
    out->vis = 1;
}

/* ===================== Draw ===================== */
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

/* Texture quad في 3D */
static void draw_tex_face(Tex *t,
                          float x1,float y1,float z1,float u1,float v1,
                          float x2,float y2,float z2,float u2,float v2,
                          float x3,float y3,float z3,float u3,float v3,
                          float x4,float y4,float z4,float u4,float v4,
                          u32 col){
    SP pa,pb,pc,pd;
    project(x1,y1,z1,&pa);
    project(x2,y2,z2,&pb);
    project(x3,y3,z3,&pc);
    project(x4,y4,z4,&pd);
    if (!pa.vis || !pb.vis || !pc.vis || !pd.vis) return;

    if (t && t->ok){
        tiny3d_TextureOffset(&t->png);
        tiny3d_TextureFormat(TINY3D_TEX_FORMAT_A8R8G8B8);
        tiny3d_TextureEnable();

        tiny3d_SetPolygon(TINY3D_QUADS);
        tiny3d_VertexPos(pa.x,pa.y,pa.z);
        tiny3d_VertexColor(col);
        tiny3d_VertexTexture2(u1, v1);

        tiny3d_VertexPos(pb.x,pb.y,pb.z);
        tiny3d_VertexTexture2(u2, v2);

        tiny3d_VertexPos(pc.x,pc.y,pc.z);
        tiny3d_VertexTexture2(u3, v3);

        tiny3d_VertexPos(pd.x,pd.y,pd.z);
        tiny3d_VertexTexture2(u4, v4);
        tiny3d_End();

        tiny3d_TextureDisable();
    } else {
        draw_quad2d(pa.x,pa.y,pb.x,pb.y,pc.x,pc.y,pd.x,pd.y,col);
    }
}

static void draw_billboard(Tex *t, float wx, float wy, float wz,
                            float w, float h, u32 col){
    float fx = wx - p_x, fz = wz - p_z;
    float len = sqrtf(fx*fx + fz*fz);
    if (len < 0.001f){ fx=0; fz=-1; len=1; }
    fx/=len; fz/=len;
    float rx = -fz, rz = fx;
    float hw = w*0.5f;
    float ax = wx - rx*hw, az = wz - rz*hw;
    float bx = wx + rx*hw, bz = wz + rz*hw;
    float y0 = wy, y1 = wy + h;

    draw_tex_face(t,
        ax, y0, az,  0.0f, 1.0f,
        bx, y0, bz,  1.0f, 1.0f,
        bx, y1, bz,  1.0f, 0.0f,
        ax, y1, az,  0.0f, 0.0f, col);
}

/* box بـ texture على الوجه الأمامي فقط */
static void draw_3d_box(float cx,float cy,float cz,float w,float h,float depth,
                        u32 col, Tex *tex){
    float hw=w*0.5f, hd=depth*0.5f;
    float x0=cx-hw, x1=cx+hw, z0=cz-hd, z1=cz+hd;
    float y0=cy, y1=cy+h;
    u32 top  = col;
    u32 side = ((col & 0xFEFEFE) >> 1) | 0xFF000000u;
    u32 dark = ((col & 0xFCFCFC) >> 2) | 0xFF000000u;

    SP pa,pb,pc,pd;

    /* فوق */
    project(x0,y1,z0,&pa); project(x1,y1,z0,&pb);
    project(x1,y1,z1,&pc); project(x0,y1,z1,&pd);
    if (pa.vis&&pb.vis&&pc.vis&&pd.vis)
        draw_quad2d(pa.x,pa.y,pb.x,pb.y,pc.x,pc.y,pd.x,pd.y,top);

    /* خلف */
    project(x0,y0,z1,&pa); project(x1,y0,z1,&pb);
    project(x1,y1,z1,&pc); project(x0,y1,z1,&pd);
    if (pa.vis&&pb.vis&&pc.vis&&pd.vis)
        draw_quad2d(pa.x,pa.y,pb.x,pb.y,pc.x,pc.y,pd.x,pd.y,side);

    /* يمين */
    project(x1,y0,z0,&pa); project(x0,y0,z0,&pb);
    project(x0,y1,z0,&pc); project(x1,y1,z0,&pd);
    if (pa.vis&&pb.vis&&pc.vis&&pd.vis)
        draw_quad2d(pa.x,pa.y,pb.x,pb.y,pc.x,pc.y,pd.x,pd.y,dark);

    /* يسار */
    project(x0,y0,z0,&pa); project(x0,y0,z1,&pb);
    project(x0,y1,z1,&pc); project(x0,y1,z0,&pd);
    if (pa.vis&&pb.vis&&pc.vis&&pd.vis)
        draw_quad2d(pa.x,pa.y,pb.x,pb.y,pc.x,pc.y,pd.x,pd.y,dark);

    /* أمام بـ texture */
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

/* ===================== City gen ===================== */
static void gen_city(void){
    num_bld = 0; num_tree = 0;
    int bI = (int)(p_x/60.0f);
    int bJ = (int)(p_z/60.0f);

    for (int i=-3;i<=3 && num_bld<MAX_BLD-6;i++){
        for (int j=-3;j<=3 && num_bld<MAX_BLD-6;j++){
            int gi=bI+i, gj=bJ+j;
            if (gi==0 && gj==0) continue;

            unsigned int h = ((unsigned int)(gi&0xFFFF)*73856093u) ^
                             ((unsigned int)(gj&0xFFFF)*19349663u);
            if (!h) h=1;

            float cx = (gi+0.5f)*60.0f, cz = (gj+0.5f)*60.0f;

            if ((gi*7 + gj*3) % 5 == 0){
                for (int k=0;k<4 && num_tree<MAX_TREE;k++){
                    t_x[num_tree] = cx + ((k%2)-1)*20.0f;
                    t_z[num_tree] = cz + ((k/2)-1)*20.0f;
                    num_tree++;
                }
                continue;
            }

            int cnt = (h%3)+1;
            for (int k=0;k<cnt && num_bld<MAX_BLD;k++){
                float ox = ((h>>(k*3)) & 0xF) - 7.5f;
                float oz = ((h>>(k*3+4)) & 0xF) - 7.5f;
                float w = 12.0f + ((h>>(k*5)) & 0xF);
                float d = 12.0f + ((h>>(k*5+4)) & 0xF);
                float ht = 8.0f + ((h>>(k*7)) & 0x1F) * 1.8f;
                u32 col;
                switch (h % 6){
                    case 0: col = 0xFF8B6F47u; break;
                    case 1: col = 0xFF6B7280u; break;
                    case 2: col = 0xFF4A5568u; break;
                    case 3: col = 0xFF7B6B8Bu; break;
                    case 4: col = 0xFFAA8B5Fu; break;
                    default: col = 0xFF5A6878u;
                }
                b_x[num_bld]=cx+ox; b_z[num_bld]=cz+oz;
                b_w[num_bld]=w; b_h[num_bld]=ht; b_d[num_bld]=d;
                b_col[num_bld]=col;
                num_bld++;
            }
        }
    }
}

/* ===================== AI ===================== */
static void init_ai(void){
    for (int i=0;i<MAX_AI;i++){
        a_ax[i] = rng() & 1;
        a_dir[i] = (rng() & 1) ? 1 : -1;
        a_spd[i] = 15.0f + (rng() % 20);
        a_tex[i] = i % 3;
        if (a_ax[i]==0){
            a_x[i] = -250.0f + (rng()%500);
            a_z[i] = ((int)(rng()%8)-4)*60.0f + 15.0f*a_dir[i];
        } else {
            a_x[i] = ((int)(rng()%8)-4)*60.0f + 15.0f*a_dir[i];
            a_z[i] = -250.0f + (rng()%500);
        }
    }
}

static void update_ai(float dt){
    for (int i=0;i<MAX_AI;i++){
        if (a_ax[i]==0){
            a_x[i] += a_spd[i]*a_dir[i]*dt;
            if (a_x[i]>p_x+250 || a_x[i]<p_x-250){
                a_x[i] = p_x + (rng()%500) - 250;
                a_z[i] = p_z + ((int)(rng()%8)-4)*60.0f + 15.0f;
            }
        } else {
            a_z[i] += a_spd[i]*a_dir[i]*dt;
            if (a_z[i]>p_z+250 || a_z[i]<p_z-250){
                a_z[i] = p_z + (rng()%500) - 250;
                a_x[i] = p_x + ((int)(rng()%8)-4)*60.0f + 15.0f;
            }
        }
    }
}

/* ===================== Particles ===================== */
static void spawn_particle(float x,float y,float z,float vx,float vy,float vz,u32 col){
    if (num_prt >= MAX_PAR) return;
    prt_x[num_prt]=x; prt_y[num_prt]=y; prt_z[num_prt]=z;
    prt_vx[num_prt]=vx; prt_vy[num_prt]=vy; prt_vz[num_prt]=vz;
    prt_life[num_prt]=45;
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

/* ===================== Collision ===================== */
static int hit_bld(float x,float z,float r){
    for (int i=0;i<num_bld;i++){
        float hw = b_w[i]*0.5f + r;
        float hd = b_d[i]*0.5f + r;
        if (fabsf(x-b_x[i])<hw && fabsf(z-b_z[i])<hd) return 1;
    }
    return 0;
}

/* ===================== Audio ===================== */
static short a_buf[4][1024*2];

static void audio_init(void){
    if (audioInit() != 0) return;
    audioPortParam p; memset(&p,0,sizeof(p));
    p.numChannels = 2;
    p.numBlocks   = 4;
    p.attrib      = 0;
    p.level       = 1.0f;
    if (audioPortOpen(&p, &g_audioPort) != 0) return;
    audioPortStart(g_audioPort);
    g_audioOK = 1;
}

static void audio_shutdown(void){
    if (!g_audioOK) return;
    audioPortStop(g_audioPort);
    audioPortClose(g_audioPort);
    audioQuit();
    g_audioOK = 0;
}

static void audio_fill(short *out){
    float freq, amp;
    float spd = fabsf(p_speed);
    freq = 55.0f + spd*4.5f;
    amp  = 0.05f + spd*0.006f;
    if (amp > 0.35f) amp = 0.35f;

    int boost = ((pad_btns()&BTN_R1) && p_boost > 0.05f);
    float boostAmp = boost ? 0.15f : 0.0f;

    for (int i=0;i<1024;i++){
        g_enginePhase += freq/48000.0f;
        if (g_enginePhase > 1.0f) g_enginePhase -= 1.0f;
        float sq = (g_enginePhase < 0.5f) ? 1.0f : -1.0f;
        float s  = sq * amp;
        if (boostAmp > 0.0f){
            float noise = ((rng() & 0xFFFF)/32768.0f - 1.0f);
            s += noise * boostAmp;
        }
        short s16 = (short)(s * 32000.0f);
        *out++ = s16;
        *out++ = s16;
    }
}

extern int audioPortSend(u32 port, void *buffer, u32 size);

static void audio_pump(void){
    if (!g_audioOK) return;
    static int bi = 0;
    audio_fill(a_buf[bi]);
    if (audioPortSend(g_audioPort, a_buf[bi], 1024*2*sizeof(short)) == 0)
        bi = (bi+1) % 4;
}

/* ===================== Update ===================== */
static void update(float dt){
    if (!g_padReady) return;
    u16 btn = pad_btns();
    int lx = pad_lx(), ly = pad_ly();
    int rx = pad_rx(), ry = pad_ry();

    cam_yaw += (rx/128.0f)*2.6f*dt;
    cam_pitch = clampf(cam_pitch + (ry/128.0f)*1.5f*dt, -0.1f, 1.2f);
    if (btn & BTN_R2) cam_dist += 15.0f*dt;
    if (btn & BTN_L2) cam_dist -= 15.0f*dt;
    cam_dist = clampf(cam_dist, 5.0f, 30.0f);

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
                bx + (rng()%100-50)*0.05f,
                0.5f + (rng()%100)*0.02f,
                bz + (rng()%100-50)*0.05f,
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
    p_speed = clampf(p_speed, -20.0f, 60.0f);

    if (fabsf(p_speed) > 0.5f){
        float sgn = p_speed > 0 ? 1.0f : -1.0f;
        float gain = clampf(fabsf(p_speed)/15.0f, 0.3f, 1.0f);
        p_heading += st * 2.2f * gain * dt * sgn;
    }

    float vx = sinf(p_heading)*p_speed*dt;
    float vz = cosf(p_heading)*p_speed*dt;
    float nx = p_x + vx, nz = p_z + vz;
    if (!hit_bld(nx, p_z, 1.6f)) p_x = nx; else { p_speed *= 0.2f; g_shake = 0.7f; }
    if (!hit_bld(p_x, nz, 1.6f)) p_z = nz; else { p_speed *= 0.2f; g_shake = 0.7f; }
    if (fabsf(p_speed) > 40.0f) g_shake = 0.3f;

    g_shake *= (1.0f - 5.0f*dt);
    if (g_shake < 0.01f) g_shake = 0.0f;

    g_timeOfDay += dt/240.0f;
    if (g_timeOfDay > 1.0f) g_timeOfDay -= 1.0f;

    update_ai(dt);
    update_particles(dt);
}

/* ===================== Render ===================== */
static u32 sky_col(void){
    float sY = sinf((g_timeOfDay - 0.25f) * 6.2831853f);
    float d = sY > 0 ? sY : 0;
    u8 r = (u8)(8 + d*50);
    u8 g = (u8)(18 + d*110);
    u8 b = (u8)(40 + d*180);
    return 0xFF000000u | ((u32)r<<16) | ((u32)g<<8) | b;
}

static void render(void){
    u32 sky = sky_col();
    tiny3d_Clear(sky, 0xFFFFFFFF);

    static float last_x = 1e9f, last_z = 1e9f;
    if (fabsf(p_x-last_x) > 25.0f || fabsf(p_z-last_z) > 25.0f){
        gen_city();
        last_x = p_x; last_z = p_z;
    }

    /* Sky */
    draw_rect2d(0, 0, SCR_W, SCR_H*0.58f, sky);
    float sY = sinf((g_timeOfDay - 0.25f) * 6.2831853f);
    u32 horizon = (sY > 0) ? 0xFFBBCCDDu : 0xFF223344u;
    draw_rect2d(0, SCR_H*0.58f, SCR_W, 6, horizon);

    /* Ground */
    int bI = (int)(p_x/60.0f), bJ = (int)(p_z/60.0f);
    for (int i=-4;i<=4;i++)
        for (int j=-4;j<=4;j++)
            draw_ground_tex((bI+i)*60.0f, (bJ+j)*60.0f, 60.0f, &tex_road);

    /* Buildings sorted */
    int ord[MAX_BLD];
    float dist[MAX_BLD];
    for (int i=0;i<num_bld;i++){
        ord[i] = i;
        float dx = b_x[i]-p_x, dz = b_z[i]-p_z;
        dist[i] = dx*dx + dz*dz;
    }
    for (int i=0;i<num_bld-1;i++)
        for (int j=0;j<num_bld-1-i;j++)
            if (dist[ord[j]] < dist[ord[j+1]]){
                int t=ord[j]; ord[j]=ord[j+1]; ord[j+1]=t;
            }

    for (int k=0;k<num_bld;k++){
        int i = ord[k];
        draw_3d_box(b_x[i], 0, b_z[i], b_w[i], b_h[i], b_d[i], b_col[i], &tex_building);
    }

    for (int i=0;i<num_tree;i++)
        draw_billboard(&tex_tree, t_x[i], 0.0f, t_z[i], 6.0f, 10.0f, 0xFFFFFFFFu);

    for (int i=0;i<MAX_AI;i++){
        Tex *tt = (a_tex[i]==0) ? &tex_car_red :
                  (a_tex[i]==1) ? &tex_car_blue : &tex_car_yel;
        draw_billboard(tt, a_x[i], 0.15f, a_z[i], 5.0f, 2.5f, 0xFFFFFFFFu);
    }

    draw_billboard(&tex_car_red, p_x, 0.15f, p_z, 5.5f, 2.8f, 0xFFFFFFFFu);

    for (int i=0;i<num_prt;i++)
        draw_billboard(&tex_road, prt_x[i], prt_y[i], prt_z[i], 0.4f, 0.4f, prt_col[i]);

    /* HUD */
    SetFontSize(30, 30);
    SetFontColor(0xFF00D4FFu, 0x00000000);
    DrawString(20, 42, "NEON CITY");

    SetFontSize(12, 12);
    SetFontColor(0xFFFF6B35u, 0x00000000);
    DrawString(20, 66, "U L T R A");

    char buf[128];
    sprintf(buf, "%d KM/H", (int)(fabsf(p_speed)*5.4f));
    SetFontSize(28, 28);
    SetFontColor(0xFFFFCC00u, 0x00000000);
    DrawString(20, SCR_H - 100, buf);

    SetFontSize(12, 12);
    SetFontColor(0xFFFF6B35u, 0x00000000);
    DrawString(20, SCR_H - 70, "BOOST");
    draw_rect2d(95, SCR_H - 73, 220, 16, 0xFF333333u);
    draw_rect2d(95, SCR_H - 73, p_boost*220.0f, 16, 0xFFFF6B35u);

    /* Minimap */
    {
        int mx = SCR_W - 200, my = 20, ms = 180;
        draw_rect2d(mx-2, my-2, ms+4, ms+4, 0xFFFFCC00u);
        draw_rect2d(mx, my, ms, ms, 0xBB000818u);

        float sc = (float)ms / 280.0f;
        float cxm = mx + ms*0.5f, cym = my + ms*0.5f;

        for (int i=0;i<num_bld;i++){
            float dx = (b_x[i]-p_x)*sc;
            float dz = (b_z[i]-p_z)*sc;
            if (fabsf(dx) > ms*0.5f || fabsf(dz) > ms*0.5f) continue;
            float hs = b_w[i]*0.5f*sc;
            draw_rect2d(cxm+dx-hs, cym+dz-hs, hs*2, hs*2, 0xFF5A6878u);
        }
        for (int i=0;i<num_tree;i++){
            float dx = (t_x[i]-p_x)*sc;
            float dz = (t_z[i]-p_z)*sc;
            if (fabsf(dx) > ms*0.5f || fabsf(dz) > ms*0.5f) continue;
            draw_rect2d(cxm+dx-2, cym+dz-2, 4, 4, 0xFF2E7A30u);
        }
        for (int i=0;i<MAX_AI;i++){
            float dx = (a_x[i]-p_x)*sc;
            float dz = (a_z[i]-p_z)*sc;
            if (fabsf(dx) > ms*0.5f || fabsf(dz) > ms*0.5f) continue;
            draw_rect2d(cxm+dx-2, cym+dz-2, 4, 4, 0xFFDC7864u);
        }
        draw_rect2d(cxm-4, cym-4, 8, 8, 0xFFFFCC00u);
    }

    /* Clock */
    int hours = (int)((g_timeOfDay*24.0f) + 6.0f) % 24;
    int minutes = (int)((((g_timeOfDay*24.0f) + 6.0f) - (float)hours) * 60.0f);
    sprintf(buf, "%02d:%02d", hours, minutes);
    SetFontSize(20, 20);
    SetFontColor(0xFFFFE080u, 0x00000000);
    DrawString(SCR_W - 130, SCR_H - 40, buf);

    SetFontSize(10, 10);
    SetFontColor(0xFFAAAAAAu, 0x00000000);
    DrawString(20, SCR_H - 20,
        "L-Stick:drive  R-Stick:camera  R1:boost  L1:brake  L2/R2:zoom  SELECT:quit");
}

/* ===================== Main ===================== */
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

    audio_init();
    textures_init();

    p_x = 0.0f; p_z = 0.0f; p_heading = 0.0f;
    gen_city();
    init_ai();

    struct timeval tv; gettimeofday(&tv, NULL);
    rng_s = (unsigned int)tv.tv_usec;

    while (g_running){
        sysUtilCheckCallback();

        g_padReady = 0;
        if (ioPadGetInfo(&g_padInfo) == 0 && g_padInfo.status[0]){
            if (ioPadGetData(0, &g_padData) == 0) g_padReady = 1;
        }

        if (g_padReady && (pad_btns() & BTN_SELECT)) g_running = 0;

        update(1.0f/60.0f);
        audio_pump();
        render();
        tiny3d_Flip();
    }

    audio_shutdown();
    ioPadEnd();
    tiny3d_Exit();
    return 0;
}
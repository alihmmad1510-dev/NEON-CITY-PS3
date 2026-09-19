/* =====================================================================
 *   NEON CITY ULTRA v2 - PS3 Homebrew
 *   Buildings + Villa + Arena + Cars + Shops + Mall
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

/* Weak stubs */
__attribute__((weak)) void tiny3d_TextureFormat(int f){ (void)f; }
__attribute__((weak)) void tiny3d_TextureEnable(void){ }
__attribute__((weak)) void tiny3d_TextureDisable(void){ }
#ifndef TINY3D_TEX_FORMAT_A8R8G8B8
#define TINY3D_TEX_FORMAT_A8R8G8B8  0x85
#endif

/* ===================== Config ===================== */
#define SCR_W 1280
#define SCR_H 720
#define FOV   620.0f

#define MAX_OBJ  400
#define MAX_AI   20
#define MAX_PAR  100

/* Building types */
#define BT_TOWER   0
#define BT_HOUSE   1
#define BT_VILLA   2
#define BT_SHOP    3
#define BT_MALL    4
#define BT_ARENA   5
#define BT_TREE    6
#define BT_PARK    7
#define BT_GAS     8

/* ===================== Textures ===================== */
typedef struct { u16 width; u16 height; u32 *bmp_out; } LocalPngData;
typedef struct { LocalPngData png; u16 w, h; int ok; u32 *pixels; } Tex;

static Tex tex_player;
static Tex tex_car_red, tex_car_blue, tex_car_yel, tex_car_grn, tex_car_prp;
static Tex tex_tower, tex_house, tex_villa, tex_shop, tex_mall, tex_arena;
static Tex tex_tree, tex_road;

/* ===================== State ===================== */
static int      g_running = 1;
static padInfo  g_padInfo;
static padData  g_padData;
static int      g_padReady = 0;
static u8       g_prevL3 = 0, g_prevR3 = 0;

static float p_x = 0, p_z = 0, p_heading = 0, p_speed = 0, p_boost = 1.0f;
static float cam_yaw = 0, cam_pitch = 0.35f, cam_dist = 13.0f;
static float g_timeOfDay = 0.20f;
static float g_shake = 0.0f;
static int   g_carTex = 0;   /* 0=red, 1=blue, 2=yellow, 3=green, 4=purple */
static int   g_score = 0;    /* نقاط من الجولات */
static float g_msgTimer = 0.0f;
static char  g_msg[64] = "";

/* Objects */
static float o_x[MAX_OBJ], o_z[MAX_OBJ], o_w[MAX_OBJ], o_h[MAX_OBJ], o_d[MAX_OBJ];
static int   o_type[MAX_OBJ];
static int   o_tex[MAX_OBJ];
static int   num_obj = 0;

/* AI */
static float a_x[MAX_AI], a_z[MAX_AI], a_spd[MAX_AI];
static int   a_ax[MAX_AI], a_dir[MAX_AI], a_tex[MAX_AI];

/* Particles */
static float prt_x[MAX_PAR], prt_y[MAX_PAR], prt_z[MAX_PAR];
static float prt_vx[MAX_PAR], prt_vy[MAX_PAR], prt_vz[MAX_PAR];
static int   prt_life[MAX_PAR];
static u32   prt_col[MAX_PAR];
static int   num_prt = 0;

/* ===================== Pad ===================== */
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

/* =====================================================================
 *  TEXTURE DRAWING
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

/* ---- Person ---- */
static void draw_person(Tex *t, u32 shirt, u32 pants, u32 skin, u32 hair){
    int W=t->w, H=t->h, cx=W/2;
    int headCY=H/8, headR=W/6;
    px_circle(t, cx, headCY, headR, skin);
    for (int y=headCY-headR; y<headCY-headR/3; y++)
        for (int x=cx-headR; x<=cx+headR; x++)
            if ((x-cx)*(x-cx)+(y-headCY)*(y-headCY)<=headR*headR)
                px_set(t,x,y,hair);
    px_rect(t, cx-headR/2, headCY, cx-headR/2+1, headCY+1, 0xFF000000u);
    px_rect(t, cx+headR/2-1, headCY, cx+headR/2, headCY+1, 0xFF000000u);
    px_rect(t, cx-2, headCY+headR/2, cx+2, headCY+headR/2, 0xFF8B3030u);
    int bTop=headCY+headR+2, bBot=H*3/5, bW=W/5;
    px_rect(t, cx-bW, bTop, cx+bW, bBot, shirt);
    px_rect(t, cx-bW+1, bTop, cx+bW-1, bTop+1, 0xFFDDDDDDu);
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

/* ---- Car (side) ---- */
static void draw_car(Tex *t, u32 body, u32 glass){
    int W=t->w, H=t->h;
    px_rect(t, 2, H/2, W-3, H*3/4, body);
    px_rect(t, W/5, H/3, W*4/5, H/2, body);
    px_rect(t, W/5+3, H/3+3, W/2-2, H/2-3, glass);
    px_rect(t, W/2+2, H/3+3, W*4/5-3, H/2-3, glass);
    px_rect(t, W/2-1, H/3+3, W/2+1, H/2-3, body);
    px_rect(t, 3, H/2-1, W-4, H/2, 0xFFFFFFFFu);
    px_rect(t, 3, H/2+3, W/7, H/2+7, 0xFFFFF8C0u);
    px_rect(t, W-W/7, H/2+3, W-4, H/2+7, 0xFFFF3020u);
    px_circle(t, W/5, H*3/4-1, 6, 0xFF0A0A0Au);
    px_circle(t, W/5, H*3/4-1, 3, 0xFF808890u);
    px_circle(t, W*4/5, H*3/4-1, 6, 0xFF0A0A0Au);
    px_circle(t, W*4/5, H*3/4-1, 3, 0xFF808890u);
}

/* ---- Tower (skyscraper) ---- */
static void draw_tower(Tex *t){
    int W=t->w, H=t->h;
    px_rect(t, 0, 0, W-1, H-1, 0xFF2A2A38u);
    /* شبكة نوافذ */
    for (int r=0; r<20; r++){
        for (int c=0; c<6; c++){
            int x0 = 4 + c*(W-8)/6;
            int y0 = 4 + r*(H-8)/20;
            int x1 = x0 + (W-8)/6 - 3;
            int y1 = y0 + (H-8)/20 - 3;
            u32 col = ((r*7+c*3)%5 == 0) ? 0xFFFFEE88u : 0xFF1A2030u;
            px_rect(t, x0, y0, x1, y1, col);
        }
    }
}

/* ---- House ---- */
static void draw_house(Tex *t){
    int W=t->w, H=t->h;
    px_rect(t, 0, 0, W-1, H-1, 0xFFA05A3Au);
    /* سقف مثلث */
    for (int y=0; y<H/4; y++){
        int w = (y*W)/(H/4);
        int x0 = (W-w)/2;
        px_rect(t, x0, y, x0+w, y, 0xFF5A2A1Au);
    }
    /* جدار */
    px_rect(t, 6, H/4, W-7, H-7, 0xFFDDBB88u);
    /* نوافذ */
    px_rect(t, 10, H/3, W/3, H*2/3, 0xFFCCE0F0u);
    px_rect(t, W*2/3, H/3, W-10, H*2/3, 0xFFCCE0F0u);
    /* باب */
    px_rect(t, W/2-6, H*2/3, W/2+6, H-7, 0xFF6A3A20u);
}

/* ---- Villa ---- */
static void draw_villa(Tex *t){
    int W=t->w, H=t->h;
    px_rect(t, 0, 0, W-1, H-1, 0xFFF0EAD8u);
    /* سقف مسطح */
    px_rect(t, 0, 0, W-1, 6, 0xFF303038u);
    /* جدار فاتح */
    px_rect(t, 3, 6, W-4, H-3, 0xFFEFE8D8u);
    /* نوافذ كبيرة */
    for (int c=0; c<4; c++){
        int x0 = 8 + c*(W-16)/4;
        int x1 = x0 + (W-16)/4 - 5;
        px_rect(t, x0, 18, x1, H*2/3, 0xFF9EC9E8u);
    }
    /* باب */
    px_rect(t, W/2-8, H*2/3, W/2+8, H-4, 0xFF6A4A30u);
    /* سياج أمامي */
    px_rect(t, 0, H-6, W-1, H-1, 0xFF3A6A32u);
}

/* ---- Shop (small colored) ---- */
static void draw_shop(Tex *t, u32 wall){
    int W=t->w, H=t->h;
    px_rect(t, 0, 0, W-1, H-1, wall);
    /* سقف داكن */
    px_rect(t, 0, 0, W-1, 8, 0xFF202020u);
    /* لافتة */
    px_rect(t, 4, 12, W-5, 24, 0xFFFFE080u);
    /* واجهة زجاجية */
    px_rect(t, 6, 30, W-7, H-3, 0xFF9EC9E8u);
    /* إطار */
    for (int x=6; x<=W-7; x++){ px_set(t,x,30,0xFF404040u); px_set(t,x,H-3,0xFF404040u); }
    for (int y=30; y<=H-3; y++){ px_set(t,6,y,0xFF404040u); px_set(t,W-7,y,0xFF404040u); }
    /* عمود في النص */
    px_rect(t, W/2-1, 30, W/2+1, H-3, 0xFF404040u);
}

/* ---- Mall (big) ---- */
static void draw_mall(Tex *t){
    int W=t->w, H=t->h;
    px_rect(t, 0, 0, W-1, H-1, 0xFF3A3A4Au);
    /* سقف */
    px_rect(t, 0, 0, W-1, 10, 0xFF202028u);
    /* لافتة كبيرة */
    px_rect(t, 20, 14, W-20, 30, 0xFFFF33AAu);
    /* صفوف النوافذ */
    for (int r=0; r<3; r++){
        for (int c=0; c<5; c++){
            int x0 = 6 + c*(W-12)/5;
            int y0 = 40 + r*(H-50)/3;
            int x1 = x0 + (W-12)/5 - 4;
            int y1 = y0 + (H-50)/3 - 4;
            px_rect(t, x0, y0, x1, y1, 0xFFA0D0F0u);
        }
    }
    /* باب كبير */
    px_rect(t, W/2-15, H-40, W/2+15, H-2, 0xFF303030u);
    px_rect(t, W/2-12, H-37, W/2-1, H-5, 0xFF90C0E0u);
    px_rect(t, W/2+1, H-37, W/2+12, H-5, 0xFF90C0E0u);
}

/* ---- Arena (stadium) ---- */
static void draw_arena(Tex *t){
    int W=t->w, H=t->h;
    int cx=W/2, cy=H/2;
    /* ملعب بيضاوي */
    px_rect(t, 0, 0, W-1, H-1, 0xFF2A3A4Au);
    /* مدرجات خارجية */
    for (int r=W/2; r>W/3; r--){
        for (int y=-r; y<=r; y++){
            for (int x=-r; x<=r; x++){
                int d2 = x*x + y*y;
                if (d2 <= r*r && d2 > (r-2)*(r-2)){
                    u32 c = ((y+100)%8<4) ? 0xFF505060u : 0xFF404048u;
                    px_set(t, cx+x, cy+y, c);
                }
            }
        }
    }
    /* العشب الداخلي */
    for (int y=-W/3; y<=W/3; y++)
        for (int x=-W/3; x<=W/3; x++)
            if (x*x + y*y <= (W/3)*(W/3))
                px_set(t, cx+x, cy+y, 0xFF2A8A30u);
    /* خط الوسط */
    for (int y=-W/3; y<=W/3; y++)
        px_set(t, cx, cy+y, 0xFFFFFFFFu);
}

/* ---- Tree ---- */
static void draw_tree(Tex *t){
    int W=t->w, H=t->h, cx=W/2;
    px_rect(t, cx-3, H*2/3, cx+3, H-2, 0xFF4A2E1Au);
    for (int y=H*2/3; y<H-2; y+=5) px_set(t, cx-1, y, 0xFF3A2010u);
    px_circle(t, cx, H/4, W/3, 0xFF1A5A20u);
    px_circle(t, cx-8, H/3, W/4, 0xFF2E7A30u);
    px_circle(t, cx+8, H/3, W/4, 0xFF2E7A30u);
    px_circle(t, cx, H/5, W/4, 0xFF4AA050u);
    px_circle(t, cx-4, H/3+2, W/5, 0xFF4AA050u);
    px_circle(t, cx+4, H/3+2, W/5, 0xFF4AA050u);
}

/* ---- Road ---- */
static void draw_road(Tex *t){
    int W=t->w, H=t->h;
    px_rect(t, 0, 0, W-1, H-1, 0xFF303030u);
    px_rect(t, W/2-1, 0, W/2+1, H-1, 0xFFFFCC00u);
    for (int i=0;i<30;i++){
        int x=rng()%W, y=rng()%H;
        px_rect(t, x, y, x+3, y+1, 0xFF1A1A1Au);
    }
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
    tex_house    = tex_make(96, 96);
    tex_villa    = tex_make(160, 96);
    tex_shop     = tex_make(96, 96);
    tex_mall     = tex_make(200, 128);
    tex_arena    = tex_make(256, 256);
    tex_tree     = tex_make(64, 128);
    tex_road     = tex_make(64, 64);

    draw_person(&tex_player, 0xFF2A7FD0u, 0xFF23252Bu, 0xFFD8A87Cu, 0xFF2A1A0Au);
    draw_car(&tex_car_red,  0xFFC0392Bu, 0xFF304050u);
    draw_car(&tex_car_blue, 0xFF2874A6u, 0xFF304050u);
    draw_car(&tex_car_yel,  0xFFD4AC0Du, 0xFF304050u);
    draw_car(&tex_car_grn,  0xFF27AE60u, 0xFF304050u);
    draw_car(&tex_car_prp,  0xFF8E44ADu, 0xFF304050u);
    draw_tower(&tex_tower);
    draw_house(&tex_house);
    draw_villa(&tex_villa);
    draw_shop(&tex_shop, 0xFFEFEFEFu);
    draw_mall(&tex_mall);
    draw_arena(&tex_arena);
    draw_tree(&tex_tree);
    draw_road(&tex_road);

    tex_upload(&tex_player);
    tex_upload(&tex_car_red);
    tex_upload(&tex_car_blue);
    tex_upload(&tex_car_yel);
    tex_upload(&tex_car_grn);
    tex_upload(&tex_car_prp);
    tex_upload(&tex_tower);
    tex_upload(&tex_house);
    tex_upload(&tex_villa);
    tex_upload(&tex_shop);
    tex_upload(&tex_mall);
    tex_upload(&tex_arena);
    tex_upload(&tex_tree);
    tex_upload(&tex_road);
}

/* ===================== 3D projection ===================== */
typedef struct { float x,y,z; int vis; } SP;

static void project(float wx,float wy,float wz,SP*out){
    float cx = p_x - sinf(cam_yaw)*cam_dist*cosf(cam_pitch);
    float cy = 4.0f + sinf(cam_pitch)*cam_dist;
    float cz = p_z - cosf(cam_yaw)*cam_dist*cosf(cam_pitch);
    cx += (rng()%100 - 50) * 0.01f * g_shake;
    cy += (rng()%100 - 50) * 0.01f * g_shake;
    float dx=wx-cx, dy=wy-cy, dz=wz-cz;
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

/* box بـ texture على الوجه الأمامي + لون على الباقي */
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

/* ===================== City generation ===================== */
static void add_obj(int type, float x, float z, float w, float h, float d){
    if (num_obj >= MAX_OBJ) return;
    o_x[num_obj] = x; o_z[num_obj] = z;
    o_w[num_obj] = w; o_h[num_obj] = h; o_d[num_obj] = d;
    o_type[num_obj] = type;
    o_tex[num_obj] = type;
    num_obj++;
}

static void gen_city(void){
    num_obj = 0;
    int bI = (int)(p_x/60.0f);
    int bJ = (int)(p_z/60.0f);

    /* الحي الرئيسي: فيلا + حدائق حول نقطة البداية */
    for (int i=-4;i<=4;i++){
        for (int j=-4;j<=4;j++){
            int gi=bI+i, gj=bJ+j;
            float cx=(gi+0.5f)*60.0f, cz=(gj+0.5f)*60.0f;

            unsigned int h = ((unsigned int)(gi&0xFFFF)*73856093u) ^
                             ((unsigned int)(gj&0xFFFF)*19349663u);
            if (!h) h=1;

            /* الأصل: فيلا اللاعب */
            if (gi==0 && gj==0){
                add_obj(BT_VILLA, cx, cz, 20.0f, 10.0f, 16.0f);
                /* حدائق جانبية */
                add_obj(BT_TREE, cx-20, cz-15, 8, 12, 8);
                add_obj(BT_TREE, cx+20, cz-15, 8, 12, 8);
                add_obj(BT_TREE, cx-20, cz+15, 8, 12, 8);
                add_obj(BT_TREE, cx+20, cz+15, 8, 12, 8);
                continue;
            }
            /* أرينا كبير على بعد 3 بلوكات */
            if (gi==3 && gj==0){
                add_obj(BT_ARENA, cx, cz, 40, 15, 40);
                continue;
            }
            /* مول على الشمال */
            if (gi==0 && gj==3){
                add_obj(BT_MALL, cx, cz, 40, 22, 28);
                continue;
            }
            /* محطة بنزين */
            if (gi==-2 && gj==-2){
                add_obj(BT_GAS, cx, cz, 20, 6, 14);
                continue;
            }
            /* حدائق */
            if ((gi*3 + gj*5) % 7 == 0){
                for (int k=0;k<4;k++){
                    float tx=cx+((k%2)-1)*15.0f;
                    float tz=cz+((k/2)-1)*15.0f;
                    add_obj(BT_TREE, tx, tz, 6, 12, 6);
                }
                continue;
            }

            int typ = h % 6;
            if (typ == 0 || typ == 1){
                add_obj(BT_TOWER, cx, cz, 16+(h%8), 30+(h%40), 16+(h%8));
            } else if (typ == 2 || typ == 3){
                /* صف محلات */
                for (int k=0;k<3;k++){
                    float sx = cx + (k-1)*15.0f;
                    add_obj(BT_SHOP, sx, cz, 10, 8, 12);
                }
            } else if (typ == 4){
                add_obj(BT_HOUSE, cx, cz, 14, 8, 14);
            } else {
                add_obj(BT_HOUSE, cx-8, cz-8, 12, 8, 12);
                add_obj(BT_HOUSE, cx+8, cz+8, 12, 8, 12);
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
        case BT_GAS:   return &tex_shop;
    }
    return &tex_tower;
}

/* ===================== AI ===================== */
static void init_ai(void){
    for (int i=0;i<MAX_AI;i++){
        a_ax[i] = rng() & 1;
        a_dir[i] = (rng() & 1) ? 1 : -1;
        a_spd[i] = 15.0f + (rng() % 20);
        a_tex[i] = i % 5;
        if (a_ax[i]==0){
            a_x[i] = -250.0f + (rng()%500);
            a_z[i] = ((int)(rng()%10)-5)*60.0f + 15.0f*a_dir[i];
        } else {
            a_x[i] = ((int)(rng()%10)-5)*60.0f + 15.0f*a_dir[i];
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
                a_z[i] = p_z + ((int)(rng()%10)-5)*60.0f + 15.0f;
            }
        } else {
            a_z[i] += a_spd[i]*a_dir[i]*dt;
            if (a_z[i]>p_z+250 || a_z[i]<p_z-250){
                a_z[i] = p_z + (rng()%500) - 250;
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
static int hit_obj(float x,float z,float r, int* out_type, float* out_ox, float* out_oz){
    for (int i=0;i<num_obj;i++){
        if (o_type[i] == BT_TREE) continue; /* الأشجار مش صعبة */
        float hw = o_w[i]*0.5f + r;
        float hd = o_d[i]*0.5f + r;
        if (fabsf(x-o_x[i])<hw && fabsf(z-o_z[i])<hd){
            if (out_type) *out_type = o_type[i];
            if (out_ox) *out_ox = o_x[i];
            if (out_oz) *out_oz = o_z[i];
            return 1;
        }
    }
    return 0;
}

/* ===================== Update ===================== */
static void show_msg(const char* m){
    strncpy(g_msg, m, 63); g_msg[63]=0;
    g_msgTimer = 2.0f;
}

static void update(float dt){
    if (!g_padReady) return;
    u16 btn = pad_btns();
    int lx = pad_lx(), ly = pad_ly();
    int rx = pad_rx(), ry = pad_ry();

    /* camera */
    cam_yaw += (rx/128.0f)*2.6f*dt;
    cam_pitch = clampf(cam_pitch + (ry/128.0f)*1.5f*dt, -0.1f, 1.2f);
    if (btn & BTN_R2) cam_dist += 15.0f*dt;
    if (btn & BTN_L2) cam_dist -= 15.0f*dt;
    cam_dist = clampf(cam_dist, 5.0f, 32.0f);

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

    /* R3: reset position */
    u8 r3 = (btn & BTN_R3) ? 1 : 0;
    if (r3 && !g_prevR3){
        p_x = 0; p_z = 0; p_speed = 0; p_heading = 0;
        show_msg("Teleport to Villa");
    }
    g_prevR3 = r3;

    /* speed */
    float thr = -(ly/128.0f);
    float st  = -(lx/128.0f);
    if (btn & BTN_UP)    thr = 1.0f;
    if (btn & BTN_DOWN)  thr = -1.0f;
    if (btn & BTN_LEFT)  st = 1.0f;
    if (btn & BTN_RIGHT) st = -1.0f;

    /* boost */
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
    float t_ox = 0, t_oz = 0;

    if (!hit_obj(nx, p_z, 1.6f, &t_type, &t_ox, &t_oz)) p_x = nx;
    else {
        p_speed *= 0.15f; g_shake = 0.8f;
        if (t_type == BT_SHOP || t_type == BT_MALL){
            g_score += 10; show_msg("Shopped! +10");
        } else if (t_type == BT_GAS){
            p_boost = 1.0f; show_msg("Refueled!");
        }
    }
    if (!hit_obj(p_x, nz, 1.6f, &t_type, &t_ox, &t_oz)) p_z = nz;
    else {
        p_speed *= 0.15f; g_shake = 0.8f;
        if (t_type == BT_SHOP || t_type == BT_MALL){
            g_score += 10; show_msg("Shopped! +10");
        } else if (t_type == BT_GAS){
            p_boost = 1.0f; show_msg("Refueled!");
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
    if (fabsf(p_x-last_x) > 20.0f || fabsf(p_z-last_z) > 20.0f){
        gen_city();
        last_x = p_x; last_z = p_z;
    }

    draw_rect2d(0, 0, SCR_W, SCR_H*0.58f, sky);
    float sY = sinf((g_timeOfDay - 0.25f) * 6.2831853f);
    u32 horizon = (sY > 0) ? 0xFFBBCCDDu : 0xFF223344u;
    draw_rect2d(0, SCR_H*0.58f, SCR_W, 6, horizon);

    int bI = (int)(p_x/60.0f), bJ = (int)(p_z/60.0f);
    for (int i=-4;i<=4;i++)
        for (int j=-4;j<=4;j++)
            draw_ground_tex((bI+i)*60.0f, (bJ+j)*60.0f, 60.0f, &tex_road);

    /* Sort by distance */
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
            draw_billboard(tt, o_x[i], 0, o_z[i], o_w[i], o_h[i], 0xFFFFFFFFu);
        } else {
            u32 bc = 0xFFFFFFFFu;
            if (o_type[i] == BT_TOWER) bc = 0xFF404050u;
            else if (o_type[i] == BT_SHOP) bc = 0xFFEFEFEFu;
            else if (o_type[i] == BT_MALL) bc = 0xFF3A3A4Au;
            else if (o_type[i] == BT_ARENA) bc = 0xFF2A3A4Au;
            draw_3d_box(o_x[i], 0, o_z[i], o_w[i], o_h[i], o_d[i], bc, tt);
        }
    }

    /* AI cars */
    for (int i=0;i<MAX_AI;i++){
        Tex *tt = ai_tex(a_tex[i]);
        draw_billboard(tt, a_x[i], 0.15f, a_z[i], 5.0f, 2.5f, 0xFFFFFFFFu);
    }

    /* Player car */
    Tex *pcar = &tex_car_red;
    switch(g_carTex){
        case 1: pcar = &tex_car_blue; break;
        case 2: pcar = &tex_car_yel;  break;
        case 3: pcar = &tex_car_grn;  break;
        case 4: pcar = &tex_car_prp;  break;
    }
    draw_billboard(pcar, p_x, 0.15f, p_z, 5.5f, 2.8f, 0xFFFFFFFFu);

    /* Particles */
    for (int i=0;i<num_prt;i++)
        draw_billboard(&tex_road, prt_x[i], prt_y[i], prt_z[i], 0.4f, 0.4f, prt_col[i]);

    /* ============= HUD ============= */
    SetFontSize(30, 30);
    SetFontColor(0xFF00D4FFu, 0x00000000);
    DrawString(20, 42, "NEON CITY");

    SetFontSize(12, 12);
    SetFontColor(0xFFFF6B35u, 0x00000000);
    DrawString(20, 66, "U L T R A   v2");

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

    /* Score */
    SetFontSize(16, 16);
    SetFontColor(0xFF2ECC71u, 0x00000000);
    sprintf(buf, "SCORE: %d", g_score);
    DrawString(20, SCR_H - 130, buf);

    /* Message */
    if (g_msgTimer > 0.0f){
        SetFontSize(20, 20);
        SetFontColor(0xFFFFFFFFu, 0x80000000u);
        int w = (int)strlen(g_msg)*10;
        DrawString(SCR_W/2 - w/2, 130, g_msg);
    }

    /* Minimap */
    {
        int mx = SCR_W - 200, my = 20, ms = 180;
        draw_rect2d(mx-2, my-2, ms+4, ms+4, 0xFFFFCC00u);
        draw_rect2d(mx, my, ms, ms, 0xBB000818u);
        float sc = (float)ms / 300.0f;
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
        "L:drive  R:cam  R1:nitro  L1:brake  L3:color  R3:reset  SEL:quit");
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
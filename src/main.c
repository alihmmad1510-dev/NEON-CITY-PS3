/* =====================================================================
 *   NEON CITY - PS3 Homebrew  [PROCEDURAL TEXTURES + 3D 360]
 *   كل الصور مرسومة بالكود - مش محتاج أي ملف خارجي
 * ===================================================================== */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
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

/* ===================== CONFIG ===================== */
#define SCR_W 1280
#define SCR_H  720
#define CELL 60.0f
#define RENDER_R 2
#define CURB_H  0.35f

#define HOME_CX  -30.0f
#define HOME_CZ   30.0f
#define MAX_BUILDINGS 96
#define MAX_AICARS    10
#define MAX_PEDS      30
#define MAX_COLLIDERS 200

#define DAY_DURATION 240.0f
#define WALK_SPD  14.0f
#define RUN_SPD   26.0f
#define GRAVITY  -32.0f
#define JUMP_V    11.0f
#define PLAYER_R   0.55f
#define CAR_R       1.5f

#define CAR_MAX_SPEED 70.0f
#define CAR_REV_SPEED 25.0f
#define CAR_ACCEL     38.0f
#define CAR_TURN       1.8f
#define BOOST_REGEN    0.35f
#define BOOST_DRAIN    0.8f

#define SAVE_PATH  "/dev_hdd0/tmp/neoncity_save.dat"
#define SAVE_MAGIC 0x4E454F43u

/* ===================== TYPES ===================== */
typedef struct { float x,y,z,w,h,d; u32 color; int type; } Building;
typedef struct { float minX,maxX,minZ,maxZ; } Collider;
typedef struct { float x,z,heading,speed; int axis,dir; u32 color; float minP,maxP; } AiCar;
typedef struct { float sx,sz,ex,ez,t,speed,phase; u32 color; } Ped;
typedef struct { float x,y,z,heading,speed; int inCar,grounded; float yVel; } Player;

typedef struct {
    u32 magic, version;
    float px,py,pz,pheading; int outfit;
    float cx,cy,cz,cheading; float timeOfDay;
    u32 checksum;
} SaveData;

/* ===================== TEXTURE SYSTEM ===================== */
typedef struct {
    pngData png;
    u32 offset;
    int loaded;
    int width, height;
    u32 *pixels;
} Texture;

/* رسم بكسل واحد */
static inline void px_set(Texture *t, int x, int y, u32 col) {
    if (x < 0 || y < 0 || x >= t->width || y >= t->height) return;
    t->pixels[y * t->width + x] = col;
}

/* رسم مستطيل */
static void px_rect(Texture *t, int x0, int y0, int x1, int y1, u32 col) {
    for (int y = y0; y <= y1; y++)
        for (int x = x0; x <= x1; x++)
            px_set(t, x, y, col);
}

/* رسم دائرة (تقريبية) */
static void px_circle(Texture *t, int cx, int cy, int r, u32 col) {
    for (int y = -r; y <= r; y++)
        for (int x = -r; x <= r; x++)
            if (x*x + y*y <= r*r)
                px_set(t, cx+x, cy+y, col);
}

/* ============= رسم شخصية ============= */
static void draw_sprite_person(Texture *t, u32 shirt, u32 pants, u32 skin,
                                u32 hair, int facing_right, int walking) {
    int W = t->width, H = t->height;
    int cx = W/2;
    int footY = H - 4;

    /* رأس (بيضاوي) */
    int headCY = H/6 + 2;
    int headR  = W/7;
    px_circle(t, cx, headCY, headR, skin);

    /* شعر */
    for (int y = headCY - headR; y <= headCY - headR/2; y++)
        for (int x = cx - headR; x <= cx + headR; x++)
            if ((x-cx)*(x-cx) + (y-headCY)*(y-headCY) <= headR*headR)
                px_set(t, x, y, hair);
    /* حواجب جانبية */
    px_rect(t, cx-headR, headCY-headR/2, cx-headR/2, headCY-headR/3, hair);
    px_rect(t, cx+headR/2, headCY-headR/2, cx+headR, headCY-headR/3, hair);

    /* عيون */
    int eyeY = headCY + 1;
    px_rect(t, cx - headR/2, eyeY, cx - headR/2 + 1, eyeY + 1, 0xFF000000u);
    px_rect(t, cx + headR/2 - 1, eyeY, cx + headR/2, eyeY + 1, 0xFF000000u);

    /* فم */
    if (facing_right)
        px_rect(t, cx - 1, headCY + headR/2 + 1, cx + 2, headCY + headR/2 + 1, 0xFF8B3030u);
    else
        px_rect(t, cx - 2, headCY + headR/2 + 1, cx + 1, headCY + headR/2 + 1, 0xFF8B3030u);

    /* رقبة */
    px_rect(t, cx - 2, headCY + headR - 1, cx + 2, headCY + headR + 2, skin);

    /* جسم (قميص) */
    int bodyTop = headCY + headR + 3;
    int bodyBot = footY - (H/3);
    int bodyW   = W/5;
    px_rect(t, cx - bodyW, bodyTop, cx + bodyW, bodyBot, shirt);

    /* زر القميص */
    for (int y = bodyTop + 3; y < bodyBot; y += 6)
        px_set(t, cx, y, 0xFF303030u);

    /* ياقة */
    px_rect(t, cx - bodyW + 1, bodyTop, cx + bodyW - 1, bodyTop + 1, 0xFFE0E0E0u);

    /* ذراع يسار */
    int armY0 = bodyTop + 2;
    int armY1 = armY0 + (walking && facing_right ? 18 : 15);
    px_rect(t, cx - bodyW - 3, armY0, cx - bodyW - 1, armY1, shirt);
    px_rect(t, cx - bodyW - 3, armY1, cx - bodyW - 1, armY1 + 2, skin);

    /* ذراع يمين */
    px_rect(t, cx + bodyW + 1, armY0, cx + bodyW + 3, armY1, shirt);
    px_rect(t, cx + bodyW + 1, armY1, cx + bodyW + 3, armY1 + 2, skin);

    /* رجلين */
    int legTop = bodyBot + 1;
    int legW   = W/8;
    int legOff = 3;

    /* لو ماشي، حرّك الرجلين */
    int leftLegShift  = walking ? -2 : 0;
    int rightLegShift = walking ?  2 : 0;

    px_rect(t, cx - legOff - legW + leftLegShift, legTop,
               cx - legOff + leftLegShift,          footY, pants);
    px_rect(t, cx + legOff + rightLegShift,        legTop,
               cx + legOff + legW + rightLegShift,  footY, pants);

    /* أحذية */
    int shoeColor = 0xFF1A1A1Au;
    px_rect(t, cx - legOff - legW - 1 + leftLegShift,  footY - 1,
               cx - legOff + 1 + leftLegShift,         footY + 2, shoeColor);
    px_rect(t, cx + legOff - 1 + rightLegShift,        footY - 1,
               cx + legOff + legW + 1 + rightLegShift, footY + 2, shoeColor);
}

/* ============= رسم عربية ============= */
static void draw_sprite_car(Texture *t, u32 body_col, u32 accent) {
    int W = t->width, H = t->height;

    /* الهيكل السفلي */
    px_rect(t, 2, H/2, W-3, H*3/4, body_col);

    /* الكابينة (فوق) */
    px_rect(t, W/5, H/3, W*4/5, H/2, body_col);

    /* خط التمييز */
    px_rect(t, W/5, H/2 - 2, W*4/5, H/2, accent);

    /* النوافذ الأمامية */
    u32 winCol = 0xFF1E2A3Au;
    px_rect(t, W/5 + 3, H/3 + 3, W/2 - 2, H/2 - 3, winCol);
    px_rect(t, W/2 + 2, H/3 + 3, W*4/5 - 3, H/2 - 3, winCol);

    /* عمود بين الشبابيك */
    px_rect(t, W/2 - 1, H/3 + 3, W/2 + 1, H/2 - 3, body_col);

    /* مصابيح أمامية */
    px_rect(t, 3, H/2 + 2, W/6, H/2 + 6, 0xFFFFF8C0u);
    px_rect(t, W - W/6, H/2 + 2, W - 4, H/2 + 6, 0xFFFFF8C0u);

    /* شبكة أمامية */
    px_rect(t, W/4, H/2 + 4, W*3/4, H/2 + 7, 0xFF0A0A0Au);
    for (int x = W/4 + 2; x < W*3/4; x += 3)
        px_set(t, x, H/2 + 5, 0xFF404040u);

    /* العجلات */
    u32 wheelCol = 0xFF0A0A0Au;
    u32 rimCol   = 0xFF808890u;
    px_circle(t, W/5,     H*3/4 - 1, 5, wheelCol);
    px_circle(t, W/5,     H*3/4 - 1, 2, rimCol);
    px_circle(t, W*4/5,   H*3/4 - 1, 5, wheelCol);
    px_circle(t, W*4/5,   H*3/4 - 1, 2, rimCol);

    /* ظل أسفل */
    px_rect(t, 2, H*3/4 + 3, W-3, H*3/4 + 5, 0x40000000u);
}

/* ============= رسم شجرة ============= */
static void draw_sprite_tree(Texture *t) {
    int W = t->width, H = t->height;
    int cx = W/2;

    /* الجزع */
    px_rect(t, cx-3, H*2/3, cx+3, H-2, 0xFF4A2E1Au);
    /* خطوط الجزع */
    for (int y = H*2/3; y < H-2; y += 4)
        px_set(t, cx-1, y, 0xFF3A2010u);

    /* الأوراق (دوائر متداخلة) */
    u32 leafDark  = 0xFF1A5A20u;
    u32 leafMid   = 0xFF2E7A30u;
    u32 leafLight = 0xFF4AA050u;

    px_circle(t, cx,     H/4,      W/3, leafDark);
    px_circle(t, cx-6,   H/3,      W/4, leafMid);
    px_circle(t, cx+6,   H/3,      W/4, leafMid);
    px_circle(t, cx,     H/5,      W/4, leafLight);
    px_circle(t, cx-4,   H/3+2,    W/5, leafLight);
    px_circle(t, cx+4,   H/3+2,    W/5, leafLight);
}

/* ============= رسم مبنى ============= */
static void draw_sprite_building(Texture *t, u32 wall, u32 accent) {
    int W = t->width, H = t->height;

    /* الجدار */
    px_rect(t, 4, 4, W-5, H-5, wall);

    /* حدود */
    for (int x = 4; x < W-4; x++) {
        px_set(t, x, 4, accent);
        px_set(t, x, H-5, accent);
    }
    for (int y = 4; y < H-4; y++) {
        px_set(t, 4, y, accent);
        px_set(t, W-5, y, accent);
    }

    /* شبكة نوافذ 3×4 */
    u32 winOn  = 0xFFFFE080u;
    u32 winOff = 0xFF1A2030u;
    int cols = 3, rows = 4;
    int marginX = 12, marginY = 12;
    int winW = (W - 2*marginX) / cols - 6;
    int winH = (H - 2*marginY) / rows - 6;

    for (int r = 0; r < rows; r++) {
        for (int c = 0; c < cols; c++) {
            int x0 = marginX + c * (winW + 6);
            int y0 = marginY + r * (winH + 6);
            u32 col = ((r + c) % 3 == 0) ? winOn : winOff;
            px_rect(t, x0, y0, x0 + winW, y0 + winH, col);
            /* إطار */
            for (int x = x0; x <= x0 + winW; x++) {
                px_set(t, x, y0, accent);
                px_set(t, x, y0 + winH, accent);
            }
            for (int y = y0; y <= y0 + winH; y++) {
                px_set(t, x0, y, accent);
                px_set(t, x0 + winW, y, accent);
            }
        }
    }

    /* باب */
    int doorW = W/6, doorH = H/5;
    int dx = W/2 - doorW/2;
    int dy = H - 5 - doorH;
    px_rect(t, dx, dy, dx + doorW, H - 5, 0xFF3A2010u);
    px_rect(t, dx + 2, dy + 2, dx + doorW - 2, H - 7, 0xFF5A3A20u);
    /* مقبض */
    px_rect(t, dx + doorW - 4, dy + doorH/2, dx + doorW - 3, dy + doorH/2 + 1, 0xFFFFD700u);
}

/* ============= رسم أرضية ============= */
static void draw_sprite_ground(Texture *t, u32 base, u32 line) {
    int W = t->width, H = t->height;
    px_rect(t, 0, 0, W-1, H-1, base);
    for (int i = 0; i < W; i += 8) {
        for (int j = 0; j < H; j++) px_set(t, i, j, line);
        for (int j = 0; j < W; j++) px_set(t, j, i, line);
    }
}

/* ============= تحميل texture من pixels ============= */
static int tex_from_pixels(Texture *t, u32 *pixels, int w, int h) {
    t->width = w;
    t->height = h;
    t->pixels = pixels;

    memset(&t->png, 0, sizeof(pngData));
    t->png.width    = w;
    t->png.height   = h;
    t->png.bmp_out  = pixels;

    t->offset = rsxLoadTexture(&t->png);
    t->loaded = 1;
    return 1;
}

/* تخصيص texture جديد */
static Texture *tex_create(int w, int h) {
    Texture *t = calloc(1, sizeof(Texture));
    t->width = w; t->height = h;
    t->pixels = (u32*)memalign(16, w*h*4);
    memset(t->pixels, 0, w*h*4);
    t->loaded = 0;
    return t;
}

static void tex_finalize(Texture *t) {
    tex_from_pixels(t, t->pixels, t->width, t->height);
}

static void tex_bind(Texture *tex) {
    if (!tex || !tex->loaded) return;
    rsxTextureOffset(tex->offset);
    rsxTextureFormat(TINY3D_TEX_FORMAT_A8R8G8B8);
    rsxTextureFilter(TINY3D_TEX_FILTER_LINEAR, TINY3D_TEX_FILTER_LINEAR);
    rsxTextureWrap(TINY3D_TEX_WRAP_CLAMP, TINY3D_TEX_WRAP_CLAMP);
    rsxTextureEnable();
}
static void tex_unbind(void) { rsxTextureDisable(); }

/* ===================== TEXTURE GLOBALS ===================== */
static Texture *g_texPlayerIdle;
static Texture *g_texPlayerWalk;
static Texture *g_texPed1, *g_texPed2, *g_texPed3;
static Texture *g_texCarRed, *g_texCarBlue, *g_texCarYellow;
static Texture *g_texTree;
static Texture *g_texBuildingA, *g_texBuildingB;
static Texture *g_texGround;

/* ===================== GLOBALS ===================== */
static Player g_player, g_car;
static AiCar  g_aiCars[MAX_AICARS];
static Ped    g_peds[MAX_PEDS];
static Building g_buildings[MAX_BUILDINGS]; static int g_numBuildings=0;
static Collider g_colliders[MAX_COLLIDERS]; static int g_numColliders=0;

static int g_running=1;
static float g_camYaw=0, g_camPitch=0.35f, g_camDist=9.0f;
static padInfo g_padInfo; static padData g_padData; static int g_padReady=0;
static u8 g_prevCross=0,g_prevL3=0,g_prevSelect=0,g_prevStart=0;
static float g_timeOfDay=0.25f, g_boostValue=1.0f;
static int g_headlights=0;
static u64 g_lastTick=0;
static int g_lastBlockI=9999, g_lastBlockJ=9999;
static audioPortId g_audioPort=0;
static int g_audioOK=0;
static float g_enginePhase=0;
static int g_currentOutfit = 0;

/* ===================== UTILS ===================== */
static inline float clampf(float v,float a,float b){return v<a?a:(v>b?b:v);}
static inline u64 tick_us(void){struct timeval tv;gettimeofday(&tv,NULL);
    return (u64)tv.tv_sec*1000000ULL+(u64)tv.tv_usec;}

static u32 g_rng=0xC0FFEE42u;
static inline u32 fastrand(void){g_rng^=g_rng<<13;g_rng^=g_rng>>17;g_rng^=g_rng<<5;return g_rng;}
static inline float frand01(void){return (fastrand()&0xFFFFFF)/(float)0xFFFFFF;}
static inline float frand(float a,float b){return a+frand01()*(b-a);}

/* ===================== AUDIO ===================== */
static short g_audioBuf[4][1024*2];
static void audio_init(void){
    if (audioInit()!=0) return;
    audioPortParam p; memset(&p,0,sizeof(p));
    p.numChannels=2; p.numBlocks=4; p.attrib=0; p.level=1.0f;
    if (audioPortOpen(&p,&g_audioPort)!=0) return;
    audioPortStart(g_audioPort); g_audioOK=1;
}
static void audio_shutdown(void){
    if(!g_audioOK) return;
    audioPortStop(g_audioPort); audioPortClose(g_audioPort); audioQuit(); g_audioOK=0;
}
static void audio_fill_block(short *out){
    float freq,amp;
    if (g_player.inCar){
        float spd=fabsf(g_car.speed);
        freq=60.0f+spd*3.5f; amp=0.22f+spd*0.004f;
        if(amp>0.45f)amp=0.45f;
    } else { freq=40.0f; amp=0.05f; }
    int boost = g_player.inCar && (g_padData.btn&PAD_TRIANGLE) && g_boostValue>0.05f;
    float bAmp = boost?0.35f:0.0f;
    for(int i=0;i<1024;i++){
        g_enginePhase += freq/48000.0f;
        if(g_enginePhase>1.0f) g_enginePhase-=1.0f;
        float sq=(g_enginePhase<0.5f)?1.0f:-1.0f;
        float s = sq*amp;
        if(bAmp>0) s += ((fastrand()&0xFFFF)/32768.0f-1.0f)*bAmp;
        short s16=(short)(s*32000.0f);
        *out++=s16; *out++=s16;
    }
}
static void audio_pump(void){
    if(!g_audioOK) return;
    static int bi=0;
    audio_fill_block(g_audioBuf[bi]);
    if(audioPortSend(g_audioPort,g_audioBuf[bi],1024*2*sizeof(short))==0)
        bi=(bi+1)%4;
}

/* ===================== SAVE/LOAD ===================== */
static u32 save_checksum(const SaveData*s){
    u32 h=2166136261u; const u8*p=(const u8*)s;
    for(size_t i=0;i<offsetof(SaveData,checksum);i++){h^=p[i];h*=16777619u;}
    return h;
}
static int save_game(void){
    SaveData s; memset(&s,0,sizeof(s));
    s.magic=SAVE_MAGIC; s.version=1;
    s.px=g_player.x; s.py=g_player.y; s.pz=g_player.z; s.pheading=g_player.heading;
    s.outfit=g_currentOutfit;
    s.cx=g_car.x; s.cy=g_car.y; s.cz=g_car.z; s.cheading=g_car.heading;
    s.timeOfDay=g_timeOfDay;
    s.checksum=save_checksum(&s);
    FILE*f=fopen(SAVE_PATH,"wb"); if(!f)return 0;
    fwrite(&s,sizeof(s),1,f); fclose(f); return 1;
}
static int load_game(void){
    FILE*f=fopen(SAVE_PATH,"rb"); if(!f)return 0;
    SaveData s;
    if(fread(&s,sizeof(s),1,f)!=1){fclose(f);return 0;}
    fclose(f);
    if(s.magic!=SAVE_MAGIC||s.version!=1)return 0;
    if(s.checksum!=save_checksum(&s))return 0;
    g_player.x=s.px; g_player.y=s.py; g_player.z=s.pz; g_player.heading=s.pheading;
    g_currentOutfit=s.outfit;
    g_car.x=s.cx; g_car.y=s.cy; g_car.z=s.cz; g_car.heading=s.cheading;
    g_timeOfDay=s.timeOfDay;
    return 1;
}

/* ===================== INIT TEXTURES ===================== */
static void textures_init(void){
    /* شخصيات */
    g_texPlayerIdle = tex_create(64,128);
    draw_sprite_person(g_texPlayerIdle, 0xFF2A7FD0u, 0xFF23252Bu, 0xFFD8A87Cu, 0xFF2A1A0Au, 1, 0);
    tex_finalize(g_texPlayerIdle);

    g_texPlayerWalk = tex_create(64,128);
    draw_sprite_person(g_texPlayerWalk, 0xFFFF8C42u, 0xFF1A1A1Au, 0xFFD8A87Cu, 0xFF3A2A1Au, 1, 1);
    tex_finalize(g_texPlayerWalk);

    g_texPed1 = tex_create(64,128);
    draw_sprite_person(g_texPed1, 0xFFB03A2Eu, 0xFF1A1A1Au, 0xFFD8A87Cu, 0xFF1A0A0Au, 1, 1);
    tex_finalize(g_texPed1);

    g_texPed2 = tex_create(64,128);
    draw_sprite_person(g_texPed2, 0xFF27AE60u, 0xFF2C3E50u, 0xFFE8C0A0u, 0xFF4A3A2Au, 0, 1);
    tex_finalize(g_texPed2);

    g_texPed3 = tex_create(64,128);
    draw_sprite_person(g_texPed3, 0xFF8E44ADu, 0xFF34495Eu, 0xFFC89870u, 0xFF0A0A0Au, 1, 1);
    tex_finalize(g_texPed3);

    /* عربيات */
    g_texCarRed = tex_create(128,64);
    draw_sprite_car(g_texCarRed, 0xFFC0392Bu, 0xFFFFFFFFu);
    tex_finalize(g_texCarRed);

    g_texCarBlue = tex_create(128,64);
    draw_sprite_car(g_texCarBlue, 0xFF2874A6u, 0xFFC0C0C0u);
    tex_finalize(g_texCarBlue);

    g_texCarYellow = tex_create(128,64);
    draw_sprite_car(g_texCarYellow, 0xFFD4AC0Du, 0xFF1A1A1Au);
    tex_finalize(g_texCarYellow);

    /* شجرة */
    g_texTree = tex_create(64,128);
    draw_sprite_tree(g_texTree);
    tex_finalize(g_texTree);

    /* مباني */
    g_texBuildingA = tex_create(128,128);
    draw_sprite_building(g_texBuildingA, 0xFFF0EAE0u, 0xFF404040u);
    tex_finalize(g_texBuildingA);

    g_texBuildingB = tex_create(128,128);
    draw_sprite_building(g_texBuildingB, 0xFF3A3A4Au, 0xFF1A1A22u);
    tex_finalize(g_texBuildingB);

    /* أرضية */
    g_texGround = tex_create(64,64);
    draw_sprite_ground(g_texGround, 0xFF1A1A22u, 0xFF2A2A36u);
    tex_finalize(g_texGround);
}

/* ===================== DRAW BILLBOARD ===================== */
static void draw_billboard(Texture *tex, float wx, float wy, float wz,
                           float w, float h, u32 tint){
    if(!tex || !tex->loaded) return;
    float camX=g_player.inCar?g_car.x:g_player.x;
    float camZ=g_player.inCar?g_car.z:g_player.z;
    float fx=wx-camX, fz=wz-camZ;
    float len=sqrtf(fx*fx+fz*fz);
    if(len<0.001f){fx=0;fz=-1;len=1;}
    fx/=len; fz/=len;
    float rx=-fz, rz=fx, hw=w*0.5f;
    float ax=wx-rx*hw, az=wz-rz*hw;
    float bx=wx+rx*hw, bz=wz+rz*hw;
    float y0=wy, y1=wy+h;
    tex_bind(tex);
    tiny3d_SetPolygon(TINY3D_QUADS);
    tiny3d_VertexPos(ax,y0,az); tiny3d_VertexTexture2D(0.0f,1.0f); tiny3d_VertexColor(tint);
    tiny3d_VertexPos(bx,y0,bz); tiny3d_VertexTexture2D(1.0f,1.0f);
    tiny3d_VertexPos(bx,y1,bz); tiny3d_VertexTexture2D(1.0f,0.0f);
    tiny3d_VertexPos(ax,y1,az); tiny3d_VertexTexture2D(0.0f,0.0f);
    tiny3d_End();
    tex_unbind();
}

static void draw_quad_color(float ax,float ay,float az,float bx,float by,float bz,
                             float cx,float cy,float cz,float dx,float dy,float dz,u32 color){
    tiny3d_SetPolygon(TINY3D_QUADS);
    tiny3d_VertexPos(ax,ay,az); tiny3d_VertexColor(color);
    tiny3d_VertexPos(bx,by,bz);
    tiny3d_VertexPos(cx,cy,cz);
    tiny3d_VertexPos(dx,dy,dz);
    tiny3d_End();
}

/* ===================== PALETTE ===================== */
static const u32 g_carColors[8]={0xFFB03A2Eu,0xFF2874A6u,0xFFD4AC0Du,0xFFECF0F1u,
                                 0xFF2C3E50u,0xFF229954u,0xFF7D3C98u,0xFFCA6F1Eu};
static const u32 g_pedColors[8]={0xFFB03A2Eu,0xFF2874A6u,0xFFD4AC0Du,0xFF229954u,
                                 0xFF7D3C98u,0xFFCA6F1Eu,0xFFECF0F1u,0xFF34495Eu};

/* ===================== BUILDINGS ===================== */
static void add_collider(float x,float z,float w,float d){
    if(g_numColliders>=MAX_COLLIDERS)return;
    Collider*c=&g_colliders[g_numColliders++];
    c->minX=x-w*0.5f; c->maxX=x+w*0.5f;
    c->minZ=z-d*0.5f; c->maxZ=z+d*0.5f;
}
static void add_building(int t,float x,float z,float w,float h,float d,u32 color){
    if(g_numBuildings>=MAX_BUILDINGS)return;
    Building*b=&g_buildings[g_numBuildings++];
    b->x=x;b->y=CURB_H;b->z=z;b->w=w;b->h=h;b->d=d;b->color=color;b->type=t;
}
static void build_tree(float cx,float cz){
    add_building(9,cx,cz,4.0f,6.0f,4.0f,0xFF2A6A2Au);
    add_collider(cx,cz,1.5f,1.5f);
}
static void build_tower(float cx,float cz,float w,float h,float d,u32 color){
    add_building(0,cx,cz,w,h,d,color); add_collider(cx,cz,w,d);
}
static void build_house(float cx,float cz){
    add_building(1,cx,cz,14,7,14,0xFFA05A3Au); add_collider(cx,cz,14,14);
}

static void gen_block(int i,int j){
    if(g_numBuildings>=MAX_BUILDINGS)return;
    float cx=(i+0.5f)*CELL, cz=(j+0.5f)*CELL;
    if(i==-1&&j==-1){
        for(int k=0;k<8;k++){
            float tx=cx+((k%3)-1)*15, tz=cz+((k/3)-1)*15;
            build_tree(tx,tz);
        }
        return;
    }
    if(i==0&&j==0){
        add_building(2,cx,cz-8,30,8,16,0xFF1A1A22u);
        add_collider(cx,cz-8,30,16);
        return;
    }
    if(i==1&&j==0){
        add_building(3,cx,cz-10,14,6,10,0xFF222225u); add_collider(cx,cz-10,14,10);
        return;
    }
    u32 h=((u32)(i&0xFFFF)*73856093u)^((u32)(j&0xFFFF)*19349663u);
    if(h==0)h=1;
    switch(h%5){
        case 0: build_tower(cx,cz,28,20+(h>>4)%30,28,0xFF2A2A38u); break;
        case 1:
            build_tower(cx-9,cz,16,16+(h>>6)%24,28,0xFF35404Eu);
            build_tower(cx+9,cz,16,16+(h>>8)%24,28,0xFF44404Eu); break;
        case 2:
            build_house(cx-10,cz-10); build_house(cx+10,cz-10);
            build_house(cx-10,cz+10); build_house(cx+10,cz+10); break;
        case 3:
            build_tower(cx,cz,34,14,24,0xFF3A3A4Au); break;
        default:
            build_tower(cx-10,cz-2,16,18+(h>>10)%14,20,0xFF3A3546u);
            build_house(cx+10,cz+5); break;
    }
}
static void gen_city_around(float px,float pz){
    int pi=(int)floorf(px/CELL+0.5f), pj=(int)floorf(pz/CELL+0.5f);
    g_numBuildings=0; g_numColliders=0;
    for(int i=pi-RENDER_R;i<=pi+RENDER_R;i++)
        for(int j=pj-RENDER_R;j<=pj+RENDER_R;j++)
            gen_block(i,j);
}

/* ===================== COLLISION ===================== */
static int hits_building(float x,float z,float r){
    for(int i=0;i<g_numColliders;i++){
        Collider*c=&g_colliders[i];
        if(x>c->minX-r&&x<c->maxX+r&&z>c->minZ-r&&z<c->maxZ+r) return 1;
    }
    return 0;
}
static float ground_height_at(float x,float z,float cy){ (void)x;(void)z;(void)cy; return CURB_H; }

/* ===================== INPUT ===================== */
static void read_pad(void){
    g_padReady=0;
    if(ioPadGetInfo(&g_padInfo)!=0) return;
    if(!g_padInfo.status[0]) return;
    if(ioPadGetData(0,&g_padData)!=0) return;
    g_padReady=1;
}

/* ===================== AI ===================== */
static void assign_car_path(AiCar*c,float px,float pz){
    int pi=(int)floorf(px/CELL+0.5f), pj=(int)floorf(pz/CELL+0.5f);
    int horiz=(fastrand()&1), dir=(fastrand()&1)?1:-1;
    if(horiz){
        int jOff=(int)(fastrand()%(RENDER_R*2+3))-RENDER_R-1;
        float lineZ=(pj+jOff)*CELL+(dir>0?5.0f:-5.0f);
        float xS=(pi-RENDER_R-2)*CELL, xE=(pi+RENDER_R+2)*CELL;
        c->axis=0; c->dir=dir; c->x=(dir>0)?xS:xE; c->z=lineZ;
        c->speed=18.0f+frand(0,12.0f);
        c->minP=xS<xE?xS:xE; c->maxP=xS<xE?xE:xS;
    } else {
        int iOff=(int)(fastrand()%(RENDER_R*2+3))-RENDER_R-1;
        float lineX=(pi+iOff)*CELL+(dir>0?-5.0f:5.0f);
        float zS=(pj-RENDER_R-2)*CELL, zE=(pj+RENDER_R+2)*CELL;
        c->axis=1; c->dir=dir; c->x=lineX; c->z=(dir>0)?zS:zE;
        c->speed=18.0f+frand(0,12.0f);
        c->minP=zS<zE?zS:zE; c->maxP=zS<zE?zE:zS;
    }
}
static void update_ai_cars(float dt,float px,float pz){
    for(int i=0;i<MAX_AICARS;i++){
        AiCar*c=&g_aiCars[i];
        if(c->axis==0){
            c->x+=c->speed*c->dir*dt;
            if(c->x>c->maxP||c->x<c->minP) assign_car_path(c,px,pz);
        } else {
            c->z+=c->speed*c->dir*dt;
            if(c->z>c->maxP||c->z<c->minP) assign_car_path(c,px,pz);
        }
    }
}
static void assign_ped_target(Ped*p,float px,float pz){
    int pi=(int)floorf(px/CELL+0.5f), pj=(int)floorf(pz/CELL+0.5f);
    int i=pi+(int)(fastrand()%(RENDER_R*2+1))-RENDER_R;
    int j=pj+(int)(fastrand()%(RENDER_R*2+1))-RENDER_R;
    float cx=(i+0.5f)*CELL, cz=(j+0.5f)*CELL, R=22.0f;
    p->sx=cx-R+frand(0,2*R); p->sz=cz-R;
    p->ex=cx-R+frand(0,2*R); p->ez=cz+R;
    float dx=p->ex-p->sx, dz=p->ez-p->sz;
    float len=sqrtf(dx*dx+dz*dz); if(len<0.01f)len=0.01f;
    p->speed=(1.5f+frand(0,1.5f))/len;
    p->t=0; p->phase=frand(0,6.283f);
}
static void update_peds(float dt,float px,float pz){
    for(int i=0;i<MAX_PEDS;i++){
        Ped*p=&g_peds[i];
        p->t+=p->speed*dt;
        if(p->t>=1.0f){assign_ped_target(p,px,pz);}
    }
}

/* ===================== DAY/NIGHT ===================== */
static u32 sky_color(void){
    float sunY=sinf((g_timeOfDay-0.25f)*6.2831853f);
    float day=sunY>0?sunY:0;
    u8 r=(u8)(10+day*30), g=(u8)(18+day*80), b=(u8)(40+day*130);
    return 0xFF000000u|((u32)r<<16)|((u32)g<<8)|b;
}
static float day_factor(void){
    float sunY=sinf((g_timeOfDay-0.25f)*6.2831853f);
    return clampf(sunY*1.6f+0.15f,0.0f,1.0f);
}
static int is_night(void){return day_factor()<0.25f;}

/* ===================== UPDATE ===================== */
static void toggle_car(void){
    if(g_player.inCar){
        g_player.inCar=0;
        float h=g_car.heading;
        float lx=cosf(h), lz=-sinf(h);
        float ex=g_car.x+lx*2.3f, ez=g_car.z+lz*2.3f;
        g_player.x=ex; g_player.z=ez;
        g_player.y=CURB_H;
        g_player.heading=h; g_player.speed=0; g_player.yVel=0; g_player.grounded=1;
    } else {
        float dx=g_player.x-g_car.x, dz=g_player.z-g_car.z;
        if(dx*dx+dz*dz<16.0f){
            g_player.inCar=1; g_car.heading=g_player.heading; g_car.speed=0;
            g_enginePhase=0;
        }
    }
}
static void cycle_outfit(void){ g_currentOutfit=(g_currentOutfit+1)%6; }

static void update(float dt){
    if(!g_padReady) return;
    float lx=(float)g_padData.ana[0]/128.0f;
    float ly=(float)g_padData.ana[1]/128.0f;
    float rx=(float)g_padData.ana[2]/128.0f;
    float ry=(float)g_padData.ana[3]/128.0f;
    if(fabsf(lx)<0.15f)lx=0; if(fabsf(ly)<0.15f)ly=0;
    if(fabsf(rx)<0.15f)rx=0; if(fabsf(ry)<0.15f)ry=0;

    u16 btn=g_padData.btn;
    u8 cross=(btn&PAD_CROSS)?1:0;
    u8 l3=(btn&PAD_L3)?1:0, sel=(btn&PAD_SELECT)?1:0, start=(btn&PAD_START)?1:0;

    if(cross&&!g_prevCross) toggle_car();
    g_prevCross=cross;
    if(l3&&!g_prevL3) g_headlights=!g_headlights;
    g_prevL3=l3;
    if(sel&&!g_prevSelect) save_game();
    g_prevSelect=sel;
    if(start&&!g_prevStart) cycle_outfit();
    g_prevStart=start;

    g_camYaw+=rx*2.4f*dt;
    g_camPitch=clampf(g_camPitch+ry*1.6f*dt,-0.15f,1.25f);
    if(btn&PAD_R2) g_camDist+=14.0f*dt;
    if(btn&PAD_L2) g_camDist-=14.0f*dt;
    g_camDist=clampf(g_camDist,4.0f,26.0f);

    if(g_player.inCar){
        float throttle=-ly, steer=lx;
        if(btn&PAD_R1) throttle+=1.0f;
        throttle=clampf(throttle,-1.0f,1.0f);
        steer=clampf(steer,-1.0f,1.0f);
        if(btn&PAD_L1){ g_car.speed*=(1.0f-4.0f*dt); if(fabsf(g_car.speed)<0.5f)g_car.speed=0; }
        int boosting=(btn&PAD_TRIANGLE)&&g_boostValue>0.05f;
        if(boosting){ g_boostValue=clampf(g_boostValue-BOOST_DRAIN*dt,0,1); g_car.speed+=30.0f*dt; }
        else g_boostValue=clampf(g_boostValue+BOOST_REGEN*dt,0,1);

        g_car.speed+=throttle*CAR_ACCEL*dt;
        if(fabsf(throttle)<0.05f){ g_car.speed*=(1.0f-1.8f*dt); if(fabsf(g_car.speed)<0.2f)g_car.speed=0; }
        g_car.speed=clampf(g_car.speed,-CAR_REV_SPEED,CAR_MAX_SPEED);
        if(fabsf(g_car.speed)>0.4f){
            float sgn=(g_car.speed>0)?1.0f:-1.0f;
            float gain=clampf(fabsf(g_car.speed)/20.0f,0.3f,1.0f);
            g_car.heading-=steer*CAR_TURN*gain*dt*sgn;
        }
        float vx=sinf(g_car.heading)*g_car.speed*dt;
        float vz=cosf(g_car.heading)*g_car.speed*dt;
        float nx=g_car.x+vx, nz=g_car.z+vz;
        if(!hits_building(nx,g_car.z,CAR_R)) g_car.x=nx; else g_car.speed*=0.3f;
        if(!hits_building(g_car.x,nz,CAR_R)) g_car.z=nz; else g_car.speed*=0.3f;
        g_car.y=CURB_H+0.02f;
    } else {
        float speed=(btn&PAD_R1)?RUN_SPD:WALK_SPD;
        float mfx=-sinf(g_camYaw), mfz=-cosf(g_camYaw);
        float mrx=cosf(g_camYaw), mrz=-sinf(g_camYaw);
        float mvx=mfx*(-ly)+mrx*lx;
        float mvz=mfz*(-ly)+mrz*lx;
        float mag=sqrtf(mvx*mvx+mvz*mvz);
        if(mag>0.05f){
            mvx/=mag; mvz/=mag;
            float nx=g_player.x+mvx*speed*dt;
            float nz=g_player.z+mvz*speed*dt;
            if(!hits_building(nx,g_player.z,PLAYER_R)) g_player.x=nx;
            if(!hits_building(g_player.x,nz,PLAYER_R)) g_player.z=nz;
            float tgt=atan2f(mvx,mvz), diff=tgt-g_player.heading;
            while(diff>3.14159f)diff-=6.28318f;
            while(diff<-3.14159f)diff+=6.28318f;
            g_player.heading+=diff*clampf(dt*12.0f,0,1);
        }
        float gy=CURB_H;
        g_player.yVel+=GRAVITY*dt;
        g_player.y+=g_player.yVel*dt;
        if(g_player.y<=gy){g_player.y=gy; g_player.yVel=0; g_player.grounded=1;}
        else g_player.grounded=0;
        if((btn&PAD_SQUARE)&&g_player.grounded){g_player.yVel=JUMP_V; g_player.grounded=0;}
    }
    g_timeOfDay+=dt/DAY_DURATION; if(g_timeOfDay>1.0f) g_timeOfDay-=1.0f;
    update_ai_cars(dt,g_player.x,g_player.z);
    update_peds(dt,g_player.x,g_player.z);
}

/* ===================== RENDER ===================== */
static void render_world(void){
    tiny3d_Clear(sky_color(),0xFFFFFFFF);

    float fx=g_player.inCar?g_car.x:g_player.x;
    float fz=g_player.inCar?g_car.z:g_player.z;
    int ci=(int)floorf(fx/CELL+0.5f), cj=(int)floorf(fz/CELL+0.5f);
    if(ci!=g_lastBlockI||cj!=g_lastBlockJ){
        gen_city_around(fx,fz); g_lastBlockI=ci; g_lastBlockJ=cj;
    }

    /* 360° camera */
    float cx=fx-sinf(g_camYaw)*g_camDist*cosf(g_camPitch);
    float cy=3.0f+sinf(g_camPitch)*g_camDist;
    float cz=fz-cosf(g_camYaw)*g_camDist*cosf(g_camPitch);

    set_3d_projection(60.0f,0.3f,800.0f,(float)SCR_W/(float)SCR_H);
    set_3d_view(cx,cy,cz,fx,1.6f,fz);

    /* أرضية بـ texture */
    int pi=(int)floorf(fx/CELL+0.5f), pj=(int)floorf(fz/CELL+0.5f);
    for(int i=-RENDER_R-1;i<=RENDER_R+1;i++)
        for(int j=-RENDER_R-1;j<=RENDER_R+1;j++){
            float gx=(pi+i)*CELL, gz=(pj+j)*CELL;
            u32 gc=((i+j)&1)?0xFF16161Cu:0xFF1A1A22u;
            float hw=CELL*0.5f-1, hd=CELL*0.5f-1;
            tiny3d_SetPolygon(TINY3D_QUADS);
            tiny3d_VertexPos(gx-hw,0,gz-hd); tiny3d_VertexColor(gc);
            tiny3d_VertexPos(gx+hw,0,gz-hd);
            tiny3d_VertexPos(gx+hw,0,gz+hd);
            tiny3d_VertexPos(gx-hw,0,gz+hd);
            tiny3d_End();
        }

    /* مبانٍ - billboard */
    for(int i=0;i<g_numBuildings;i++){
        Building*b=&g_buildings[i];
        if(b->type==9){
            draw_billboard(g_texTree, b->x, b->y, b->z, b->w, b->h, 0xFFFFFFFF);
        } else if(b->type==2 || b->type==3){
            draw_billboard(g_texBuildingB, b->x, b->y, b->z, b->w, b->h, 0xFFFFFFFF);
        } else {
            draw_billboard(g_texBuildingA, b->x, b->y, b->z, b->w, b->h, 0xFFFFFFFF);
        }
    }

    /* AI cars */
    for(int i=0;i<MAX_AICARS;i++){
        AiCar*c=&g_aiCars[i];
        Texture *tex = (i%3==0)?g_texCarRed : (i%3==1)?g_texCarBlue : g_texCarYellow;
        draw_billboard(tex, c->x, 0.2f, c->z, 3.5f, 1.8f, 0xFFFFFFFF);
    }

    /* pedestrians */
    for(int i=0;i<MAX_PEDS;i++){
        Ped*p=&g_peds[i];
        float px=p->sx+(p->ex-p->sx)*p->t;
        float pz=p->sz+(p->ez-p->sz)*p->t;
        float bob=fabsf(sinf(p->t*30.0f+p->phase))*0.05f;
        Texture *tex = (i%3==0)?g_texPed1 : (i%3==1)?g_texPed2 : g_texPed3;
        draw_billboard(tex, px, CURB_H+bob, pz, 0.9f, 1.9f, 0xFFFFFFFF);
    }

    /* player car */
    draw_billboard(g_texCarRed, g_car.x, g_car.y+0.1f, g_car.z, 3.8f, 2.0f, 0xFFFFFFFF);

    /* player on foot */
    if(!g_player.inCar){
        int moving=(abs(g_padData.ana[0])>20||abs(g_padData.ana[1])>20);
        Texture *tex = moving ? g_texPlayerWalk : g_texPlayerIdle;
        draw_billboard(tex, g_player.x, g_player.y, g_player.z, 0.9f, 1.9f, 0xFFFFFFFF);
    }
}

/* ===================== HUD ===================== */
static void draw_hud(void){
    char buf[128];
    SetFontSize(22,22); SetFontColor(0xFF00D4FFu);
    DrawString(20,30,"NEON CITY");
    SetFontSize(12,12); SetFontColor(0xFFFFFFFFu);
    DrawString(20,52,"PS3 HOMEBREW - 3D 360 - PROCEDURAL");

    int hours=(int)((g_timeOfDay*24.0f)+6.0f);
    int minutes=(int)(((g_timeOfDay*24.0f)+6.0f-(float)hours)*60.0f);
    hours%=24;
    sprintf(buf,"%02d:%02d",hours,minutes);
    SetFontSize(24,24); SetFontColor(0xFFFFCC00u);
    DrawString(SCR_W-150,30,buf);

    sprintf(buf,"POS %.0f, %.0f",g_player.x,g_player.z);
    SetFontSize(14,14); SetFontColor(0xFFFFFFFFu);
    DrawString(20,SCR_H-100,buf);

    if(g_player.inCar){
        sprintf(buf,"SPEED %.0f KM/H",fabsf(g_car.speed)*2.5f);
        SetFontSize(20,20); SetFontColor(0xFFFFCC00u);
        DrawString(20,SCR_H-70,buf);
        int bw=(int)(g_boostValue*120.0f);
        tiny3d_SetPolygon(TINY3D_QUADS);
        tiny3d_VertexPos(SCR_W-180,SCR_H-90,0); tiny3d_VertexColor(0xFFFF6B35u);
        tiny3d_VertexPos(SCR_W-180+bw,SCR_H-90,0);
        tiny3d_VertexPos(SCR_W-180+bw,SCR_H-80,0);
        tiny3d_VertexPos(SCR_W-180,SCR_H-80,0);
        tiny3d_End();
    }
    SetFontSize(11,11); SetFontColor(0xFFAAAAAAu);
    DrawString(20,SCR_H-20,"CROSS:enter/exit  R1:run/gas  SQUARE:jump  L3:lights  SELECT:save");
}

/* ===================== MAIN ===================== */
static void sysutil_cb(u64 status,u64 param,void*ud){
    (void)param;(void)ud;
    if(status==SYSUTIL_EXIT_GAME) g_running=0;
}

int main(int argc,char*argv[]){
    (void)argc;(void)argv;
    sysModuleLoad(SYSMODULE_FS);
    sysModuleLoad(SYSMODULE_PAD);
    sysModuleLoad(SYSMODULE_AUDIO);
    ioPadInit(7);
    sysUtilRegisterCallback(SYSUTIL_EVENT_SLOT0,sysutil_cb,NULL);
    if(tiny3d_Init(1024*1024)!=0) return 1;
    ResetFont();
    audio_init();
    textures_init();

    memset(&g_player,0,sizeof(g_player));
    g_player.x=HOME_CX; g_player.z=HOME_CZ+20.0f;
    g_player.y=CURB_H; g_player.heading=0; g_player.grounded=1;
    memset(&g_car,0,sizeof(g_car));
    g_car.x=HOME_CX+20; g_car.z=HOME_CZ;
    g_car.y=CURB_H+0.02f; g_car.heading=3.14159f;

    for(int i=0;i<MAX_AICARS;i++){
        g_aiCars[i].color=g_carColors[i%8];
        assign_car_path(&g_aiCars[i],g_player.x,g_player.z);
    }
    for(int i=0;i<MAX_PEDS;i++){
        g_peds[i].color=g_pedColors[i%8];
        assign_ped_target(&g_peds[i],g_player.x,g_player.z);
        g_peds[i].t=frand(0,1);
    }
    load_game();
    gen_city_around(g_player.x,g_player.z);
    g_lastBlockI=(int)floorf(g_player.x/CELL+0.5f);
    g_lastBlockJ=(int)floorf(g_player.z/CELL+0.5f);
    g_lastTick=tick_us();

    while(g_running){
        sysUtilCheckCallback();
        read_pad();
        u64 now=tick_us();
        float dt=(float)(now-g_lastTick)/1000000.0f;
        if(dt>0.1f) dt=0.1f;
        g_lastTick=now;
        update(dt);
        audio_pump();
        render_world();
        set_2d_view(0,0,SCR_W,SCR_H);
        draw_hud();
        tiny3d_Flip();
    }
    save_game();
    audio_shutdown();
    ioPadEnd();
    tiny3d_Exit();
    return 0;
}
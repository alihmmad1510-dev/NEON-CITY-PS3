/* =====================================================================
 *  FILE: src/main.c
 *  PROJECT: NEON CITY ULTRA v5 - PS3 BLACK SCREEN FIX
 *  DESCRIPTION: Tiny3D 3D/2D game with OBJ models
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

#include "obj_loader.h"

/* From assets.c */
void assets_init(void);
u32  assets_count(void);

SYS_PROCESS_PARAM(1001, 0x100000)

/* ===================== CONFIG ===================== */

#define SCR_W 1280
#define SCR_H 720
#define FOV   700.0f

#define MAX_BLD   200
#define MAX_AI    20
#define MAX_PAR   180
#define MAX_STARS 200

/* ===================== COLORS ===================== */

#define NEON_CYAN     0xFF00FFFFu
#define NEON_MAGENTA  0xFFFF00FFu
#define NEON_PURPLE   0xFF9B30FFu
#define NEON_BLUE     0xFF0080FFu
#define NEON_PINK     0xFFFF1493u
#define NEON_LIME     0xFF39FF14u
#define NEON_ORANGE   0xFFFF6600u
#define NEON_YELLOW   0xFFFFDD00u

#define DARK_BG       0xFF05050Fu
#define GROUND_DARK   0xFF0A0A1Fu
#define GRID_LINE     0xFF0080AAu

/* ===================== TYPES ===================== */

typedef struct {
    float x, y, z;
} Vec3;

typedef struct {
    float x, y, w, h;
    int vis;
    float z;
} SP;

typedef enum {
    ST_LOADING = 0,
    ST_MENU,
    ST_PLAYING,
    ST_PAUSED
} GameState;

typedef struct {
    float x, z;
    float w, h, d;
    u32 color;
    u32 top_color;
    u32 glow_color;
    int type;
} Obj3D;

typedef struct {
    float x, z;
    float heading;
    float speed;
    int axis;
    int dir;
    u32 color;
} AiCar;

typedef struct {
    float x, y, z;
    float vx, vy, vz;
    int life;
    int max_life;
    u32 color;
    float size;
} Particle;

typedef struct {
    float x, y, z;
    u8 brightness;
} Star;

/* ===================== STATE ===================== */

static GameState g_state = ST_LOADING;

static float g_loadProgress = 0.0f;
static float g_menuTimer = 0.0f;

static int g_menuSel = 0;
static int g_running = 1;

static padInfo g_padInfo;
static padData g_padData;

static int g_padReady = 0;

static u8 g_prevCross = 0;
static u8 g_prevStart = 0;
static u8 g_prevUp = 0;
static u8 g_prevDown = 0;
static u8 g_prevL3 = 0;
static u8 g_prevR3 = 0;

static float p_x = 0.0f;
static float p_z = 0.0f;
static float p_heading = 0.0f;
static float p_speed = 0.0f;
static float p_boost = 1.0f;

static int p_carColor = 0;

static float cam_yaw = 0.0f;
static float cam_pitch = 0.45f;
static float cam_dist = 20.0f;

static float cam_target_y = 1.5f;

static float g_timeOfDay = 0.25f;
static int g_score = 0;

static float g_shake = 0.0f;
static float g_msgTimer = 0.0f;

static char g_msg[64] = "";

static Obj3D g_objs[MAX_BLD];
static int g_numObjs = 0;

static AiCar g_ai[MAX_AI];

static Particle g_parts[MAX_PAR];
static int g_numParts = 0;

static Star g_stars[MAX_STARS];
static int g_numStars = 0;

static ObjModel g_carModels[5];
static ObjModel g_bldModels[2];

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

static u16 pad_btns(void)
{
    u8 *p = (u8 *)&g_padData;
    return (u16)(p[2] | (p[3] << 8));
}

static int pad_lx(void)
{
    return ((s8 *)&g_padData)[6];
}

static int pad_ly(void)
{
    return ((s8 *)&g_padData)[7];
}

static int pad_rx(void)
{
    return ((s8 *)&g_padData)[4];
}

static int pad_ry(void)
{
    return ((s8 *)&g_padData)[5];
}

/* ===================== RNG ===================== */

static u32 rng_s = 0xC0FFEE42u;

static u32 rng(void)
{
    rng_s ^= rng_s << 13;
    rng_s ^= rng_s >> 17;
    rng_s ^= rng_s << 5;

    return rng_s;
}

static float frand01(void)
{
    return (rng() & 0xFFFFFF) / (float)0xFFFFFF;
}

static float frand(float a, float b)
{
    return a + frand01() * (b - a);
}

static float clampf(float v, float a, float b)
{
    if (v < a)
        return a;

    if (v > b)
        return b;

    return v;
}

/* ===================== VEC3 ===================== */

static inline Vec3 vec3(float x, float y, float z)
{
    Vec3 v = {x, y, z};
    return v;
}

static inline Vec3 vec3_sub(Vec3 a, Vec3 b)
{
    return vec3(
        a.x - b.x,
        a.y - b.y,
        a.z - b.z
    );
}

static inline float vec3_dot(Vec3 a, Vec3 b)
{
    return
        a.x * b.x +
        a.y * b.y +
        a.z * b.z;
}

static inline Vec3 vec3_cross(Vec3 a, Vec3 b)
{
    return vec3(
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x
    );
}

static inline Vec3 vec3_norm(Vec3 v)
{
    float l =
        sqrtf(
            v.x * v.x +
            v.y * v.y +
            v.z * v.z
        );

    if (l < 0.0001f)
        return vec3(0, 0, 1);

    return vec3(
        v.x / l,
        v.y / l,
        v.z / l
    );
}

/* ===================== CAMERA ===================== */

static Vec3 camera_pos(void)
{
    float cp = cosf(cam_pitch);
    float sp = sinf(cam_pitch);

    return vec3(
        p_x - sinf(cam_yaw) * cam_dist * cp,
        cam_target_y + sp * cam_dist,
        p_z - cosf(cam_yaw) * cam_dist * cp
    );
}

static SP project(Vec3 wp)
{
    SP out;

    memset(&out, 0, sizeof(out));

    Vec3 cam = camera_pos();

    Vec3 rel =
        vec3_sub(wp, cam);

    Vec3 target =
        vec3(
            p_x,
            cam_target_y,
            p_z
        );

    Vec3 forward =
        vec3_norm(
            vec3_sub(
                target,
                cam
            )
        );

    Vec3 world_up =
        vec3(0, 1, 0);

    Vec3 right =
        vec3_norm(
            vec3_cross(
                forward,
                world_up
            )
        );

    Vec3 up =
        vec3_cross(
            right,
            forward
        );

    float rx =
        vec3_dot(
            rel,
            right
        );

    float ry =
        vec3_dot(
            rel,
            up
        );

    float rz =
        vec3_dot(
            rel,
            forward
        );

    if (rz < 0.3f)
    {
        out.vis = 0;
        return out;
    }

    out.x =
        SCR_W * 0.5f +
        (rx / rz) * FOV;

    out.y =
        SCR_H * 0.5f -
        (ry / rz) * FOV;

    out.z = rz;
    out.vis = 1;

    return out;
}

void project_point(
    float wx,
    float wy,
    float wz,
    float *sx,
    float *sy,
    float *sz,
    int *vis
)
{
    SP sp =
        project(
            vec3(
                wx,
                wy,
                wz
            )
        );

    *sx = sp.x;
    *sy = sp.y;
    *sz = sp.z;
    *vis = sp.vis;
}

/* ===================== DRAW HELPERS ===================== */

static void begin_2d(void)
{
    tiny3d_Project2D();
}

static void begin_3d(void)
{
    tiny3d_Project3D();
}

static void draw_quad_3d(
    Vec3 a,
    Vec3 b,
    Vec3 c,
    Vec3 d,
    u32 color
)
{
    SP pa = project(a);
    SP pb = project(b);
    SP pc = project(c);
    SP pd = project(d);

    if (!pa.vis ||
        !pb.vis ||
        !pc.vis ||
        !pd.vis)
    {
        return;
    }

    begin_2d();

    tiny3d_SetPolygon(TINY3D_QUADS);

    tiny3d_VertexPos(
        pa.x,
        pa.y,
        0
    );
    tiny3d_VertexColor(color);

    tiny3d_VertexPos(
        pb.x,
        pb.y,
        0
    );

    tiny3d_VertexPos(
        pc.x,
        pc.y,
        0
    );

    tiny3d_VertexPos(
        pd.x,
        pd.y,
        0
    );

    tiny3d_End();
}

static void draw_rect2d(
    float x,
    float y,
    float w,
    float h,
    u32 col
)
{
    begin_2d();

    tiny3d_SetPolygon(TINY3D_QUADS);

    tiny3d_VertexPos(
        x,
        y,
        0
    );
    tiny3d_VertexColor(col);

    tiny3d_VertexPos(
        x + w,
        y,
        0
    );

    tiny3d_VertexPos(
        x + w,
        y + h,
        0
    );

    tiny3d_VertexPos(
        x,
        y + h,
        0
    );

    tiny3d_End();
}

static void draw_glow_3d(
    float wx,
    float wy,
    float wz,
    float radius,
    u32 color
)
{
    SP sp =
        project(
            vec3(
                wx,
                wy,
                wz
            )
        );

    if (!sp.vis)
        return;

    float r =
        radius / sp.z * FOV;

    if (r < 3.0f)
        r = 3.0f;

    if (r > 150.0f)
        r = 150.0f;

    for (int layer = 4; layer >= 1; layer--)
    {
        float lr =
            r * (float)layer / 4.0f;

        u8 a =
            (u8)(
                15 +
                (4 - layer) * 12
            );

        u32 c =
            ((u32)a << 24) |
            (color & 0xFFFFFF);

        for (
            int dy = -(int)lr;
            dy <= (int)lr;
            dy += 5
        )
        {
            float inside =
                lr * lr -
                (float)(dy * dy);

            if (inside < 0)
                continue;

            float w =
                sqrtf(inside);

            draw_rect2d(
                sp.x - w,
                sp.y + dy,
                w * 2.0f,
                5.0f,
                c
            );
        }
    }
}

/* ===================== GROUND ===================== */

static void draw_ground_grid(void)
{
    int bI =
        (int)(p_x / 60.0f);

    int bJ =
        (int)(p_z / 60.0f);

    float cell = 60.0f;

    int R = 5;

    for (int i = -R; i <= R; i++)
    {
        for (int j = -R; j <= R; j++)
        {
            float gx =
                (bI + i) * cell;

            float gz =
                (bJ + j) * cell;

            float hc =
                cell * 0.5f - 0.5f;

            draw_quad_3d(
                vec3(
                    gx - hc,
                    0,
                    gz - hc
                ),
                vec3(
                    gx + hc,
                    0,
                    gz - hc
                ),
                vec3(
                    gx + hc,
                    0,
                    gz + hc
                ),
                vec3(
                    gx - hc,
                    0,
                    gz + hc
                ),
                GROUND_DARK
            );
        }
    }

    for (
        int i = -R;
        i <= R + 1;
        i++
    )
    {
        float gx =
            (bI + i) * cell -
            cell * 0.5f;

        draw_quad_3d(
            vec3(
                gx - 0.15f,
                0.01f,
                (bJ - R) * cell
            ),
            vec3(
                gx + 0.15f,
                0.01f,
                (bJ - R) * cell
            ),
            vec3(
                gx + 0.15f,
                0.01f,
                (bJ + R + 1) * cell
            ),
            vec3(
                gx - 0.15f,
                0.01f,
                (bJ + R + 1) * cell
            ),
            GRID_LINE
        );
    }

    for (
        int j = -R;
        j <= R + 1;
        j++
    )
    {
        float gz =
            (bJ + j) * cell -
            cell * 0.5f;

        draw_quad_3d(
            vec3(
                (bI - R) * cell,
                0.01f,
                gz - 0.15f
            ),
            vec3(
                (bI + R + 1) * cell,
                0.01f,
                gz - 0.15f
            ),
            vec3(
                (bI + R + 1) * cell,
                0.01f,
                gz + 0.15f
            ),
            vec3(
                (bI - R) * cell,
                0.01f,
                gz + 0.15f
            ),
            GRID_LINE
        );
    }
}

/* ===================== SKY ===================== */

static void draw_sky(void)
{
    begin_2d();

    tiny3d_Clear(
        DARK_BG,
        TINY3D_CLEAR_ALL
    );

    draw_rect2d(
        0,
        0,
        SCR_W,
        SCR_H * 0.35f,
        0xFF050515u
    );

    draw_rect2d(
        0,
        SCR_H * 0.35f,
        SCR_W,
        SCR_H * 0.15f,
        0xFF0A0520u
    );

    draw_rect2d(
        0,
        SCR_H * 0.50f,
        SCR_W,
        SCR_H * 0.08f,
        0xFF150A30u
    );

    draw_rect2d(
        0,
        SCR_H * 0.58f,
        SCR_W,
        3,
        NEON_CYAN
    );

    draw_rect2d(
        0,
        SCR_H * 0.58f + 3,
        SCR_W,
        2,
        0x6000FFFFu
    );

    for (int i = 0; i < g_numStars; i++)
    {
        int sx =
            (int)g_stars[i].x;

        int sy =
            (int)g_stars[i].y;

        if (
            sx < 0 ||
            sx >= SCR_W ||
            sy < 0 ||
            sy > SCR_H * 0.58f
        )
        {
            continue;
        }

        u8 b =
            g_stars[i].brightness;

        u32 col =
            0xFF000000u |
            ((u32)b << 16) |
            ((u32)b << 8) |
            b;

        draw_rect2d(
            sx,
            sy,
            2,
            2,
            col
        );
    }
}

/* ===================== CITY ===================== */

static void add_obj3d(
    float x,
    float z,
    float w,
    float h,
    float d,
    u32 col,
    int type
)
{
    if (g_numObjs >= MAX_BLD)
        return;

    Obj3D *o =
        &g_objs[g_numObjs++];

    o->x = x;
    o->z = z;
    o->w = w;
    o->h = h;
    o->d = d;

    o->color = col;

    u8 r =
        (col >> 16) & 0xFF;

    u8 g =
        (col >> 8) & 0xFF;

    u8 b =
        col & 0xFF;

    o->top_color =
        0xFF000000u |
        ((u32)(r < 230 ? r + 25 : 255) << 16) |
        ((u32)(g < 230 ? g + 25 : 255) << 8) |
        (b < 230 ? b + 25 : 255);

    o->glow_color =
        0xFF000000u |
        ((u32)(r / 2) << 16) |
        ((u32)(g / 2) << 8) |
        (b / 2);

    o->type = type;
}

static void gen_city(void)
{
    g_numObjs = 0;

    int bI =
        (int)(p_x / 60.0f);

    int bJ =
        (int)(p_z / 60.0f);

    static const u32 palette[] =
    {
        NEON_CYAN,
        NEON_MAGENTA,
        NEON_PURPLE,
        NEON_BLUE,
        NEON_PINK,
        0xFF00CCAAu,
        0xFFAA00FFu,
        0xFF00AAFFu,
        0xFFFF0080u,
        0xFF8800FFu
    };

    for (int i = -4; i <= 4; i++)
    {
        for (int j = -4; j <= 4; j++)
        {
            int gi = bI + i;
            int gj = bJ + j;

            if (gi == 0 && gj == 0)
                continue;

            float cx =
                (gi + 0.5f) * 60.0f;

            float cz =
                (gj + 0.5f) * 60.0f;

            u32 h =
                ((u32)(gi & 0xFFFF) *
                 73856093u) ^
                ((u32)(gj & 0xFFFF) *
                 19349663u);

            if (!h)
                h = 1;

            u32 col =
                palette[h % 10];

            int kind =
                h % 5;

            if (kind == 0)
            {
                add_obj3d(
                    cx,
                    cz,
                    20.0f,
                    60.0f + (h % 40),
                    20.0f,
                    col,
                    1
                );
            }
            else if (kind == 1)
            {
                add_obj3d(
                    cx - 12,
                    cz,
                    18.0f,
                    40.0f + (h % 25),
                    18.0f,
                    col,
                    1
                );

                add_obj3d(
                    cx + 12,
                    cz,
                    18.0f,
                    30.0f + (h % 20),
                    18.0f,
                    palette[(h + 2) % 10],
                    1
                );
            }
            else if (kind == 2)
            {
                add_obj3d(
                    cx - 15,
                    cz - 15,
                    14.0f,
                    18.0f,
                    14.0f,
                    col,
                    0
                );

                add_obj3d(
                    cx + 15,
                    cz - 15,
                    14.0f,
                    22.0f,
                    14.0f,
                    palette[(h + 3) % 10],
                    0
                );

                add_obj3d(
                    cx - 15,
                    cz + 15,
                    14.0f,
                    25.0f,
                    14.0f,
                    palette[(h + 4) % 10],
                    0
                );

                add_obj3d(
                    cx + 15,
                    cz + 15,
                    14.0f,
                    20.0f,
                    14.0f,
                    palette[(h + 5) % 10],
                    0
                );
            }
            else if (kind == 3)
            {
                for (int k = 0; k < 4; k++)
                {
                    float tx =
                        cx +
                        ((k % 2) - 1) *
                        15.0f;

                    float tz =
                        cz +
                        ((k / 2) - 1) *
                        15.0f;

                    add_obj3d(
                        tx,
                        tz,
                        2.0f,
                        8.0f,
                        2.0f,
                        0xFF4A2E1Au,
                        2
                    );

                    add_obj3d(
                        tx,
                        tz,
                        10.0f,
                        10.0f,
                        10.0f,
                        0xFF1A8A20u,
                        3
                    );
                }
            }
            else
            {
                add_obj3d(
                    cx,
                    cz,
                    30.0f,
                    25.0f + (h % 15),
                    25.0f,
                    col,
                    0
                );
            }
        }
    }
}

/* ===================== AI ===================== */

static void init_ai(void)
{
    static const u32 cols[5] =
    {
        NEON_CYAN,
        NEON_MAGENTA,
        NEON_ORANGE,
        NEON_LIME,
        NEON_PINK
    };

    for (int i = 0; i < MAX_AI; i++)
    {
        g_ai[i].axis =
            rng() & 1;

        g_ai[i].dir =
            (rng() & 1)
            ? 1
            : -1;

        g_ai[i].speed =
            20.0f +
            (rng() % 15);

        g_ai[i].color =
            cols[i % 5];

        if (g_ai[i].axis == 0)
        {
            g_ai[i].x =
                -280.0f +
                (rng() % 560);

            g_ai[i].z =
                ((int)(rng() % 8) - 4) *
                60.0f +
                20.0f *
                g_ai[i].dir;

            g_ai[i].heading =
                (g_ai[i].dir > 0)
                ? 1.5708f
                : -1.5708f;
        }
        else
        {
            g_ai[i].x =
                ((int)(rng() % 8) - 4) *
                60.0f +
                20.0f *
                g_ai[i].dir;

            g_ai[i].z =
                -280.0f +
                (rng() % 560);

            g_ai[i].heading =
                (g_ai[i].dir > 0)
                ? 0.0f
                : 3.14159f;
        }
    }
}

static void update_ai(float dt)
{
    for (int i = 0; i < MAX_AI; i++)
    {
        if (g_ai[i].axis == 0)
        {
            g_ai[i].x +=
                g_ai[i].speed *
                g_ai[i].dir *
                dt;

            if (
                g_ai[i].x >
                    p_x + 280 ||
                g_ai[i].x <
                    p_x - 280
            )
            {
                g_ai[i].x =
                    p_x +
                    (
                        g_ai[i].dir > 0
                        ? -280
                        : 280
                    );

                g_ai[i].z =
                    p_z +
                    ((int)(rng() % 8) - 4) *
                    60.0f +
                    20.0f;
            }
        }
        else
        {
            g_ai[i].z +=
                g_ai[i].speed *
                g_ai[i].dir *
                dt;

            if (
                g_ai[i].z >
                    p_z + 280 ||
                g_ai[i].z <
                    p_z - 280
            )
            {
                g_ai[i].z =
                    p_z +
                    (
                        g_ai[i].dir > 0
                        ? -280
                        : 280
                    );

                g_ai[i].x =
                    p_x +
                    ((int)(rng() % 8) - 4) *
                    60.0f +
                    20.0f;
            }
        }
    }
}

/* ===================== PARTICLES ===================== */

static void spawn_particle(
    float x,
    float y,
    float z,
    float vx,
    float vy,
    float vz,
    u32 col,
    int life,
    float size
)
{
    if (g_numParts >= MAX_PAR)
        return;

    Particle *p =
        &g_parts[g_numParts++];

    p->x = x;
    p->y = y;
    p->z = z;

    p->vx = vx;
    p->vy = vy;
    p->vz = vz;

    p->life = life;
    p->max_life = life;

    p->color = col;
    p->size = size;
}

static void update_particles(float dt)
{
    for (
        int i = g_numParts - 1;
        i >= 0;
        i--
    )
    {
        Particle *p =
            &g_parts[i];

        p->x += p->vx * dt;
        p->y += p->vy * dt;
        p->z += p->vz * dt;

        p->vy -=
            6.0f * dt;

        p->life--;

        if (
            p->life <= 0 ||
            p->y < -5.0f
        )
        {
            g_parts[i] =
                g_parts[g_numParts - 1];

            g_numParts--;
        }
    }
}

static void init_stars(void)
{
    g_numStars =
        MAX_STARS;

    for (int i = 0; i < MAX_STARS; i++)
    {
        g_stars[i].x =
            frand(
                0,
                SCR_W
            );

        g_stars[i].y =
            frand(
                0,
                SCR_H * 0.55f
            );

        g_stars[i].z =
            frand(
                -500,
                500
            );

        g_stars[i].brightness =
            (u8)(
                120 +
                (rng() % 135)
            );
    }
}

/* ===================== COLLISION ===================== */

static int hit_any(
    float x,
    float z,
    float r
)
{
    for (int i = 0; i < g_numObjs; i++)
    {
        Obj3D *o =
            &g_objs[i];

        if (
            o->type == 2 ||
            o->type == 3
        )
        {
            continue;
        }

        float hw =
            o->w * 0.5f + r;

        float hd =
            o->d * 0.5f + r;

        if (
            fabsf(x - o->x) < hw &&
            fabsf(z - o->z) < hd
        )
        {
            return 1;
        }
    }

    return 0;
}

static void show_msg(const char *m)
{
    strncpy(
        g_msg,
        m,
        63
    );

    g_msg[63] = 0;

    g_msgTimer = 2.0f;
}

/* ===================== GAME UPDATE ===================== */

static void update_game(float dt)
{
    if (!g_padReady)
        return;

    u16 btn =
        pad_btns();

    int lx =
        pad_lx();

    int ly =
        pad_ly();

    int rx =
        pad_rx();

    int ry =
        pad_ry();

    cam_yaw +=
        (rx / 128.0f) *
        2.8f *
        dt;

    cam_pitch =
        clampf(
            cam_pitch +
            (ry / 128.0f) *
            1.5f *
            dt,
            0.05f,
            1.35f
        );

    if (btn & BTN_R2)
        cam_dist +=
            20.0f * dt;

    if (btn & BTN_L2)
        cam_dist -=
            20.0f * dt;

    cam_dist =
        clampf(
            cam_dist,
            8.0f,
            40.0f
        );

    u8 l3 =
        (btn & BTN_L3)
        ? 1
        : 0;

    if (
        l3 &&
        !g_prevL3
    )
    {
        p_carColor =
            (p_carColor + 1) % 5;

        show_msg(
            "Car color"
        );
    }

    g_prevL3 = l3;

    u8 r3 =
        (btn & BTN_R3)
        ? 1
        : 0;

    if (
        r3 &&
        !g_prevR3
    )
    {
        p_x = 0;
        p_z = 0;
        p_speed = 0;
        p_heading = 0;

        show_msg(
            "Home"
        );
    }

    g_prevR3 = r3;

    u8 st =
        (btn & BTN_START)
        ? 1
        : 0;

    if (
        st &&
        !g_prevStart
    )
    {
        g_state =
            ST_PAUSED;

        g_menuSel = 0;
    }

    g_prevStart = st;

    float thr =
        -(ly / 128.0f);

    float str =
        -(lx / 128.0f);

    if (btn & BTN_UP)
        thr = 1.0f;

    if (btn & BTN_DOWN)
        thr = -1.0f;

    if (btn & BTN_LEFT)
        str = 1.0f;

    if (btn & BTN_RIGHT)
        str = -1.0f;

    if (
        (btn & BTN_R1) &&
        p_boost > 0.0f
    )
    {
        p_speed +=
            55.0f * dt;

        p_boost -=
            0.5f * dt;

        g_shake =
            0.4f;

        float bx =
            p_x -
            sinf(p_heading) *
            3.0f;

        float bz =
            p_z -
            cosf(p_heading) *
            3.0f;

        for (int k = 0; k < 3; k++)
        {
            spawn_particle(
                bx +
                frand(-0.3f, 0.3f),

                0.5f,

                bz +
                frand(-0.3f, 0.3f),

                frand(-0.5f, 0.5f) -
                sinf(p_heading) * 3.0f,

                frand(0.2f, 1.5f),

                frand(-0.5f, 0.5f) -
                cosf(p_heading) * 3.0f,

                (k & 1)
                ? NEON_ORANGE
                : NEON_YELLOW,

                30 + rng() % 20,
                0.8f
            );
        }
    }
    else
    {
        p_boost =
            clampf(
                p_boost +
                0.12f * dt,
                0.0f,
                1.0f
            );
    }

    if (btn & BTN_L1)
    {
        p_speed *=
            (1.0f - 4.0f * dt);

        g_shake =
            0.5f;
    }

    p_speed +=
        thr *
        35.0f *
        dt;

    if (fabsf(thr) < 0.1f)
    {
        p_speed *=
            (1.0f - 1.5f * dt);
    }

    p_speed =
        clampf(
            p_speed,
            -25.0f,
            70.0f
        );

    if (fabsf(p_speed) > 0.5f)
    {
        float sgn =
            p_speed > 0
            ? 1.0f
            : -1.0f;

        float gain =
            clampf(
                fabsf(p_speed) /
                20.0f,
                0.3f,
                1.0f
            );

        p_heading +=
            str *
            2.4f *
            gain *
            dt *
            sgn;
    }

    float vx =
        sinf(p_heading) *
        p_speed *
        dt;

    float vz =
        cosf(p_heading) *
        p_speed *
        dt;

    float nx =
        p_x + vx;

    float nz =
        p_z + vz;

    if (!hit_any(
        nx,
        p_z,
        1.5f
    ))
    {
        p_x = nx;
    }
    else
    {
        p_speed *= 0.2f;
        g_shake = 0.8f;
    }

    if (!hit_any(
        p_x,
        nz,
        1.5f
    ))
    {
        p_z = nz;
    }
    else
    {
        p_speed *= 0.2f;
        g_shake = 0.8f;
    }

    if (
        fabsf(p_speed) > 3.0f &&
        (rng() % 5 == 0)
    )
    {
        float bx =
            p_x -
            sinf(p_heading) *
            2.5f;

        float bz =
            p_z -
            cosf(p_heading) *
            2.5f;

        spawn_particle(
            bx,
            0.3f,
            bz,
            frand(-0.3f, 0.3f),
            0.5f,
            frand(-0.3f, 0.3f),
            0x60888888,
            25,
            1.2f
        );
    }

    if (fabsf(p_speed) > 45.0f)
        g_shake = 0.3f;

    g_shake *=
        (1.0f - 5.0f * dt);

    if (g_shake < 0.01f)
        g_shake = 0.0f;

    g_timeOfDay +=
        dt / 240.0f;

    if (g_timeOfDay > 1.0f)
        g_timeOfDay -= 1.0f;

    if (g_msgTimer > 0.0f)
    {
        g_msgTimer -= dt;

        if (g_msgTimer < 0.0f)
            g_msgTimer = 0.0f;
    }

    update_ai(dt);
    update_particles(dt);
}

/* ===================== WORLD RENDER ===================== */

static void render_world(void)
{
    /*
     * Clear once per frame.
     * Then explicitly use 2D for our screen-space renderer.
     */
    begin_2d();

    tiny3d_Clear(
        DARK_BG,
        TINY3D_CLEAR_ALL
    );

    draw_sky();

    static float last_x = 1000000000.0f;
    static float last_z = 1000000000.0f;

    if (
        fabsf(p_x - last_x) > 25.0f ||
        fabsf(p_z - last_z) > 25.0f
    )
    {
        gen_city();

        last_x = p_x;
        last_z = p_z;
    }

    draw_ground_grid();

    typedef struct
    {
        float dist;
        int idx;
        int is_ai;
    } DI;

    static DI items[MAX_BLD + MAX_AI];

    int n = 0;

    for (int i = 0; i < g_numObjs; i++)
    {
        float dx =
            g_objs[i].x - p_x;

        float dz =
            g_objs[i].z - p_z;

        items[n].dist =
            dx * dx +
            dz * dz;

        items[n].idx =
            i;

        items[n].is_ai =
            0;

        n++;
    }

    for (int i = 0; i < MAX_AI; i++)
    {
        float dx =
            g_ai[i].x - p_x;

        float dz =
            g_ai[i].z - p_z;

        items[n].dist =
            dx * dx +
            dz * dz;

        items[n].idx =
            i;

        items[n].is_ai =
            1;

        n++;
    }

    for (
        int i = 0;
        i < n - 1;
        i++
    )
    {
        for (
            int j = 0;
            j < n - 1 - i;
            j++
        )
        {
            if (
                items[j].dist <
                items[j + 1].dist
            )
            {
                DI t =
                    items[j];

                items[j] =
                    items[j + 1];

                items[j + 1] =
                    t;
            }
        }
    }

    for (int i = 0; i < n; i++)
    {
        if (items[i].is_ai)
        {
            AiCar *c =
                &g_ai[items[i].idx];

            ObjModel *mm =
                &g_carModels[
                    items[i].idx % 5
                ];

            if (mm->loaded)
            {
                obj_draw(
                    mm,
                    c->x,
                    0.0f,
                    c->z,
                    c->heading,
                    1.8f
                );
            }
            else
            {
                draw_glow_3d(
                    c->x,
                    0.8f,
                    c->z,
                    3.0f,
                    c->color
                );
            }

            float hx =
                c->x +
                sinf(c->heading) *
                2.5f;

            float hz =
                c->z +
                cosf(c->heading) *
                2.5f;

            draw_glow_3d(
                hx,
                0.8f,
                hz,
                1.5f,
                NEON_YELLOW
            );
        }
        else
        {
            Obj3D *o =
                &g_objs[
                    items[i].idx
                ];

            if (
                o->type == 2 ||
                o->type == 3
            )
            {
                float hw =
                    o->w * 0.5f;

                float hd =
                    o->d * 0.5f;

                float y0 = 0.0f;
                float y1 = o->h;

                draw_quad_3d(
                    vec3(
                        o->x - hw,
                        y1,
                        o->z - hd
                    ),
                    vec3(
                        o->x + hw,
                        y1,
                        o->z - hd
                    ),
                    vec3(
                        o->x + hw,
                        y1,
                        o->z + hd
                    ),
                    vec3(
                        o->x - hw,
                        y1,
                        o->z + hd
                    ),
                    o->top_color
                );

                draw_quad_3d(
                    vec3(
                        o->x - hw,
                        y0,
                        o->z - hd
                    ),
                    vec3(
                        o->x + hw,
                        y0,
                        o->z - hd
                    ),
                    vec3(
                        o->x + hw,
                        y1,
                        o->z - hd
                    ),
                    vec3(
                        o->x - hw,
                        y1,
                        o->z - hd
                    ),
                    o->color
                );
            }
            else
            {
                ObjModel *bm =
                    &g_bldModels[
                        items[i].idx % 2
                    ];

                if (bm->loaded)
                {
                    obj_draw(
                        bm,
                        o->x,
                        0.0f,
                        o->z,
                        0.0f,
                        o->w * 0.8f
                    );
                }
                else
                {
                    draw_glow_3d(
                        o->x,
                        o->h * 0.5f,
                        o->z,
                        o->w,
                        o->color
                    );
                }
            }
        }
    }

    /* PLAYER */

    {
        ObjModel *pm =
            &g_carModels[
                p_carColor % 5
            ];

        if (pm->loaded)
        {
            obj_draw(
                pm,
                p_x,
                0.0f,
                p_z,
                p_heading,
                2.0f
            );
        }
        else
        {
            static const u32 cols[5] =
            {
                NEON_CYAN,
                NEON_MAGENTA,
                NEON_ORANGE,
                NEON_LIME,
                NEON_PINK
            };

            draw_glow_3d(
                p_x,
                1.0f,
                p_z,
                5.0f,
                cols[
                    p_carColor % 5
                ]
            );
        }

        float fx =
            p_x +
            sinf(p_heading) *
            2.8f;

        float fz =
            p_z +
            cosf(p_heading) *
            2.8f;

        draw_glow_3d(
            fx -
            cosf(p_heading) *
            0.7f,
            0.8f,
            fz +
            sinf(p_heading) *
            0.7f,
            2.0f,
            NEON_YELLOW
        );

        draw_glow_3d(
            fx +
            cosf(p_heading) *
            0.7f,
            0.8f,
            fz -
            sinf(p_heading) *
            0.7f,
            2.0f,
            NEON_YELLOW
        );
    }

    /* PARTICLES */

    for (int i = 0; i < g_numParts; i++)
    {
        Particle *p =
            &g_parts[i];

        SP sp =
            project(
                vec3(
                    p->x,
                    p->y,
                    p->z
                )
            );

        if (!sp.vis)
            continue;

        float sz =
            p->size /
            sp.z *
            FOV;

        if (sz < 2)
            sz = 2;

        if (sz > 40)
            sz = 40;

        u8 a =
            (u8)(
                (float)p->life /
                (float)p->max_life *
                200
            );

        u32 c =
            ((u32)a << 24) |
            (p->color & 0x00FFFFFF);

        draw_rect2d(
            sp.x - sz * 0.5f,
            sp.y - sz * 0.5f,
            sz,
            sz,
            c
        );
    }
}

/* ===================== HUD ===================== */

static void draw_hud(void)
{
    char buf[128];

    begin_2d();

    draw_rect2d(
        15,
        15,
        260,
        70,
        0x80000000u
    );

    draw_rect2d(
        15,
        15,
        260,
        3,
        NEON_CYAN
    );

    SetFontSize(28, 28);
    SetFontColor(
        NEON_CYAN,
        0x00000000
    );

    DrawString(
        25,
        45,
        (char*)"NEON CITY"
    );

    SetFontSize(12, 12);
    SetFontColor(
        NEON_MAGENTA,
        0x00000000
    );

    DrawString(
        27,
        68,
        (char*)"U L T R A   v 5"
    );

    draw_rect2d(
        15,
        SCR_H - 130,
        240,
        115,
        0x80000000u
    );

    draw_rect2d(
        15,
        SCR_H - 130,
        3,
        115,
        NEON_YELLOW
    );

    sprintf(
        buf,
        "%d",
        (int)(
            fabsf(p_speed) *
            5.4f
        )
    );

    SetFontSize(44, 44);

    SetFontColor(
        NEON_YELLOW,
        0x00000000
    );

    DrawString(
        30,
        SCR_H - 95,
        buf
    );

    SetFontSize(14, 14);

    SetFontColor(
        0xFFFFE080u,
        0x00000000
    );

    DrawString(
        140,
        SCR_H - 95,
        (char*)"KM/H"
    );

    SetFontSize(12, 12);

    SetFontColor(
        NEON_ORANGE,
        0x00000000
    );

    DrawString(
        30,
        SCR_H - 60,
        (char*)"BOOST"
    );

    draw_rect2d(
        90,
        SCR_H - 63,
        150,
        12,
        0xFF222222u
    );

    draw_rect2d(
        90,
        SCR_H - 63,
        p_boost * 150.0f,
        12,
        NEON_ORANGE
    );

    SetFontSize(14, 14);

    SetFontColor(
        NEON_LIME,
        0x00000000
    );

    sprintf(
        buf,
        "SCORE: %d",
        g_score
    );

    DrawString(
        30,
        SCR_H - 35,
        buf
    );

    if (g_msgTimer > 0.0f)
    {
        int w =
            (int)strlen(g_msg) *
            12;

        draw_rect2d(
            SCR_W / 2 - w / 2 - 20,
            110,
            w + 40,
            40,
            0x80000000u
        );

        SetFontSize(22, 22);

        SetFontColor(
            0xFFFFFFFFu,
            0x00000000
        );

        DrawString(
            SCR_W / 2 - w / 2,
            138,
            g_msg
        );
    }

    /* MINIMAP */

    {
        int mx =
            SCR_W - 210;

        int my = 20;
        int ms = 190;

        draw_rect2d(
            mx - 3,
            my - 3,
            ms + 6,
            ms + 6,
            NEON_CYAN
        );

        draw_rect2d(
            mx,
            my,
            ms,
            ms,
            0xCC000818u
        );

        float sc =
            (float)ms /
            320.0f;

        float cxm =
            mx +
            ms * 0.5f;

        float cym =
            my +
            ms * 0.5f;

        for (int i = 0; i < g_numObjs; i++)
        {
            Obj3D *o =
                &g_objs[i];

            float dx =
                (o->x - p_x) *
                sc;

            float dz =
                (o->z - p_z) *
                sc;

            if (
                fabsf(dx) >
                    ms * 0.5f ||
                fabsf(dz) >
                    ms * 0.5f
            )
            {
                continue;
            }

            float hs =
                o->w *
                0.5f *
                sc;

            if (hs < 1.5f)
                hs = 1.5f;

            draw_rect2d(
                cxm + dx - hs,
                cym + dz - hs,
                hs * 2,
                hs * 2,
                o->color
            );
        }

        for (int i = 0; i < MAX_AI; i++)
        {
            float dx =
                (g_ai[i].x - p_x) *
                sc;

            float dz =
                (g_ai[i].z - p_z) *
                sc;

            if (
                fabsf(dx) >
                    ms * 0.5f ||
                fabsf(dz) >
                    ms * 0.5f
            )
            {
                continue;
            }

            draw_rect2d(
                cxm + dx - 2,
                cym + dz - 2,
                4,
                4,
                g_ai[i].color
            );
        }

        draw_rect2d(
            cxm - 5,
            cym - 5,
            10,
            10,
            NEON_YELLOW
        );

        draw_rect2d(
            cxm - 3,
            cym - 3,
            6,
            6,
            0xFFFFFFFFu
        );
    }

    int hours =
        (int)(
            g_timeOfDay *
            24.0f +
            6.0f
        ) % 24;

    int minutes =
        (int)(
            (
                (
                    g_timeOfDay *
                    24.0f +
                    6.0f
                ) -
                (float)hours
            ) *
            60.0f
        );

    sprintf(
        buf,
        "%02d:%02d",
        hours,
        minutes
    );

    draw_rect2d(
        SCR_W - 130,
        SCR_H - 55,
        100,
        30,
        0x80000000u
    );

    SetFontSize(20, 20);

    SetFontColor(
        0xFFFFE080u,
        0x00000000
    );

    DrawString(
        SCR_W - 125,
        SCR_H - 32,
        buf
    );

    SetFontSize(11, 11);

    SetFontColor(
        0xFFBBBBBBu,
        0x00000000
    );

    DrawString(
        24,
        SCR_H - 8,
        (char*)
        "L:drive  R:cam(360)  R1:nitro  L1:brake  L3:color  R3:reset  START:pause"
    );
}

/* ===================== LOADING ===================== */

static void draw_loading(float dt)
{
    g_loadProgress +=
        dt * 0.4f;

    if (g_loadProgress > 1.0f)
        g_loadProgress = 1.0f;

    begin_2d();

    tiny3d_Clear(
        0xFF03030Au,
        TINY3D_CLEAR_ALL
    );

    for (int i = 0; i < 25; i++)
    {
        int y =
            30 + i * 28;

        u8 a =
            (u8)(
                10 +
                (i % 5) * 6
            );

        draw_rect2d(
            0,
            y,
            SCR_W,
            1,
            ((u32)a << 24) |
            (GRID_LINE & 0x00FFFFFF)
        );
    }

    for (
        int layer = 6;
        layer >= 1;
        layer--
    )
    {
        float r =
            180.0f +
            layer * 20.0f;

        u8 a =
            (u8)(
                8 +
                (6 - layer) * 8
            );

        u32 c =
            ((u32)a << 24) |
            (NEON_CYAN & 0x00FFFFFF);

        for (
            int dy = -(int)r;
            dy <= (int)r;
            dy += 4
        )
        {
            float inside =
                r * r -
                (float)(dy * dy);

            if (inside < 0)
                continue;

            float w =
                sqrtf(inside);

            draw_rect2d(
                SCR_W / 2 - w,
                200 + dy,
                w * 2,
                4,
                c
            );
        }
    }

    SetFontSize(80, 80);

    SetFontColor(
        NEON_CYAN,
        0x00000000
    );

    DrawString(
        SCR_W / 2 - 280,
        220,
        (char*)"NEON CITY"
    );

    SetFontSize(28, 28);

    SetFontColor(
        NEON_MAGENTA,
        0x00000000
    );

    DrawString(
        SCR_W / 2 - 100,
        300,
        (char*)"U L T R A   v 5"
    );

    int bw = 600;

    int bx =
        SCR_W / 2 -
        bw / 2;

    int by = 480;

    draw_rect2d(
        bx - 3,
        by - 3,
        bw + 6,
        36,
        NEON_CYAN
    );

    draw_rect2d(
        bx,
        by,
        bw,
        30,
        0xFF111122u
    );

    int fill =
        (int)(
            g_loadProgress *
            bw
        );

    draw_rect2d(
        bx,
        by,
        fill,
        30,
        NEON_MAGENTA
    );

    draw_rect2d(
        bx,
        by,
        fill,
        8,
        NEON_PINK
    );

    char buf[64];

    sprintf(
        buf,
        "%d%%",
        (int)(
            g_loadProgress *
            100.0f
        )
    );

    SetFontSize(20, 20);

    SetFontColor(
        0xFFFFFFFFu,
        0x00000000
    );

    DrawString(
        SCR_W / 2 - 20,
        by + 25,
        buf
    );

    SetFontSize(14, 14);

    SetFontColor(
        0xFFAAAAAAu,
        0x00000000
    );

    DrawString(
        SCR_W / 2 - 130,
        570,
        (char*)"Loading models..."
    );

    if (g_loadProgress >= 1.0f)
    {
        g_state =
            ST_MENU;

        g_menuSel = 0;
        g_menuTimer = 0.0f;
    }
}

/* ===================== MENU ===================== */

static void draw_menu(float dt)
{
    g_menuTimer += dt;

    begin_2d();

    tiny3d_Clear(
        0xFF03030Au,
        TINY3D_CLEAR_ALL
    );

    for (
        int i = -20;
        i <= 20;
        i++
    )
    {
        float x1 =
            SCR_W / 2.0f +
            i * 30;

        float x2 =
            SCR_W / 2.0f +
            i * 200;

        for (int t = 0; t < 12; t++)
        {
            float y0 =
                400 +
                t * t * 2.5f;

            float y1 =
                400 +
                (t + 1) *
                (t + 1) *
                2.5f;

            if (y0 >= SCR_H)
                break;

            float xa =
                x1 +
                (x2 - x1) *
                (float)t /
                12.0f;

            draw_rect2d(
                xa,
                y0,
                1,
                y1 - y0,
                0x3000AAFFu
            );
        }
    }

    for (int i = 0; i < 12; i++)
    {
        float y =
            400 +
            i * i * 2.5f;

        if (y >= SCR_H)
            break;

        draw_rect2d(
            0,
            y,
            SCR_W,
            1,
            0x3000D4FFu
        );
    }

    for (int i = 0; i < g_numStars; i += 3)
    {
        int sx =
            (int)g_stars[i].x;

        int sy =
            (int)g_stars[i].y;

        u8 b =
            g_stars[i].brightness;

        draw_rect2d(
            sx,
            sy,
            2,
            2,
            0xFF000000u |
            ((u32)b << 16) |
            ((u32)b << 8) |
            b
        );
    }

    SetFontSize(90, 90);

    SetFontColor(
        NEON_CYAN,
        0x00000000
    );

    DrawString(
        SCR_W / 2 - 310,
        210,
        (char*)"NEON CITY"
    );

    SetFontSize(28, 28);

    SetFontColor(
        NEON_MAGENTA,
        0x00000000
    );

    DrawString(
        SCR_W / 2 - 110,
        290,
        (char*)"U L T R A   v 5"
    );

    const char *items[] =
    {
        "START GAME",
        "INSTRUCTIONS",
        "EXIT"
    };

    for (int i = 0; i < 3; i++)
    {
        int y =
            430 + i * 70;

        int x =
            SCR_W / 2 - 180;

        int selected =
            (i == g_menuSel);

        if (selected)
        {
            float pulse =
                0.5f +
                sinf(
                    g_menuTimer * 4.0f
                ) *
                0.5f;

            u8 a =
                (u8)(
                    120 +
                    pulse * 100
                );

            u32 bg =
                ((u32)a << 24) |
                (NEON_CYAN & 0x00FFFFFF);

            draw_rect2d(
                x - 20,
                y - 20,
                400,
                60,
                bg
            );

            draw_rect2d(
                x - 20,
                y - 20,
                400,
                3,
                NEON_YELLOW
            );

            draw_rect2d(
                x - 20,
                y + 37,
                400,
                3,
                NEON_YELLOW
            );
        }
        else
        {
            draw_rect2d(
                x - 20,
                y - 20,
                400,
                60,
                0x40000000u
            );
        }

        SetFontSize(32, 32);

        SetFontColor(
            selected
            ? 0xFFFFFFFFu
            : 0xFFAAAAAAu,
            0x00000000
        );

        DrawString(
            x,
            y + 14,
            (char*)items[i]
        );
    }

    SetFontSize(12, 12);

    SetFontColor(
        0xFF888888u,
        0x00000000
    );

    DrawString(
        SCR_W / 2 - 180,
        SCR_H - 40,
        (char*)
        "D-PAD: navigate   CROSS: select"
    );
}

/* ===================== PAUSE ===================== */

static void draw_pause(void)
{
    begin_2d();

    draw_rect2d(
        0,
        0,
        SCR_W,
        SCR_H,
        0xA0000000u
    );

    int x =
        SCR_W / 2 - 250;

    draw_rect2d(
        x,
        180,
        500,
        360,
        0xFF101820u
    );

    draw_rect2d(
        x,
        180,
        500,
        4,
        NEON_CYAN
    );

    draw_rect2d(
        x,
        536,
        500,
        4,
        NEON_CYAN
    );

    SetFontSize(40, 40);

    SetFontColor(
        NEON_CYAN,
        0x00000000
    );

    DrawString(
        x + 130,
        240,
        (char*)"PAUSED"
    );

    const char *items[] =
    {
        "RESUME",
        "QUIT TO MENU"
    };

    for (int i = 0; i < 2; i++)
    {
        int y =
            320 + i * 70;

        int selected =
            (i == g_menuSel);

        if (selected)
        {
            draw_rect2d(
                x + 40,
                y - 15,
                420,
                55,
                0x6000D4FFu
            );

            draw_rect2d(
                x + 40,
                y - 15,
                420,
                2,
                NEON_YELLOW
            );
        }

        SetFontSize(28, 28);

        SetFontColor(
            selected
            ? 0xFFFFFFFFu
            : 0xFFAAAAAAu,
            0x00000000
        );

        DrawString(
            x + 60,
            y + 18,
            (char*)items[i]
        );
    }
}

/* ===================== MENU INPUT ===================== */

static void update_menu(float dt)
{
    (void)dt;

    if (!g_padReady)
        return;

    u16 btn =
        pad_btns();

    u8 up =
        (btn & BTN_UP)
        ? 1
        : 0;

    u8 down =
        (btn & BTN_DOWN)
        ? 1
        : 0;

    u8 cross =
        (btn & BTN_CROSS)
        ? 1
        : 0;

    if (
        up &&
        !g_prevUp
    )
    {
        g_menuSel--;

        if (g_menuSel < 0)
            g_menuSel = 2;
    }

    g_prevUp = up;

    if (
        down &&
        !g_prevDown
    )
    {
        g_menuSel++;

        if (g_menuSel > 2)
            g_menuSel = 0;
    }

    g_prevDown = down;

    if (
        cross &&
        !g_prevCross
    )
    {
        if (g_menuSel == 0)
        {
            p_x = 0;
            p_z = 0;
            p_heading = 0;
            p_speed = 0;
            p_boost = 1.0f;

            g_score = 0;

            gen_city();
            init_ai();

            g_numParts = 0;

            show_msg(
                "Welcome!"
            );

            g_state =
                ST_PLAYING;
        }
        else if (g_menuSel == 1)
        {
            show_msg(
                "L-Stick: drive | R-Stick: 360 cam"
            );

            g_msgTimer = 4.0f;
        }
        else
        {
            g_running = 0;
        }
    }

    g_prevCross = cross;
}

static void update_pause(float dt)
{
    (void)dt;

    if (!g_padReady)
        return;

    u16 btn =
        pad_btns();

    u8 up =
        (btn & BTN_UP)
        ? 1
        : 0;

    u8 down =
        (btn & BTN_DOWN)
        ? 1
        : 0;

    u8 cross =
        (btn & BTN_CROSS)
        ? 1
        : 0;

    u8 st =
        (btn & BTN_START)
        ? 1
        : 0;

    if (
        up &&
        !g_prevUp
    )
    {
        g_menuSel--;

        if (g_menuSel < 0)
            g_menuSel = 1;
    }

    g_prevUp = up;

    if (
        down &&
        !g_prevDown
    )
    {
        g_menuSel++;

        if (g_menuSel > 1)
            g_menuSel = 0;
    }

    g_prevDown = down;

    if (
        st &&
        !g_prevStart
    )
    {
        g_state =
            ST_PLAYING;

        g_prevStart = st;

        return;
    }

    g_prevStart = st;

    if (
        cross &&
        !g_prevCross
    )
    {
        if (g_menuSel == 0)
        {
            g_state =
                ST_PLAYING;
        }
        else
        {
            g_state =
                ST_MENU;

            g_menuSel = 0;
        }
    }

    g_prevCross = cross;
}

/* ===================== SYSUTIL ===================== */

static void sysutil_cb(
    u64 s,
    u64 p,
    void *u
)
{
    (void)p;
    (void)u;

    if (s == SYSUTIL_EXIT_GAME)
        g_running = 0;
}

/* ===================== MODEL LOADING ===================== */

static void load_models(void)
{
    const char *carPaths[5] =
    {
        "models/sedan.obj",
        "models/suv.obj",
        "models/race.obj",
        "models/taxi.obj",
        "models/truck.obj"
    };

    const char *bldPaths[2] =
    {
        "models/build1.obj",
        "models/build2.obj"
    };

    for (int i = 0; i < 5; i++)
    {
        memset(
            &g_carModels[i],
            0,
            sizeof(ObjModel)
        );

        if (
            !obj_load(
                carPaths[i],
                &g_carModels[i]
            )
        )
        {
            printf(
                "[NEON CITY] Car model failed: %s\n",
                carPaths[i]
            );
        }
        else
        {
            printf(
                "[NEON CITY] Car model loaded: %s\n",
                carPaths[i]
            );
        }
    }

    for (int i = 0; i < 2; i++)
    {
        memset(
            &g_bldModels[i],
            0,
            sizeof(ObjModel)
        );

        if (
            !obj_load(
                bldPaths[i],
                &g_bldModels[i]
            )
        )
        {
            printf(
                "[NEON CITY] Building failed: %s\n",
                bldPaths[i]
            );
        }
        else
        {
            printf(
                "[NEON CITY] Building loaded: %s\n",
                bldPaths[i]
            );
        }
    }
}

/* ===================== MAIN ===================== */

int main(
    int argc,
    char *argv[]
)
{
    (void)argc;
    (void)argv;

    /*
     * System modules
     */
    sysModuleLoad(
        SYSMODULE_FS
    );

    /*
     * Controller
     */
    ioPadInit(7);

    sysUtilRegisterCallback(
        SYSUTIL_EVENT_SLOT0,
        sysutil_cb,
        NULL
    );

    /*
     * Tiny3D
     */
    if (
        tiny3d_Init(
            1024 * 1024
        ) != 0
    )
    {
        ioPadEnd();
        return 1;
    }

    /*
     * Font reset
     */
    ResetFont();

    /*
     * Assets
     */
    assets_init();

    /*
     * Models
     */
    load_models();

    /*
     * World
     */
    init_stars();

    gen_city();

    /*
     * Random seed
     */
    {
        struct timeval tv;

        gettimeofday(
            &tv,
            NULL
        );

        rng_s =
            (unsigned int)
            tv.tv_usec;

        if (rng_s == 0)
            rng_s =
                0xC0FFEE42u;
    }

    init_ai();

    /*
     * Initial state
     */
    g_state =
        ST_LOADING;

    g_loadProgress =
        0.0f;

    /*
     * Main loop
     */
    while (g_running)
    {
        sysUtilCheckCallback();

        /*
         * Read controller
         */
        g_padReady = 0;

        if (
            ioPadGetInfo(
                &g_padInfo
            ) == 0 &&
            g_padInfo.status[0]
        )
        {
            if (
                ioPadGetData(
                    0,
                    &g_padData
                ) == 0
            )
            {
                g_padReady = 1;
            }
        }

        /*
         * Fixed timestep.
         */
        float dt =
            1.0f / 60.0f;

        /*
         * Game state
         */
        switch (g_state)
        {
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

            default:
                g_state =
                    ST_MENU;
                break;
        }

        /*
         * Present frame.
         */
        tiny3d_Flip();
    }

    /*
     * Shutdown
     */
    ioPadEnd();

    tiny3d_Exit();

    return 0;
}
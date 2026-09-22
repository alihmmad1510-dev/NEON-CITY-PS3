/* =====================================================================
 *  FILE: src/main.c
 *  PROJECT: NEON CITY ULTRA v5
 *  DESCRIPTION: Stable PS3/Tiny3D main loop
 * ===================================================================== */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include <ppu-types.h>
#include <sys/process.h>
#include <sysutil/sysutil.h>
#include <sysmodule/sysmodule.h>
#include <io/pad.h>
#include <sys/time.h>

#include <tiny3d.h>
#include <libfont.h>

#include "obj_loader.h"

/* ================================================================
 * EXTERNALS
 * ================================================================ */

void assets_init(void);
u32 assets_count(void);

SYS_PROCESS_PARAM(1001, 0x100000)

/* ================================================================
 * CONFIG
 * ================================================================ */

#define SCR_W 1280
#define SCR_H 720

#define FOV 700.0f

#define MAX_BLD   200
#define MAX_AI    20
#define MAX_PAR   180
#define MAX_STARS 200

/* ================================================================
 * COLORS
 * ================================================================ */

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

/* ================================================================
 * TYPES
 * ================================================================ */

typedef struct
{
    float x;
    float y;
    float z;
} Vec3;

typedef struct
{
    float x;
    float y;
    float w;
    float h;
    int vis;
    float z;
} SP;

typedef enum
{
    ST_LOADING = 0,
    ST_MENU,
    ST_PLAYING,
    ST_PAUSED
} GameState;

typedef struct
{
    float x;
    float z;

    float w;
    float h;
    float d;

    u32 color;
    u32 top_color;
    u32 glow_color;

    int type;
} Obj3D;

typedef struct
{
    float x;
    float z;
    float heading;
    float speed;

    int axis;
    int dir;

    u32 color;
} AiCar;

typedef struct
{
    float x;
    float y;
    float z;

    float vx;
    float vy;
    float vz;

    int life;
    int max_life;

    u32 color;
    float size;
} Particle;

typedef struct
{
    float x;
    float y;
    float z;

    u8 brightness;
} Star;

/* ================================================================
 * GAME STATE
 * ================================================================ */

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

/* ================================================================
 * PLAYER
 * ================================================================ */

static float p_x = 0.0f;
static float p_z = 0.0f;

static float p_heading = 0.0f;
static float p_speed = 0.0f;

static float p_boost = 1.0f;

static int p_carColor = 0;

/* ================================================================
 * CAMERA
 * ================================================================ */

static float cam_yaw = 0.0f;
static float cam_pitch = 0.45f;
static float cam_dist = 20.0f;

static float cam_target_y = 1.5f;

/* ================================================================
 * GAME
 * ================================================================ */

static float g_timeOfDay = 0.25f;

static int g_score = 0;

static float g_shake = 0.0f;

static float g_msgTimer = 0.0f;

static char g_msg[64] = "";

/* ================================================================
 * WORLD
 * ================================================================ */

static Obj3D g_objs[MAX_BLD];
static int g_numObjs = 0;

static AiCar g_ai[MAX_AI];

static Particle g_parts[MAX_PAR];
static int g_numParts = 0;

static Star g_stars[MAX_STARS];
static int g_numStars = 0;

/* ================================================================
 * MODELS
 * ================================================================ */

static ObjModel g_carModels[5];
static ObjModel g_bldModels[2];

/* ================================================================
 * CONTROLLER
 * ================================================================ */

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

    return (u16)(
        p[2] |
        (p[3] << 8)
    );
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

/* ================================================================
 * RNG
 * ================================================================ */

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
    return (rng() & 0xFFFFFF) /
           (float)0xFFFFFF;
}

static float frand(float a, float b)
{
    return a +
           frand01() *
           (b - a);
}

static float clampf(
    float v,
    float a,
    float b
)
{
    if (v < a)
        return a;

    if (v > b)
        return b;

    return v;
}

/* ================================================================
 * VECTOR
 * ================================================================ */

static inline Vec3 vec3(
    float x,
    float y,
    float z
)
{
    Vec3 v;

    v.x = x;
    v.y = y;
    v.z = z;

    return v;
}

static inline Vec3 vec3_sub(
    Vec3 a,
    Vec3 b
)
{
    return vec3(
        a.x - b.x,
        a.y - b.y,
        a.z - b.z
    );
}

static inline float vec3_dot(
    Vec3 a,
    Vec3 b
)
{
    return
        a.x * b.x +
        a.y * b.y +
        a.z * b.z;
}

static inline Vec3 vec3_cross(
    Vec3 a,
    Vec3 b
)
{
    return vec3(
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x
    );
}

static inline Vec3 vec3_norm(Vec3 v)
{
    float l;

    l = sqrtf(
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

/* ================================================================
 * CAMERA
 * ================================================================ */

static Vec3 camera_pos(void)
{
    float cp;
    float sp;

    cp = cosf(cam_pitch);
    sp = sinf(cam_pitch);

    return vec3(
        p_x -
            sinf(cam_yaw) *
            cam_dist *
            cp,

        cam_target_y +
            sp *
            cam_dist,

        p_z -
            cosf(cam_yaw) *
            cam_dist *
            cp
    );
}

static SP project(Vec3 wp)
{
    SP out;

    Vec3 cam;
    Vec3 rel;

    Vec3 target;
    Vec3 forward;
    Vec3 world_up;
    Vec3 right;
    Vec3 up;

    float rx;
    float ry;
    float rz;

    memset(
        &out,
        0,
        sizeof(out)
    );

    cam = camera_pos();

    rel = vec3_sub(
        wp,
        cam
    );

    target = vec3(
        p_x,
        cam_target_y,
        p_z
    );

    forward = vec3_norm(
        vec3_sub(
            target,
            cam
        )
    );

    world_up = vec3(
        0,
        1,
        0
    );

    right = vec3_norm(
        vec3_cross(
            forward,
            world_up
        )
    );

    up = vec3_cross(
        right,
        forward
    );

    rx = vec3_dot(
        rel,
        right
    );

    ry = vec3_dot(
        rel,
        up
    );

    rz = vec3_dot(
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
    SP sp;

    sp = project(
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

/* ================================================================
 * DRAW 2D
 * ================================================================ */

static void draw_rect2d(
    float x,
    float y,
    float w,
    float h,
    u32 col
)
{
    tiny3d_SetPolygon(
        TINY3D_QUADS
    );

    tiny3d_VertexPos(
        x,
        y,
        0.0f
    );

    tiny3d_VertexColor(col);

    tiny3d_VertexPos(
        x + w,
        y,
        0.0f
    );

    tiny3d_VertexPos(
        x + w,
        y + h,
        0.0f
    );

    tiny3d_VertexPos(
        x,
        y + h,
        0.0f
    );

    tiny3d_End();
}

/* ================================================================
 * DRAW 3D QUAD
 * ================================================================ */

static void draw_quad_3d(
    Vec3 a,
    Vec3 b,
    Vec3 c,
    Vec3 d,
    u32 color
)
{
    SP pa;
    SP pb;
    SP pc;
    SP pd;

    pa = project(a);
    pb = project(b);
    pc = project(c);
    pd = project(d);

    if (!pa.vis ||
        !pb.vis ||
        !pc.vis ||
        !pd.vis)
    {
        return;
    }

    tiny3d_SetPolygon(
        TINY3D_QUADS
    );

    tiny3d_VertexPos(
        pa.x,
        pa.y,
        pa.z
    );

    tiny3d_VertexColor(color);

    tiny3d_VertexPos(
        pb.x,
        pb.y,
        pb.z
    );

    tiny3d_VertexPos(
        pc.x,
        pc.y,
        pc.z
    );

    tiny3d_VertexPos(
        pd.x,
        pd.y,
        pd.z
    );

    tiny3d_End();
}

/* ================================================================
 * GLOW
 * ================================================================ */

static void draw_glow_3d(
    float wx,
    float wy,
    float wz,
    float radius,
    u32 color
)
{
    SP sp;

    float r;
    int layer;

    sp = project(
        vec3(
            wx,
            wy,
            wz
        )
    );

    if (!sp.vis)
        return;

    r =
        radius /
        sp.z *
        FOV;

    if (r < 3.0f)
        r = 3.0f;

    if (r > 150.0f)
        r = 150.0f;

    for (layer = 4;
         layer >= 1;
         layer--)
    {
        float lr;
        int dy;

        lr =
            r *
            (float)layer /
            4.0f;

        for (
            dy = -(int)lr;
            dy <= (int)lr;
            dy += 5
        )
        {
            float w;

            w = sqrtf(
                lr * lr -
                (float)(dy * dy)
            );

            draw_rect2d(
                sp.x - w,
                sp.y + dy,
                w * 2.0f,
                5.0f,
                color
            );
        }
    }
}

/* ================================================================
 * GROUND
 * ================================================================ */

static void draw_ground_grid(void)
{
    int bI;
    int bJ;

    float cell;
    int R;

    int i;
    int j;

    bI =
        (int)(p_x / 60.0f);

    bJ =
        (int)(p_z / 60.0f);

    cell = 60.0f;
    R = 5;

    for (
        i = -R;
        i <= R;
        i++
    )
    {
        for (
            j = -R;
            j <= R;
            j++
        )
        {
            float gx;
            float gz;
            float hc;

            gx =
                (bI + i) *
                cell;

            gz =
                (bJ + j) *
                cell;

            hc =
                cell *
                0.5f -
                0.5f;

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
        i = -R;
        i <= R + 1;
        i++
    )
    {
        float gx;

        gx =
            (bI + i) *
            cell -
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
        j = -R;
        j <= R + 1;
        j++
    )
    {
        float gz;

        gz =
            (bJ + j) *
            cell -
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

/* ================================================================
 * SKY
 * ================================================================ */

static void draw_sky(void)
{
    tiny3d_Clear(
        DARK_BG,
        TINY3D_CLEAR_ALL
    );

    tiny3d_Project2D();

    draw_rect2d(
        0,
        0,
        SCR_W,
        SCR_H,
        0xFF050515u
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
}

/* ================================================================
 * CITY
 * ================================================================ */

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
    Obj3D *o;
    u8 r;
    u8 g;
    u8 b;

    if (g_numObjs >= MAX_BLD)
        return;

    o =
        &g_objs[g_numObjs++];

    o->x = x;
    o->z = z;

    o->w = w;
    o->h = h;
    o->d = d;

    o->color = col;

    r = (u8)((col >> 16) & 0xFF);
    g = (u8)((col >> 8) & 0xFF);
    b = (u8)(col & 0xFF);

    o->top_color =
        0xFF000000u |
        ((u32)(r < 230 ? r + 25 : 255) << 16) |
        ((u32)(g < 230 ? g + 25 : 255) << 8) |
        (u32)(b < 230 ? b + 25 : 255);

    o->glow_color =
        0xFF000000u |
        ((u32)(r / 2) << 16) |
        ((u32)(g / 2) << 8) |
        (u32)(b / 2);

    o->type = type;
}

static void gen_city(void)
{
    int bI;
    int bJ;

    int i;
    int j;

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

    g_numObjs = 0;

    bI =
        (int)(p_x / 60.0f);

    bJ =
        (int)(p_z / 60.0f);

    for (
        i = -4;
        i <= 4;
        i++
    )
    {
        for (
            j = -4;
            j <= 4;
            j++
        )
        {
            int gi;
            int gj;

            float cx;
            float cz;

            u32 h;
            u32 col;

            int kind;

            gi = bI + i;
            gj = bJ + j;

            if (gi == 0 &&
                gj == 0)
            {
                continue;
            }

            cx =
                (gi + 0.5f) *
                60.0f;

            cz =
                (gj + 0.5f) *
                60.0f;

            h =
                ((u32)(gi & 0xFFFF) *
                 73856093u) ^
                ((u32)(gj & 0xFFFF) *
                 19349663u);

            if (!h)
                h = 1;

            col =
                palette[h % 10];

            kind =
                h % 5;

            if (kind == 0)
            {
                add_obj3d(
                    cx,
                    cz,
                    20,
                    60 + (h % 40),
                    20,
                    col,
                    1
                );
            }
            else if (kind == 1)
            {
                add_obj3d(
                    cx - 12,
                    cz,
                    18,
                    40 + (h % 25),
                    18,
                    col,
                    1
                );

                add_obj3d(
                    cx + 12,
                    cz,
                    18,
                    30 + (h % 20),
                    18,
                    palette[(h + 2) % 10],
                    1
                );
            }
            else if (kind == 2)
            {
                add_obj3d(
                    cx - 15,
                    cz - 15,
                    14,
                    18,
                    14,
                    col,
                    0
                );

                add_obj3d(
                    cx + 15,
                    cz - 15,
                    14,
                    22,
                    14,
                    palette[(h + 3) % 10],
                    0
                );

                add_obj3d(
                    cx - 15,
                    cz + 15,
                    14,
                    25,
                    14,
                    palette[(h + 4) % 10],
                    0
                );

                add_obj3d(
                    cx + 15,
                    cz + 15,
                    14,
                    20,
                    14,
                    palette[(h + 5) % 10],
                    0
                );
            }
            else
            {
                add_obj3d(
                    cx,
                    cz,
                    30,
                    25 + (h % 15),
                    25,
                    col,
                    0
                );
            }
        }
    }
}

/* ================================================================
 * AI
 * ================================================================ */

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

    int i;

    for (
        i = 0;
        i < MAX_AI;
        i++
    )
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
                g_ai[i].dir > 0
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
                g_ai[i].dir > 0
                ? 0.0f
                : 3.14159f;
        }
    }
}

static void update_ai(float dt)
{
    int i;

    for (
        i = 0;
        i < MAX_AI;
        i++
    )
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
                    (g_ai[i].dir > 0
                        ? -280
                        : 280);

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
                    (g_ai[i].dir > 0
                        ? -280
                        : 280);

                g_ai[i].x =
                    p_x +
                    ((int)(rng() % 8) - 4) *
                    60.0f +
                    20.0f;
            }
        }
    }
}

/* ================================================================
 * PARTICLES
 * ================================================================ */

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
    Particle *p;

    if (g_numParts >= MAX_PAR)
        return;

    p =
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
    int i;

    for (
        i = g_numParts - 1;
        i >= 0;
        i--
    )
    {
        Particle *p =
            &g_parts[i];

        p->x +=
            p->vx * dt;

        p->y +=
            p->vy * dt;

        p->z +=
            p->vz * dt;

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

/* ================================================================
 * STARS
 * ================================================================ */

static void init_stars(void)
{
    int i;

    g_numStars =
        MAX_STARS;

    for (
        i = 0;
        i < MAX_STARS;
        i++
    )
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

/* ================================================================
 * COLLISION
 * ================================================================ */

static int hit_any(
    float x,
    float z,
    float r
)
{
    int i;

    for (
        i = 0;
        i < g_numObjs;
        i++
    )
    {
        Obj3D *o =
            &g_objs[i];

        float hw;
        float hd;

        if (
            o->type == 2 ||
            o->type == 3
        )
        {
            continue;
        }

        hw =
            o->w * 0.5f +
            r;

        hd =
            o->d * 0.5f +
            r;

        if (
            fabsf(
                x - o->x
            ) < hw &&
            fabsf(
                z - o->z
            ) < hd
        )
        {
            return 1;
        }
    }

    return 0;
}

static void show_msg(
    const char *m
)
{
    strncpy(
        g_msg,
        m,
        63
    );

    g_msg[63] = 0;

    g_msgTimer = 2.0f;
}

/* ================================================================
 * UPDATE GAME
 * ================================================================ */

static void update_game(float dt)
{
    u16 btn;

    int lx;
    int ly;

    int rx;
    int ry;

    float thr;
    float str;

    if (!g_padReady)
        return;

    btn = pad_btns();

    lx = pad_lx();
    ly = pad_ly();

    rx = pad_rx();
    ry = pad_ry();

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

    if (
        (btn & BTN_L3) &&
        !g_prevL3
    )
    {
        p_carColor =
            (p_carColor + 1) % 5;

        show_msg(
            "Car color"
        );
    }

    g_prevL3 =
        (btn & BTN_L3)
        ? 1
        : 0;

    if (
        (btn & BTN_R3) &&
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

    g_prevR3 =
        (btn & BTN_R3)
        ? 1
        : 0;

    if (
        (btn & BTN_START) &&
        !g_prevStart
    )
    {
        g_state =
            ST_PAUSED;

        g_menuSel = 0;
    }

    g_prevStart =
        (btn & BTN_START)
        ? 1
        : 0;

    thr =
        -(ly / 128.0f);

    str =
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

        g_shake = 0.5f;
    }

    p_speed +=
        thr *
        35.0f *
        dt;

    if (
        fabsf(thr) < 0.1f
    )
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

    if (
        fabsf(p_speed) >
        0.5f
    )
    {
        float sgn;
        float gain;

        sgn =
            p_speed > 0
            ? 1.0f
            : -1.0f;

        gain =
            clampf(
                fabsf(p_speed) / 20.0f,
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

    {
        float vx;
        float vz;

        float nx;
        float nz;

        vx =
            sinf(p_heading) *
            p_speed *
            dt;

        vz =
            cosf(p_heading) *
            p_speed *
            dt;

        nx =
            p_x + vx;

        nz =
            p_z + vz;

        if (
            !hit_any(
                nx,
                p_z,
                1.5f
            )
        )
        {
            p_x = nx;
        }
        else
        {
            p_speed *= 0.2f;
            g_shake = 0.8f;
        }

        if (
            !hit_any(
                p_x,
                nz,
                1.5f
            )
        )
        {
            p_z = nz;
        }
        else
        {
            p_speed *= 0.2f;
            g_shake = 0.8f;
        }
    }

    g_shake *=
        (1.0f - 5.0f * dt);

    if (g_shake < 0.01f)
        g_shake = 0.0f;

    g_timeOfDay +=
        dt / 240.0f;

    if (
        g_timeOfDay > 1.0f
    )
    {
        g_timeOfDay -= 1.0f;
    }

    if (
        g_msgTimer > 0.0f
    )
    {
        g_msgTimer -= dt;

        if (
            g_msgTimer < 0.0f
        )
        {
            g_msgTimer = 0.0f;
        }
    }

    update_ai(dt);
    update_particles(dt);
}

/* ================================================================
 * RENDER WORLD
 * ================================================================ */

static void render_world(void)
{
    int i;

    draw_sky();

    draw_ground_grid();

    for (
        i = 0;
        i < g_numObjs;
        i++
    )
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

        {
            ObjModel *bm =
                &g_bldModels[
                    i % 2
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

    for (
        i = 0;
        i < MAX_AI;
        i++
    )
    {
        AiCar *c =
            &g_ai[i];

        ObjModel *m =
            &g_carModels[
                i % 5
            ];

        if (m->loaded)
        {
            obj_draw(
                m,
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
    }

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
    }

    for (
        i = 0;
        i < g_numParts;
        i++
    )
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

        {
            float sz;

            sz =
                p->size /
                sp.z *
                FOV;

            if (sz < 2)
                sz = 2;

            if (sz > 40)
                sz = 40;

            draw_rect2d(
                sp.x - sz * 0.5f,
                sp.y - sz * 0.5f,
                sz,
                sz,
                p->color
            );
        }
    }
}

/* ================================================================
 * HUD
 * ================================================================ */

static void draw_hud(void)
{
    char buf[128];

    tiny3d_Project2D();

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

    SetFontSize(
        28,
        28
    );

    SetFontColor(
        NEON_CYAN,
        0x00000000
    );

    DrawString(
        25,
        45,
        (char *)"NEON CITY"
    );

    SetFontSize(
        12,
        12
    );

    SetFontColor(
        NEON_MAGENTA,
        0x00000000
    );

    DrawString(
        27,
        68,
        (char *)"ULTRA v5"
    );

    draw_rect2d(
        15,
        SCR_H - 130,
        240,
        115,
        0x80000000u
    );

    sprintf(
        buf,
        "%d",
        (int)(
            fabsf(p_speed) *
            5.4f
        )
    );

    SetFontSize(
        44,
        44
    );

    SetFontColor(
        NEON_YELLOW,
        0x00000000
    );

    DrawString(
        30,
        SCR_H - 95,
        buf
    );

    SetFontSize(
        14,
        14
    );

    SetFontColor(
        0xFFFFE080u,
        0x00000000
    );

    DrawString(
        140,
        SCR_H - 95,
        (char *)"KM/H"
    );

    SetFontSize(
        12,
        12
    );

    SetFontColor(
        NEON_ORANGE,
        0x00000000
    );

    DrawString(
        30,
        SCR_H - 60,
        (char *)"BOOST"
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

    SetFontSize(
        14,
        14
    );

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
        SetFontSize(
            22,
            22
        );

        SetFontColor(
            0xFFFFFFFFu,
            0x00000000
        );

        DrawString(
            SCR_W / 2 - 100,
            140,
            g_msg
        );
    }
}

/* ================================================================
 * LOADING
 * ================================================================ */

static void draw_loading(float dt)
{
    char buf[32];

    g_loadProgress +=
        dt * 0.4f;

    if (
        g_loadProgress >
        1.0f
    )
    {
        g_loadProgress = 1.0f;
    }

    tiny3d_Project2D();

    draw_rect2d(
        0,
        0,
        SCR_W,
        SCR_H,
        0xFF03030Au
    );

    SetFontSize(
        72,
        72
    );

    SetFontColor(
        NEON_CYAN,
        0x00000000
    );

    DrawString(
        300,
        230,
        (char *)"NEON CITY"
    );

    SetFontSize(
        28,
        28
    );

    SetFontColor(
        NEON_MAGENTA,
        0x00000000
    );

    DrawString(
        480,
        310,
        (char *)"ULTRA v5"
    );

    draw_rect2d(
        340,
        430,
        600,
        30,
        0xFF111122u
    );

    draw_rect2d(
        340,
        430,
        600 *
        g_loadProgress,
        30,
        NEON_MAGENTA
    );

    sprintf(
        buf,
        "%d%%",
        (int)(
            g_loadProgress *
            100.0f
        )
    );

    SetFontSize(
        20,
        20
    );

    SetFontColor(
        0xFFFFFFFFu,
        0x00000000
    );

    DrawString(
        610,
        500,
        buf
    );

    if (
        g_loadProgress >=
        1.0f
    )
    {
        g_state =
            ST_MENU;

        g_menuSel = 0;
        g_menuTimer = 0.0f;
    }
}

/* ================================================================
 * MENU
 * ================================================================ */

static void draw_menu(float dt)
{
    const char *items[3];

    int i;

    g_menuTimer += dt;

    tiny3d_Project2D();

    draw_rect2d(
        0,
        0,
        SCR_W,
        SCR_H,
        0xFF03030Au
    );

    SetFontSize(
        82,
        82
    );

    SetFontColor(
        NEON_CYAN,
        0x00000000
    );

    DrawString(
        300,
        170,
        (char *)"NEON CITY"
    );

    SetFontSize(
        28,
        28
    );

    SetFontColor(
        NEON_MAGENTA,
        0x00000000
    );

    DrawString(
        480,
        270,
        (char *)"ULTRA v5"
    );

    items[0] = "START GAME";
    items[1] = "INSTRUCTIONS";
    items[2] = "EXIT";

    for (
        i = 0;
        i < 3;
        i++
    )
    {
        int y;
        int selected;

        y =
            390 +
            i * 70;

        selected =
            i == g_menuSel;

        draw_rect2d(
            390,
            y,
            500,
            55,
            selected
                ? 0xA000D4FFu
                : 0x50000000u
        );

        SetFontSize(
            28,
            28
        );

        SetFontColor(
            selected
                ? 0xFFFFFFFFu
                : 0xFFAAAAAAu,
            0x00000000
        );

        DrawString(
            430,
            y + 35,
            (char *)items[i]
        );
    }

    SetFontSize(
        14,
        14
    );

    SetFontColor(
        0xFFAAAAAAu,
        0x00000000
    );

    DrawString(
        470,
        650,
        (char *)"D-PAD: SELECT   CROSS: ENTER"
    );
}

/* ================================================================
 * PAUSE
 * ================================================================ */

static void draw_pause(void)
{
    int x;

    tiny3d_Project2D();

    draw_rect2d(
        0,
        0,
        SCR_W,
        SCR_H,
        0xA0000000u
    );

    x =
        SCR_W / 2 -
        250;

    draw_rect2d(
        x,
        170,
        500,
        350,
        0xFF101820u
    );

    SetFontSize(
        42,
        42
    );

    SetFontColor(
        NEON_CYAN,
        0x00000000
    );

    DrawString(
        x + 150,
        245,
        (char *)"PAUSED"
    );

    SetFontSize(
        28,
        28
    );

    SetFontColor(
        0xFFFFFFFFu,
        0x00000000
    );

    DrawString(
        x + 70,
        350,
        (char *)"CROSS  RESUME"
    );

    DrawString(
        x + 70,
        420,
        (char *)"START  RESUME"
    );

    DrawString(
        x + 70,
        490,
        (char *)"SQUARE QUIT"
    );
}

/* ================================================================
 * MENU UPDATE
 * ================================================================ */

static void update_menu(float dt)
{
    u16 btn;

    u8 up;
    u8 down;
    u8 cross;

    (void)dt;

    if (!g_padReady)
        return;

    btn =
        pad_btns();

    up =
        (btn & BTN_UP)
        ? 1
        : 0;

    down =
        (btn & BTN_DOWN)
        ? 1
        : 0;

    cross =
        (btn & BTN_CROSS)
        ? 1
        : 0;

    if (
        up &&
        !g_prevUp
    )
    {
        g_menuSel--;

        if (
            g_menuSel < 0
        )
        {
            g_menuSel = 2;
        }
    }

    if (
        down &&
        !g_prevDown
    )
    {
        g_menuSel++;

        if (
            g_menuSel > 2
        )
        {
            g_menuSel = 0;
        }
    }

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

            g_state =
                ST_PLAYING;
        }
        else if (
            g_menuSel == 1
        )
        {
            show_msg(
                "L Stick DRIVE / R Stick CAMERA"
            );
        }
        else
        {
            g_running = 0;
        }
    }

    g_prevUp = up;
    g_prevDown = down;
    g_prevCross = cross;
}

/* ================================================================
 * PAUSE UPDATE
 * ================================================================ */

static void update_pause(float dt)
{
    u16 btn;

    (void)dt;

    if (!g_padReady)
        return;

    btn =
        pad_btns();

    if (
        (btn & BTN_START) &&
        !g_prevStart
    )
    {
        g_state =
            ST_PLAYING;
    }

    if (
        (btn & BTN_CROSS) &&
        !g_prevCross
    )
    {
        g_state =
            ST_PLAYING;
    }

    if (
        btn & BTN_SQUARE
    )
    {
        g_state =
            ST_MENU;

        g_menuSel = 0;
    }

    g_prevStart =
        (btn & BTN_START)
        ? 1
        : 0;

    g_prevCross =
        (btn & BTN_CROSS)
        ? 1
        : 0;
}

/* ================================================================
 * SYSUTIL
 * ================================================================ */

static void sysutil_cb(
    u64 status,
    u64 param,
    void *userdata
)
{
    (void)param;
    (void)userdata;

    if (
        status ==
        SYSUTIL_EXIT_GAME
    )
    {
        g_running = 0;
    }
}

/* ================================================================
 * LOAD MODELS
 * ================================================================ */

static void load_models(void)
{
    const char *cars[5] =
    {
        "models/sedan.obj",
        "models/suv.obj",
        "models/race.obj",
        "models/taxi.obj",
        "models/truck.obj"
    };

    const char *buildings[2] =
    {
        "models/build1.obj",
        "models/build2.obj"
    };

    int i;

    for (
        i = 0;
        i < 5;
        i++
    )
    {
        memset(
            &g_carModels[i],
            0,
            sizeof(ObjModel)
        );

        obj_load(
            cars[i],
            &g_carModels[i]
        );
    }

    for (
        i = 0;
        i < 2;
        i++
    )
    {
        memset(
            &g_bldModels[i],
            0,
            sizeof(ObjModel)
        );

        obj_load(
            buildings[i],
            &g_bldModels[i]
        );
    }
}

/* ================================================================
 * MAIN
 * ================================================================ */

int main(
    int argc,
    char *argv[]
)
{
    struct timeval tv;

    (void)argc;
    (void)argv;

    /* ----------------
       SYSTEM
       ---------------- */

    sysModuleLoad(
        SYSMODULE_FS
    );

    ioPadInit(7);

    sysUtilRegisterCallback(
        SYSUTIL_EVENT_SLOT0,
        sysutil_cb,
        NULL
    );

    /* ----------------
       TINY3D
       ---------------- */

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
     * Tiny3D 2D coordinates are used
     * by the HUD/menu drawing code.
     */
    tiny3d_Project2D();

    ResetFont();

    /* ----------------
       RNG
       ---------------- */

    gettimeofday(
        &tv,
        NULL
    );

    rng_s =
        ((u32)tv.tv_sec ^
         (u32)tv.tv_usec ^
         0xC0FFEE42u);

    /* ----------------
       ASSETS
       ---------------- */

    assets_init();

    /* ----------------
       MODELS
       ---------------- */

    load_models();

    /* ----------------
       WORLD
       ---------------- */

    init_stars();

    gen_city();

    init_ai();

    /* ----------------
       INITIAL STATE
       ---------------- */

    g_state =
        ST_LOADING;

    g_loadProgress =
        0.0f;

    g_menuSel =
        0;

    g_numParts =
        0;

    p_x = 0.0f;
    p_z = 0.0f;

    p_heading =
        0.0f;

    p_speed =
        0.0f;

    p_boost =
        1.0f;

    /* ============================================================
       MAIN LOOP
       ============================================================ */

    while (g_running)
    {
        const float dt =
            1.0f / 60.0f;

        /* ----------------
           SYSTEM CALLBACK
           ---------------- */

        sysUtilCheckCallback();

        /* ----------------
           PAD
           ---------------- */

        g_padReady = 0;

        if (
            ioPadGetInfo(
                &g_padInfo
            ) == 0
        )
        {
            if (
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
        }

        /* ----------------
           FRAME CLEAR
           ---------------- */

        tiny3d_Clear(
            DARK_BG,
            TINY3D_CLEAR_ALL
        );

        /*
         * IMPORTANT:
         * Every frame returns to
         * the 2D projection because
         * the current renderer uses
         * screen coordinates.
         */
        tiny3d_Project2D();

        /* ----------------
           STATE
           ---------------- */

        switch (g_state)
        {
            case ST_LOADING:

                draw_loading(
                    dt
                );

                break;

            case ST_MENU:

                update_menu(
                    dt
                );

                draw_menu(
                    dt
                );

                break;

            case ST_PLAYING:

                update_game(
                    dt
                );

                render_world();

                draw_hud();

                break;

            case ST_PAUSED:

                render_world();

                draw_hud();

                draw_pause();

                update_pause(
                    dt
                );

                break;

            default:

                g_state =
                    ST_MENU;

                break;
        }

        /* ----------------
           PRESENT
           ---------------- */

        tiny3d_Flip();
    }

    /* ----------------
       CLEANUP
       ---------------- */

    ioPadEnd();

    tiny3d_Exit();

    return 0;
}
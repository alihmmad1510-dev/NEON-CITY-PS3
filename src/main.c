/* =====================================================================
 *   NEON CITY - Minimal version for old tiny3d
 *   بيستخدم بس الدوال المؤكدة موجودة
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
#include <tiny3d.h>
#include <libfont.h>

SYS_PROCESS_PARAM(1001, 0x100000)

#define SCR_W 1280
#define SCR_H 720

static int      g_running = 1;
static padInfo  g_padInfo;
static padData  g_padData;
static int      g_padReady = 0;

/* موقع اللاعب في العالم */
static float g_px = 0.0f;
static float g_pz = 0.0f;
static float g_py = 1.0f;

/* كاميرا */
static float g_camYaw   = 0.0f;
static float g_camDist  = 12.0f;
static float g_camPitch = 0.4f;

/* ============================================================
 *  قراءة البايتات من padData مباشرة (لأن الأسماء مختلفة)
 * ============================================================ */
static u16 pad_buttons(void) {
    u8 *p = (u8*)&g_padData;
    return (u16)(p[2] | (p[3] << 8));
}

static int pad_analog(int idx) {
    /* analog sticks - offsets 10..17 */
    u8 *p = (u8*)&g_padData;
    if (idx < 0 || idx > 7) return 0;
    return (int)((s8)p[10 + idx]);
}

/* ============================================================
 *  SysUtil callback
 * ============================================================ */
static void sysutil_cb(u64 status, u64 param, void *userdata) {
    (void)param; (void)userdata;
    if (status == SYSUTIL_EXIT_GAME) g_running = 0;
}

/* ============================================================
 *  رسم مستطيل 2D
 * ============================================================ */
static void draw_rect_2d(float x, float y, float w, float h, u32 col) {
    tiny3d_SetPolygon(TINY3D_QUADS);
    tiny3d_VertexPos(x,     y,     0); tiny3d_VertexColor(col);
    tiny3d_VertexPos(x + w, y,     0);
    tiny3d_VertexPos(x + w, y + h, 0);
    tiny3d_VertexPos(x,     y + h, 0);
    tiny3d_End();
}

/* ============================================================
 *  MAIN
 * ============================================================ */
int main(int argc, char *argv[]) {
    (void)argc; (void)argv;

    /* Sys modules */
    sysModuleLoad(SYSMODULE_FS);
    sysModuleLoad(SYSMODULE_PAD);

    ioPadInit(7);
    sysUtilRegisterCallback(SYSUTIL_EVENT_SLOT0, sysutil_cb, NULL);

    /* Init tiny3d */
    if (tiny3d_Init(1024 * 1024) != 0) return 1;
    ResetFont();

    /* Main loop */
    while (g_running) {
        sysUtilCheckCallback();

        /* === read pad === */
        g_padReady = 0;
        if (ioPadGetInfo(&g_padInfo) == 0 && g_padInfo.status[0]) {
            if (ioPadGetData(0, &g_padData) == 0) {
                g_padReady = 1;
            }
        }

        u16 btn = g_padReady ? pad_buttons() : 0;

        /* === update logic === */
        if (g_padReady) {
            /* استخدام الأزرار بالبِت ماسك - القيم القياسية PSL1GHT */
            #define B_SELECT  0x0001
            #define B_L3      0x0002
            #define B_R3      0x0004
            #define B_START   0x0008
            #define B_UP      0x0010
            #define B_RIGHT   0x0020
            #define B_DOWN    0x0040
            #define B_LEFT    0x0080
            #define B_L2      0x0100
            #define B_R2      0x0200
            #define B_L1      0x0400
            #define B_R1      0x0800
            #define B_TRIANGLE 0x1000
            #define B_CIRCLE  0x2000
            #define B_CROSS   0x4000
            #define B_SQUARE  0x8000

            /* حركة */
            if (btn & B_UP)    g_pz -= 0.4f;
            if (btn & B_DOWN)  g_pz += 0.4f;
            if (btn & B_LEFT)  g_px -= 0.4f;
            if (btn & B_RIGHT) g_px += 0.4f;

            /* كاميرا */
            if (btn & B_L1) g_camYaw -= 0.04f;
            if (btn & B_R1) g_camYaw += 0.04f;
            if (btn & B_L2) g_camDist += 0.4f;
            if (btn & B_R2) g_camDist -= 0.4f;
            if (g_camDist < 4.0f)  g_camDist = 4.0f;
            if (g_camDist > 30.0f) g_camDist = 30.0f;

            /* خروج بـ SELECT */
            if (btn & B_SELECT) g_running = 0;
        }

        /* === render === */
        tiny3d_Clear(0xFF2050A0u, 0xFFFFFFFF);

        /* رسم منظر 2D بسيط */
        /* سماء */
        draw_rect_2d(0, 0, SCR_W, SCR_H * 0.6f, 0xFF3060C0u);
        /* أرضية */
        draw_rect_2d(0, SCR_H * 0.6f, SCR_W, SCR_H * 0.4f, 0xFF1A1A22u);

        /* مربعات تتحرك مع اللاعب (تمثيل بصري) */
        float screenCX = SCR_W * 0.5f;
        float screenCY = SCR_H * 0.6f;
        float scale = 8.0f;

        /* أرضية - شبكة */
        for (int i = -10; i <= 10; i++) {
            for (int j = -10; j <= 10; j++) {
                float wx = i * 5.0f;
                float wz = j * 5.0f;
                /* relative to player */
                float rx = wx - g_px;
                float rz = wz - g_pz;
                /* simple projection */
                if (rz < 1.0f) continue;
                float sx = screenCX + (rx / rz) * 300.0f;
                float sy = screenCY + (5.0f / rz) * 300.0f;
                float sz = 30.0f / rz * scale;
                if (sx < -50 || sx > SCR_W + 50) continue;
                if (sy < -50 || sy > SCR_H + 50) continue;
                u32 col = ((i + j) & 1) ? 0xFF202030u : 0xFF2A2A40u;
                draw_rect_2d(sx - sz * 0.5f, sy - sz * 0.25f, sz, sz * 0.5f, col);
            }
        }

        /* لاعب - مربع أصفر في النص */
        draw_rect_2d(screenCX - 20, screenCY - 40, 40, 80, 0xFFFFCC00u);

        /* === HUD === */
        SetFontSize(24, 24);
        SetFontColor(0xFF00D4FFu, 0x00000000);
        DrawString(20, 40, "NEON CITY");

        SetFontSize(14, 14);
        SetFontColor(0xFFFFFFFFu, 0x00000000);
        DrawString(20, 70, "PS3 HOMEBREW");

        char buf[128];
        sprintf(buf, "POS  X:%.1f  Z:%.1f", g_px, g_pz);
        DrawString(20, 95, buf);

        sprintf(buf, "CAM  yaw:%.2f  dist:%.1f", g_camYaw, g_camDist);
        DrawString(20, 115, buf);

        sprintf(buf, "BTN  0x%04X", btn);
        DrawString(20, 135, buf);

        SetFontSize(11, 11);
        SetFontColor(0xFFAAAAAAu, 0x00000000);
        DrawString(20, SCR_H - 25, "D-PAD: move   L1/R1: rotate   L2/R2: zoom   SELECT: exit");

        /* Flip */
        tiny3d_Flip();
    }

    /* Cleanup */
    ioPadEnd();
    tiny3d_Exit();
    return 0;
}
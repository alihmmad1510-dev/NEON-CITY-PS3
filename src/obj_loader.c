/* =====================================================================
 *  FILE: src/obj_loader.c
 *  PROJECT: NEON CITY ULTRA v5
 *  DESCRIPTION: Safe OBJ + MTL loader for Tiny3D
 * ===================================================================== */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <ppu-types.h>

#include <tiny3d.h>
#include "obj_loader.h"

/* Provided by main.c */
extern void project_point(float wx, float wy, float wz,
                          float *sx, float *sy, float *sz, int *vis);

/* ================================================================
 * Asset file helper
 * ================================================================ */

static FILE *open_asset(const char *path)
{
    FILE *f;
    char alt[512];

    if (!path || !path[0])
        return NULL;

    /* 1. Current directory */
    f = fopen(path, "r");
    if (f)
        return f;

    /* 2. Relative USRDIR */
    snprintf(alt, sizeof(alt), "USRDIR/%s", path);
    f = fopen(alt, "r");
    if (f)
        return f;

    /* 3. Installed game directory */
    snprintf(
        alt,
        sizeof(alt),
        "/dev_hdd0/game/NEONCITY1/USRDIR/%s",
        path
    );

    f = fopen(alt, "r");
    if (f)
        return f;

    /* 4. app_home */
    snprintf(alt, sizeof(alt), "/app_home/%s", path);
    f = fopen(alt, "r");

    return f;
}

/* ================================================================
 * MTL
 * ================================================================ */

static int load_mtl(const char *path, ObjModel *m)
{
    FILE *f;
    char line[256];
    int cur = -1;

    if (!m)
        return 0;

    f = open_asset(path);

    if (!f)
        return 0;

    while (fgets(line, sizeof(line), f))
    {
        char *p = line;

        while (*p == ' ' || *p == '\t')
            p++;

        /* newmtl */
        if (strncmp(p, "newmtl", 6) == 0)
        {
            if (m->num_mats < OBJ_MAX_MATS)
            {
                cur = m->num_mats++;

                memset(
                    &m->mats[cur],
                    0,
                    sizeof(m->mats[cur])
                );

                sscanf(
                    p + 6,
                    "%31s",
                    m->mats[cur].name
                );

                m->mats[cur].r = 200;
                m->mats[cur].g = 200;
                m->mats[cur].b = 200;
            }
        }

        /* diffuse color */
        else if (
            p[0] == 'K' &&
            p[1] == 'd' &&
            cur >= 0
        )
        {
            float r, g, b;

            if (sscanf(
                    p + 2,
                    "%f %f %f",
                    &r,
                    &g,
                    &b
                ) == 3)
            {
                if (r < 0.0f) r = 0.0f;
                if (g < 0.0f) g = 0.0f;
                if (b < 0.0f) b = 0.0f;

                if (r > 1.0f) r = 1.0f;
                if (g > 1.0f) g = 1.0f;
                if (b > 1.0f) b = 1.0f;

                m->mats[cur].r =
                    (u8)(r * 255.0f);

                m->mats[cur].g =
                    (u8)(g * 255.0f);

                m->mats[cur].b =
                    (u8)(b * 255.0f);
            }
        }
    }

    fclose(f);

    return 1;
}

/* ================================================================
 * Material search
 * ================================================================ */

static int find_mat(
    ObjModel *m,
    const char *name
)
{
    int i;

    if (!m || !name)
        return 0;

    for (i = 0; i < m->num_mats; i++)
    {
        if (strcmp(
                m->mats[i].name,
                name
            ) == 0)
        {
            return i;
        }
    }

    return 0;
}

/* ================================================================
 * OBJ index parser
 *
 * Supports:
 *   1
 *   1/2
 *   1/2/3
 *   1//3
 *   -1/-2/-3
 * ================================================================ */

static int parse_obj_vertex_index(
    const char *token,
    int vertex_count
)
{
    int value = 0;
    int sign = 1;
    const char *p = token;

    if (!p)
        return -1;

    if (*p == '-')
    {
        sign = -1;
        p++;
    }

    while (*p >= '0' && *p <= '9')
    {
        value =
            value * 10 +
            (*p - '0');

        p++;
    }

    value *= sign;

    if (value > 0)
        return value - 1;

    if (value < 0)
        return vertex_count + value;

    return -1;
}

/* ================================================================
 * OBJ loader
 * ================================================================ */

int obj_load(
    const char *obj_path,
    ObjModel *m
)
{
    FILE *f;
    char line[512];
    char mtl_path[256];

    int cur_mat = 0;

    if (!m || !obj_path)
        return 0;

    memset(
        m,
        0,
        sizeof(*m)
    );

    m->scale = 1.0f;
    m->y_offset = 0.0f;
    m->loaded = 0;

    /* ------------------------------------------------------------
     * Load matching MTL
     * ------------------------------------------------------------ */

    strncpy(
        mtl_path,
        obj_path,
        sizeof(mtl_path) - 1
    );

    mtl_path[
        sizeof(mtl_path) - 1
    ] = '\0';

    {
        char *dot =
            strrchr(mtl_path, '.');

        if (dot)
            strcpy(dot, ".mtl");
    }

    load_mtl(
        mtl_path,
        m
    );

    /* ------------------------------------------------------------
     * Default material
     * ------------------------------------------------------------ */

    if (m->num_mats == 0)
    {
        m->num_mats = 1;

        strcpy(
            m->mats[0].name,
            "default"
        );

        m->mats[0].r = 200;
        m->mats[0].g = 200;
        m->mats[0].b = 200;
    }

    /* ------------------------------------------------------------
     * Open OBJ
     * ------------------------------------------------------------ */

    f = open_asset(obj_path);

    if (!f)
    {
        return 0;
    }

    /* ------------------------------------------------------------
     * Parse
     * ------------------------------------------------------------ */

    while (fgets(
        line,
        sizeof(line),
        f))
    {
        char *p = line;

        while (*p == ' ' || *p == '\t')
            p++;

        /* --------------------------------------------------------
         * Vertex
         * -------------------------------------------------------- */

        if (
            p[0] == 'v' &&
            p[1] == ' '
        )
        {
            if (
                m->num_verts <
                OBJ_MAX_VERTS
            )
            {
                float x, y, z;

                if (sscanf(
                        p + 2,
                        "%f %f %f",
                        &x,
                        &y,
                        &z
                    ) == 3)
                {
                    m->verts[
                        m->num_verts
                    ].x = x;

                    m->verts[
                        m->num_verts
                    ].y = y;

                    m->verts[
                        m->num_verts
                    ].z = z;

                    m->num_verts++;
                }
            }
        }

        /* --------------------------------------------------------
         * Material
         * -------------------------------------------------------- */

        else if (
            strncmp(
                p,
                "usemtl",
                6
            ) == 0
        )
        {
            char name[64];

            if (sscanf(
                    p + 6,
                    "%63s",
                    name
                ) == 1)
            {
                cur_mat =
                    find_mat(
                        m,
                        name
                    );

                if (
                    cur_mat < 0 ||
                    cur_mat >= m->num_mats
                )
                {
                    cur_mat = 0;
                }
            }
        }

        /* --------------------------------------------------------
         * Face
         * -------------------------------------------------------- */

        else if (
            p[0] == 'f' &&
            p[1] == ' '
        )
        {
            int idx[16];
            int count = 0;
            char *tok = p + 2;

            while (
                *tok &&
                count < 16
            )
            {
                while (
                    *tok == ' ' ||
                    *tok == '\t'
                )
                {
                    tok++;
                }

                if (
                    !*tok ||
                    *tok == '\n' ||
                    *tok == '\r'
                )
                {
                    break;
                }

                idx[count] =
                    parse_obj_vertex_index(
                        tok,
                        m->num_verts
                    );

                count++;

                while (
                    *tok &&
                    *tok != ' ' &&
                    *tok != '\t' &&
                    *tok != '\n' &&
                    *tok != '\r'
                )
                {
                    tok++;
                }
            }

            /* ----------------------------------------------------
             * Fan triangulation
             * ---------------------------------------------------- */

            if (count >= 3)
            {
                int k;

                for (
                    k = 1;
                    k < count - 1;
                    k++
                )
                {
                    int a = idx[0];
                    int b = idx[k];
                    int c = idx[k + 1];

                    if (
                        a >= 0 &&
                        a < m->num_verts &&
                        b >= 0 &&
                        b < m->num_verts &&
                        c >= 0 &&
                        c < m->num_verts &&
                        m->num_faces <
                            OBJ_MAX_FACES
                    )
                    {
                        m->faces[
                            m->num_faces
                        ].a = a;

                        m->faces[
                            m->num_faces
                        ].b = b;

                        m->faces[
                            m->num_faces
                        ].c = c;

                        m->faces[
                            m->num_faces
                        ].mat = cur_mat;

                        m->num_faces++;
                    }
                }
            }
        }
    }

    fclose(f);

    /* ------------------------------------------------------------
     * Reject empty model
     * ------------------------------------------------------------ */

    if (
        m->num_verts == 0 ||
        m->num_faces == 0
    )
    {
        m->loaded = 0;
        return 0;
    }

    /* ------------------------------------------------------------
     * Auto-center / normalize height
     * ------------------------------------------------------------ */

    {
        float minY = 1000000000.0f;
        float maxY = -1000000000.0f;
        int i;

        for (
            i = 0;
            i < m->num_verts;
            i++
        )
        {
            if (
                m->verts[i].y <
                minY
            )
            {
                minY =
                    m->verts[i].y;
            }

            if (
                m->verts[i].y >
                maxY
            )
            {
                maxY =
                    m->verts[i].y;
            }
        }

        m->y_offset = -minY;

        {
            float height =
                maxY - minY;

            if (height > 0.001f)
            {
                m->scale =
                    2.0f / height;
            }
            else
            {
                m->scale = 1.0f;
            }
        }
    }

    m->loaded = 1;

    return 1;
}

/* ================================================================
 * Projection buffers
 * ================================================================ */

static float g_px[OBJ_MAX_VERTS];
static float g_py[OBJ_MAX_VERTS];
static float g_pz[OBJ_MAX_VERTS];
static int   g_pv[OBJ_MAX_VERTS];

/* ================================================================
 * Draw OBJ
 * ================================================================ */

void obj_draw(
    ObjModel *m,
    float cx,
    float cy,
    float cz,
    float yaw,
    float scale
)
{
    int i;

    if (
        !m ||
        !m->loaded ||
        m->num_verts <= 0 ||
        m->num_faces <= 0
    )
    {
        return;
    }

    {
        float c = cosf(yaw);
        float s = sinf(yaw);

        float sc =
            scale * m->scale;

        /* --------------------------------------------------------
         * Transform + project vertices
         * -------------------------------------------------------- */

        for (
            i = 0;
            i < m->num_verts;
            i++
        )
        {
            float vx =
                m->verts[i].x * sc;

            float vy =
                (m->verts[i].y +
                 m->y_offset) * sc;

            float vz =
                m->verts[i].z * sc;

            float rx =
                vx * c +
                vz * s;

            float rz =
                -vx * s +
                vz * c;

            float sx;
            float sy;
            float sz;
            int vis;

            project_point(
                cx + rx,
                cy + vy,
                cz + rz,
                &sx,
                &sy,
                &sz,
                &vis
            );

            g_px[i] = sx;
            g_py[i] = sy;
            g_pz[i] = sz;
            g_pv[i] = vis;
        }
    }

    /* ------------------------------------------------------------
     * Draw triangles
     * ------------------------------------------------------------ */

    for (
        i = 0;
        i < m->num_faces;
        i++
    )
    {
        ObjFace *face =
            &m->faces[i];

        int a = face->a;
        int b = face->b;
        int c = face->c;

        if (
            a < 0 ||
            a >= m->num_verts ||
            b < 0 ||
            b >= m->num_verts ||
            c < 0 ||
            c >= m->num_verts
        )
        {
            continue;
        }

        if (
            !g_pv[a] ||
            !g_pv[b] ||
            !g_pv[c]
        )
        {
            continue;
        }

        /* --------------------------------------------------------
         * Material safety
         * -------------------------------------------------------- */

        int mat_id =
            face->mat;

        if (
            mat_id < 0 ||
            mat_id >= m->num_mats
        )
        {
            mat_id = 0;
        }

        ObjMaterial *mat =
            &m->mats[mat_id];

        u32 col =
            0xFF000000u |
            ((u32)mat->r << 16) |
            ((u32)mat->g << 8) |
            ((u32)mat->b);

        /* --------------------------------------------------------
         * Back-face culling intentionally disabled.
         *
         * This makes debugging easier and prevents models with
         * reversed OBJ winding from becoming completely invisible.
         * -------------------------------------------------------- */

        tiny3d_SetPolygon(
            TINY3D_TRIANGLES
        );

        tiny3d_VertexPos(
            g_px[a],
            g_py[a],
            g_pz[a]
        );

        tiny3d_VertexColor(col);

        tiny3d_VertexPos(
            g_px[b],
            g_py[b],
            g_pz[b]
        );

        tiny3d_VertexColor(col);

        tiny3d_VertexPos(
            g_px[c],
            g_py[c],
            g_pz[c]
        );

        tiny3d_VertexColor(col);

        tiny3d_End();
    }
}
/* =====================================================================
 *  FILE: src/obj_loader.c
 *  PROJECT: NEON CITY ULTRA v5
 *  DESCRIPTION: Parse OBJ + MTL, draw with tiny3d
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

/* Try to open file from multiple locations */
static FILE *open_asset(const char *path){
    FILE *f = fopen(path, "r");
    if (f) return f;

    char alt[512];
    snprintf(alt, sizeof(alt), "USRDIR/%s", path);
    f = fopen(alt, "r");
    if (f) return f;

    snprintf(alt, sizeof(alt), "/dev_hdd0/game/NEONCITY1/USRDIR/%s", path);
    f = fopen(alt, "r");
    if (f) return f;

    snprintf(alt, sizeof(alt), "/app_home/%s", path);
    f = fopen(alt, "r");
    return f;
}

/* ================= MTL Parser ================= */
static int load_mtl(const char *path, ObjModel *m){
    FILE *f = open_asset(path);
    if (!f) return 0;

    char line[256];
    int cur = -1;

    while (fgets(line, sizeof(line), f)){
        char *p = line;
        while (*p == ' ' || *p == '\t') p++;

        if (strncmp(p, "newmtl", 6) == 0){
            if (m->num_mats < OBJ_MAX_MATS){
                cur = m->num_mats++;
                sscanf(p + 6, "%31s", m->mats[cur].name);
                m->mats[cur].r = 200;
                m->mats[cur].g = 200;
                m->mats[cur].b = 200;
            }
        } else if (p[0] == 'K' && p[1] == 'd' && cur >= 0){
            float r, g, b;
            if (sscanf(p + 2, "%f %f %f", &r, &g, &b) == 3){
                m->mats[cur].r = (u8)(r * 255.0f);
                m->mats[cur].g = (u8)(g * 255.0f);
                m->mats[cur].b = (u8)(b * 255.0f);
            }
        }
    }
    fclose(f);
    return 1;
}

static int find_mat(ObjModel *m, const char *name){
    for (int i = 0; i < m->num_mats; i++)
        if (strcmp(m->mats[i].name, name) == 0) return i;
    return 0;
}

/* ================= OBJ Parser ================= */
static int parse_vidx(const char *s){
    int v = 0;
    while (*s >= '0' && *s <= '9'){
        v = v * 10 + (*s - '0');
        s++;
    }
    return v;
}

int obj_load(const char *obj_path, ObjModel *m){
    memset(m, 0, sizeof(*m));
    m->scale = 1.0f;
    m->y_offset = 0.0f;

    /* Load MTL */
    char mtl_path[256];
    strncpy(mtl_path, obj_path, sizeof(mtl_path) - 1);
    mtl_path[sizeof(mtl_path) - 1] = 0;
    char *dot = strrchr(mtl_path, '.');
    if (dot) strcpy(dot, ".mtl");
    load_mtl(mtl_path, m);

    if (m->num_mats == 0){
        m->num_mats = 1;
        strcpy(m->mats[0].name, "default");
        m->mats[0].r = 200;
        m->mats[0].g = 200;
        m->mats[0].b = 200;
    }

    FILE *f = open_asset(obj_path);
    if (!f) return 0;

    char line[512];
    int cur_mat = 0;

    while (fgets(line, sizeof(line), f)){
        char *p = line;
        while (*p == ' ' || *p == '\t') p++;

        if (p[0] == 'v' && p[1] == ' '){
            if (m->num_verts < OBJ_MAX_VERTS){
                float x, y, z;
                if (sscanf(p + 2, "%f %f %f", &x, &y, &z) == 3){
                    m->verts[m->num_verts].x = x;
                    m->verts[m->num_verts].y = y;
                    m->verts[m->num_verts].z = z;
                    m->num_verts++;
                }
            }
        }
        else if (strncmp(p, "usemtl", 6) == 0){
            char name[64];
            if (sscanf(p + 6, "%63s", name) == 1)
                cur_mat = find_mat(m, name);
        }
        else if (p[0] == 'f' && p[1] == ' '){
            if (m->num_faces < OBJ_MAX_FACES){
                char *tok = p + 2;
                int idx[8];
                int count = 0;

                while (*tok && count < 8){
                    while (*tok == ' ' || *tok == '\t') tok++;
                    if (!*tok || *tok == '\n') break;
                    idx[count++] = parse_vidx(tok) - 1;
                    while (*tok && *tok != ' ' && *tok != '\t') tok++;
                }

                /* Fan triangulation */
                for (int k = 1; k < count - 1; k++){
                    int a = idx[0], b = idx[k], c = idx[k+1];
                    if (a >= 0 && a < m->num_verts &&
                        b >= 0 && b < m->num_verts &&
                        c >= 0 && c < m->num_verts &&
                        m->num_faces < OBJ_MAX_FACES){
                        m->faces[m->num_faces].a = a;
                        m->faces[m->num_faces].b = b;
                        m->faces[m->num_faces].c = c;
                        m->faces[m->num_faces].mat = cur_mat;
                        m->num_faces++;
                    }
                }
            }
        }
    }
    fclose(f);

    /* Auto-center */
    if (m->num_verts > 0){
        float minY = 1e9f, maxY = -1e9f;
        for (int i = 0; i < m->num_verts; i++){
            if (m->verts[i].y < minY) minY = m->verts[i].y;
            if (m->verts[i].y > maxY) maxY = m->verts[i].y;
        }
        m->y_offset = -minY;
        float height = maxY - minY;
        if (height > 0.001f) m->scale = 2.0f / height;
    }

    m->loaded = 1;
    return 1;
}

/* ================= Draw ================= */
static float g_px[OBJ_MAX_VERTS];
static float g_py[OBJ_MAX_VERTS];
static int   g_pv[OBJ_MAX_VERTS];

void obj_draw(ObjModel *m, float cx, float cy, float cz,
              float yaw, float scale){
    if (!m || !m->loaded || m->num_verts == 0) return;

    float c = cosf(yaw), s = sinf(yaw);
    float sc = scale * m->scale;

    for (int i = 0; i < m->num_verts; i++){
        float vx = m->verts[i].x * sc;
        float vy = (m->verts[i].y + m->y_offset) * sc;
        float vz = m->verts[i].z * sc;
        float rx =  vx * c + vz * s;
        float rz = -vx * s + vz * c;
        float sx, sy, sz;
        int   vis;
        project_point(cx + rx, cy + vy, cz + rz, &sx, &sy, &sz, &vis);
        g_px[i] = sx;
        g_py[i] = sy;
        g_pv[i] = vis;
    }

    for (int i = 0; i < m->num_faces; i++){
        ObjFace *f = &m->faces[i];
        if (!g_pv[f->a] || !g_pv[f->b] || !g_pv[f->c]) continue;

        /* Back-face culling */
        float abx = g_px[f->b] - g_px[f->a];
        float aby = g_py[f->b] - g_py[f->a];
        float acx = g_px[f->c] - g_px[f->a];
        float acy = g_py[f->c] - g_py[f->a];
        if (abx * acy - aby * acx > 0.0f) continue;

        ObjMaterial *mat = &m->mats[f->mat];
        u32 col = 0xFF000000u
                | ((u32)mat->r << 16)
                | ((u32)mat->g << 8)
                | (u32)mat->b;

        tiny3d_SetPolygon(TINY3D_TRIANGLES);
        tiny3d_VertexPos(g_px[f->a], g_py[f->a], 0); tiny3d_VertexColor(col);
        tiny3d_VertexPos(g_px[f->b], g_py[f->b], 0); tiny3d_VertexColor(col);
        tiny3d_VertexPos(g_px[f->c], g_py[f->c], 0); tiny3d_VertexColor(col);
        tiny3d_End();
    }
}
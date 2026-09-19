/* =====================================================================
 *  FILE: src/obj_loader.h
 *  PROJECT: NEON CITY ULTRA v5
 *  DESCRIPTION: OBJ + MTL loader for tiny3d
 * ===================================================================== */
#ifndef OBJ_LOADER_H
#define OBJ_LOADER_H

#include <ppu-types.h>

typedef struct { float x, y, z; } ObjVec3;

typedef struct {
    char name[32];
    u8 r, g, b;
} ObjMaterial;

typedef struct {
    int a, b, c;
    int mat;
} ObjFace;

#define OBJ_MAX_VERTS   8000
#define OBJ_MAX_FACES   8000
#define OBJ_MAX_MATS    32

typedef struct {
    char name[32];
    ObjVec3     verts[OBJ_MAX_VERTS];
    int         num_verts;
    ObjFace     faces[OBJ_MAX_FACES];
    int         num_faces;
    ObjMaterial mats[OBJ_MAX_MATS];
    int         num_mats;
    float       scale;
    float       y_offset;
    int         loaded;
} ObjModel;

int  obj_load(const char *obj_path, ObjModel *m);
void obj_draw(ObjModel *m, float cx, float cy, float cz,
              float yaw, float scale);

#endif
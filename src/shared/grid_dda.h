#ifndef GRID_DDA_H
#define GRID_DDA_H

#include "raycast_common.h"

namespace rc {

struct DdaRayHit {
    int tileX = -1;
    int tileY = -1;
    int cell = 0;
    float distance = 0.0f;
    float perpDist = 0.0f;
    int texX = 0;
    bool verticalSide = false;
};

struct DdaStepInfo {
    int tileX = 0;
    int tileY = 0;
    int cell = 0;
    float perpDist = 0.0f;
    float distance = 0.0f;
    int texX = 0;
    bool verticalSide = false;
};

using GridCellLookup = int (*)(void* ctx, int x, int y, int level);

// GBRaycaster-style delegate: return true to STOP the ray, false to penetrate.
using DdaRayDelegate = bool (*)(void* ctx, const DdaStepInfo& step);

constexpr float DDA_RAY_INFINITE = 0.0f;

bool castDdaRayEx(float originX, float originY, float dirX, float dirY, float maxDistance,
                  int level, GridCellLookup lookup, void* lookupCtx,
                  DdaRayDelegate delegate, void* delegateCtx, DdaRayHit& hit);

bool castDdaRay(float originX, float originY, float dirX, float dirY, float maxDistance,
                int level, GridCellLookup lookup, void* lookupCtx, DdaRayHit& hit);

} // namespace rc

#endif

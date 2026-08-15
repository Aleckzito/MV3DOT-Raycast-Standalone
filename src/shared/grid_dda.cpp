#include "grid_dda.h"

#include <algorithm>
#include <cmath>

namespace rc {

namespace {

void fillStepMetrics(float originX, float originY, float dirX, float dirY, float mapX, float mapY,
                     int stepX, int stepY, int side, float traveledTiles, DdaStepInfo& step)
{
    if (side == 0) {
        step.perpDist = (mapX - originX / static_cast<float>(TILE_SIZE) + (1 - stepX) / 2.0f) / dirX;
        step.texX = static_cast<int>(originY + step.perpDist * dirY);
    } else {
        step.perpDist = (mapY - originY / static_cast<float>(TILE_SIZE) + (1 - stepY) / 2.0f) / dirY;
        step.texX = static_cast<int>(originX + step.perpDist * dirX);
    }
    step.texX &= (TEXTURE_SIZE - 1);
    if (step.perpDist < 0.01f) step.perpDist = 0.01f;
    step.verticalSide = side == 1;
    step.distance = traveledTiles * static_cast<float>(TILE_SIZE);
}

bool simpleStopDelegate(void* ctx, const DdaStepInfo& step)
{
    auto* hit = static_cast<DdaRayHit*>(ctx);
    hit->tileX = step.tileX;
    hit->tileY = step.tileY;
    hit->cell = step.cell;
    hit->distance = step.distance;
    hit->perpDist = step.perpDist;
    hit->texX = step.texX;
    hit->verticalSide = step.verticalSide;
    return true;
}

} // namespace

bool castDdaRayEx(float originX, float originY, float dirX, float dirY, float maxDistance,
                  int level, GridCellLookup lookup, void* lookupCtx,
                  DdaRayDelegate delegate, void* delegateCtx, DdaRayHit& hit)
{
    hit = DdaRayHit {};

    const float len = std::sqrt(dirX * dirX + dirY * dirY);
    if (len < 1e-6f || !lookup || !delegate) return false;

    const float savedOriginX = originX;
    const float savedOriginY = originY;

    const float ndx = dirX / len;
    const float ndy = dirY / len;
    const float startBias = static_cast<float>(TILE_SIZE) * 0.05f;
    originX += ndx * startBias;
    originY += ndy * startBias;

    float mapX = originX / static_cast<float>(TILE_SIZE);
    float mapY = originY / static_cast<float>(TILE_SIZE);
    const float deltaDistX = (ndx == 0.0f) ? 1e30f : std::abs(1.0f / ndx);
    const float deltaDistY = (ndy == 0.0f) ? 1e30f : std::abs(1.0f / ndy);

    int stepX = 0;
    int stepY = 0;
    float sideDistX = 0.0f;
    float sideDistY = 0.0f;

    if (ndx < 0.0f) {
        stepX = -1;
        sideDistX = (mapX - std::floor(mapX)) * deltaDistX;
    } else {
        stepX = 1;
        sideDistX = (std::floor(mapX) + 1.0f - mapX) * deltaDistX;
    }
    if (ndy < 0.0f) {
        stepY = -1;
        sideDistY = (mapY - std::floor(mapY)) * deltaDistY;
    } else {
        stepY = 1;
        sideDistY = (std::floor(mapY) + 1.0f - mapY) * deltaDistY;
    }

    const float maxDistTiles =
        (maxDistance <= DDA_RAY_INFINITE)
            ? static_cast<float>(std::max(MAP_WIDTH, MAP_HEIGHT) * 2)
            : maxDistance / static_cast<float>(TILE_SIZE);

    float traveled = 0.0f;

    while (traveled < maxDistTiles) {
        int side = 0;
        if (sideDistX < sideDistY) {
            sideDistX += deltaDistX;
            mapX += stepX;
            traveled = sideDistX - deltaDistX;
            side = 0;
        } else {
            sideDistY += deltaDistY;
            mapY += stepY;
            traveled = sideDistY - deltaDistY;
            side = 1;
        }

        const int tileX = static_cast<int>(mapX);
        const int tileY = static_cast<int>(mapY);
        if (tileX < 0 || tileY < 0 || tileX >= MAP_WIDTH || tileY >= MAP_HEIGHT) {
            return false;
        }

        const int cell = lookup(lookupCtx, tileX, tileY, level);
        if (cell == 0) continue;

        DdaStepInfo step;
        step.tileX = tileX;
        step.tileY = tileY;
        step.cell = cell;
        fillStepMetrics(savedOriginX, savedOriginY, dirX, dirY, mapX, mapY, stepX, stepY, side,
                        traveled, step);

        if (delegate(delegateCtx, step)) {
            hit.tileX = step.tileX;
            hit.tileY = step.tileY;
            hit.cell = step.cell;
            hit.distance = step.distance;
            hit.perpDist = step.perpDist;
            hit.texX = step.texX;
            hit.verticalSide = step.verticalSide;
            return true;
        }
    }

    return false;
}

bool castDdaRay(float originX, float originY, float dirX, float dirY, float maxDistance,
                int level, GridCellLookup lookup, void* lookupCtx, DdaRayHit& hit)
{
    return castDdaRayEx(originX, originY, dirX, dirY, maxDistance, level, lookup, lookupCtx,
                        simpleStopDelegate, &hit, hit);
}

} // namespace rc

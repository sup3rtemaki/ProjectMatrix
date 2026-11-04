#pragma once

// Draw order of the world. Inside Objects, things are sorted by Y (see DepthQueue).
namespace RenderLayer {
constexpr int Ground = 0;      // floor tiles
constexpr int Objects = 1;     // cycles, trails, walls, towers
constexpr int Projectiles = 2; // on top of everything in the world
}

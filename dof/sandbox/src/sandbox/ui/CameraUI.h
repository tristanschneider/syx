#pragma once

struct sbx_Renderer;
struct nk_context;

typedef struct sbx_Renderer sbx_Renderer;
typedef struct nk_context nk_context;

void sbx_CameraUI_draw(nk_context* ctx, sbx_Renderer* renderer);

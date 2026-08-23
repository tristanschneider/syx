#pragma once

#include <stdbool.h>

struct nk_context;

struct sbx_TimeControlUI {
  //Amount of time required to tick once
  float timePerTick;
  //Time accumulated through updates that can be spent via tryTick
  float accumulatedTime;
  //If the user selected play, meaning time will accumulate until the user pauses.
  bool isPlaying;
  bool needsReset;
};
typedef struct sbx_TimeControlUI sbx_TimeControlUI;

struct sbx_TimeControlUpdate {
  bool tick;
  bool reset;
};
typedef struct sbx_TimeControlUpdate sbx_TimeControlUpdate;

//Draw without any begin/end so this goes in the active nk window
void sbx_TimeControlUI_drawInline(struct nk_context* ctx, sbx_TimeControlUI* time, float dt);
//Draw in a new nk window
void sbx_TimeControlUI_draw(struct nk_context* ctx, sbx_TimeControlUI* time, float dt);
sbx_TimeControlUpdate sbx_TimeControlUI_tryUpdate(sbx_TimeControlUI* time);

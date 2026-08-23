#include <sandbox/ui/TimeControlUI.h>

#include <Nuklear/nuklear.h>
#include <sandbox/ui/nkExt.h>

void sbx_TimeControlUI_drawInline(struct nk_context* ctx, sbx_TimeControlUI* time, float dt) {
  if(time->isPlaying) {
    time->accumulatedTime += dt;

    if(nk_button_label(ctx, "Pause")) {
      time->isPlaying = false;
      time->accumulatedTime = 0;
    }
  }
  else {
    if(nk_button_label(ctx, "Play")) {
      time->isPlaying = true;
    }
    if(nk_button_label(ctx, "Step")) {
      time->accumulatedTime += time->timePerTick;
    }
    if(nk_button_label(ctx, "Reset")) {
      time->needsReset = true;
    }
  }
}

void sbx_TimeControlUI_draw(struct nk_context* ctx, sbx_TimeControlUI* time, float dt) {
  if(nk_begin(ctx, "timecontrol", nk_rect(500, 100, 200, 20), nkx_basicWindow())) {
    nk_layout_row_static(ctx, 20, 50, 3);
    sbx_TimeControlUI_drawInline(ctx, time, dt);
  }
}

sbx_TimeControlUpdate sbx_TimeControlUI_tryUpdate(sbx_TimeControlUI* time) {
  sbx_TimeControlUpdate result = {
    .tick = time->accumulatedTime >= time->timePerTick,
    .reset = time->needsReset
  };
  if(result.tick) {
    time->accumulatedTime -= time->timePerTick;
  }
  if(result.reset) {
    time->needsReset = false;
  }
  return result;
}

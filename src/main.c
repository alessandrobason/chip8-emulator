#include <stdio.h>
#include <stdlib.h>
#include <sokol/sokol.h>

#include "chip8.h"
#include "types.h"

#include "breakout-roms.h"

#define ZOOM 12
#define SPEED_MULTIPLIER 5

void init(void);
void frame(void);
void input(const sapp_event *e);
void cleanup(void);

static struct {
    sg_pass_action pass_action;
    sg_image img;
    sgl_pipeline pip;
    u64 last_time;
} state;

sapp_desc sokol_main(int argc, char **argv) {
    (void)argc;(void)argv;
    return (sapp_desc) {
        .width = 64 * ZOOM,
        .height = 32 * ZOOM,
        .window_title = "chip-8 emulator",
        .init_cb = init,
        .frame_cb = frame,
        .event_cb = input,
        .cleanup_cb = cleanup
    };
}

void init(void) {
    sg_setup(&(sg_desc) {
        .context = sapp_sgcontext()
    });
    sgl_setup(&(sgl_desc_t) {
        .sample_count = sapp_sample_count()
    });
    stm_setup();

    state.pip = sgl_make_pipeline(&(sg_pipeline_desc) {
        .cull_mode = SG_CULLMODE_BACK,
        .depth = {
            .write_enabled = true,
            .compare = SG_COMPAREFUNC_LESS_EQUAL
        }
    });

    state.pass_action = (sg_pass_action){
        .colors[0] = {
            .action = SG_ACTION_CLEAR,
            .value = { 0.2f, 0.71f, 0.92f, 1.f }
        }
    };

    chip8_init();
    chip8_load_data(dump_breakout_ch8, sizeof(dump_breakout_ch8));
    // if (chip8_load("roms/Breakout (Brix hack) [David Winter, 1997].ch8")) {
        // printf("couldn't load chip8 cart\n");
        // exit(-1);
    // }

    state.last_time = stm_now();
}

void frame(void) {
    // == update =====================
    for(int i = 0; i < SPEED_MULTIPLIER; ++i)
        chip8_update();

    // == render =====================

    sgl_viewport(0, 0, sapp_width(), sapp_height(), true);
    sgl_defaults();
    
    chip8_render();

    sg_begin_default_pass(&state.pass_action, sapp_width(), sapp_height());
    sgl_draw();
    sg_end_pass();
    sg_commit();
}

void input(const sapp_event *e) {
    chip8_input(e);
}

void cleanup(void) {
    sgl_shutdown();
    sg_shutdown();
}

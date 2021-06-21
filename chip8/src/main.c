#include <stdio.h>
#include <sokol/sokol.h>

void init(void);
void frame(void);
void input(const sapp_event *e);
void cleanup(void);

static struct {
    sg_pass_action pass_action;
    sg_image img;
    sgl_pipeline pip;
} state;

sapp_desc sokol_main(int argc, char **argv) {
    return (sapp_desc) {
        .width = 640,
        .height = 480,
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
            .value = { 0.f, 0.f, 0.f, 1.f }
        }
    };
}

void frame(void) {
    sgl_viewport(0, 0, sapp_width(), sapp_height(), true);

    sgl_defaults();
    sgl_begin_triangles();
        sgl_v2f_c3b( 0.0f,  0.5f, 255, 0, 0);
        sgl_v2f_c3b(-0.5f, -0.5f, 0,   0, 255);
        sgl_v2f_c3b( 0.5f, -0.5f, 0, 255, 0);
    sgl_end();

    sg_begin_default_pass(&state.pass_action, sapp_width(), sapp_height());
    sgl_draw();
    sg_end_pass();
    sg_commit();
}

void input(const sapp_event *e) {
}

void cleanup(void) {
    sgl_shutdown();
    sg_shutdown();
}

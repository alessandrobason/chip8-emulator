#ifndef CHIP8_H
#define CHIP8_H

#include "types.h"
typedef struct sapp_event sapp_event;

void chip8_init();
int  chip8_load_data(void *data, u32 size);
int  chip8_load_file(const char *fname);
void chip8_update();
void chip8_input(const sapp_event *e);
void chip8_render();
void chip8_cleanup();

#endif
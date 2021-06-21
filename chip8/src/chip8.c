#include "chip8.h"
#include "chip8_font.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
	u8 registers[16];
	u8 memory[4096];
	u16 index;
	u16 pc;
	u16 stack[16];
	u8 sp;
	u8 delay_timer;
	u8 sound_timer;
	u8 keypad[16];
	u32 video[64 * 32];
	u16 opcode;
} chip8_t;

enum {
	START_ADDRESS = 0x200,
	FONTSET_START_ADDRESS = 0x50
};

#define PANIC(msg, tag) do { puts("ERROR: " msg "\n"); goto tag; } while(0)

static chip8_t chip8;

/* INSTRUCTIONS */
static inline u8 random_byte();

static inline void CLS(); // 00E0
static inline void RET(); // 00EE
static inline void JP_ADDR(); // 1NNNN
/****************/

void chip8_init() {
	chip8 = (chip8_t){
		.pc = START_ADDRESS
	};

	memcpy(&chip8.memory[FONTSET_START_ADDRESS], fontset, FONTSET_SIZE);

	srand(time(NULL));
}

int chip8_load(const char *fname) {
	int status = -1;

	FILE *f = fopen(fname, "rb");
	if (!f)
		PANIC("couldn't open file", failed_open);

	// get file size
	fseek(f, 0, SEEK_END);
	long fsize = ftell(f);
	fseek(f, 0, SEEK_SET);

	// allocate buffer
	u8 *buf = (u8 *)malloc(fsize);
	if (!buf)
		PANIC("couldn't allocate buffer", failed_malloc);

	// read whole file
	size_t size_read = fread(buf, fsize, 1, f);
	if (size_read != fsize)
		PANIC("size read different from file size", failed_fread);

	// load the ROM contents into chip8's memory
	memcpy(&chip8.memory[START_ADDRESS], buf, fsize);

	status = 0;

failed_fread:
	free(buf);
failed_malloc:
	fclose(f);
failed_open:
	return status;
}

// == INSTRUCTIONS ============================================

static inline u8 random_byte() {
	return (u8)(rand() % 256);
}
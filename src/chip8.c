#include "chip8.h"
#include "chip8_font.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <time.h>
#include <sokol/sokol.h>

//#define USE_ORIGINAL

enum {
	START_ADDRESS = 0x200,
	FONTSET_START_ADDRESS = 0x50,
	DISPLAY_WIDTH = 64,
	DISPLAY_HEIGHT = 32,
};

typedef void (*chip8_func)(void);

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
	u16 opcode;
	chip8_func table[0xf + 1];
	chip8_func table_0[0xe + 1];
	chip8_func table_8[0xe + 1];
	chip8_func table_e[0xe + 1];
	chip8_func table_f[0x65 + 1];

	//u32 display[DISPLAY_WIDTH * DISPLAY_HEIGHT];
	u32 display[DISPLAY_HEIGHT][DISPLAY_WIDTH];

	// sokol stuff
	sg_image sokol_img;
} chip8_t;

#define PANIC(msg, tag) do { puts("ERROR: " msg "\n"); goto tag; } while(0)

static chip8_t chip8;

static inline void update_screen(void);
static inline u8 random_byte();

/* INSTRUCTIONS */

static inline void goto_table_0(void);
static inline void goto_table_8(void);
static inline void goto_table_e(void);
static inline void goto_table_f(void);

#define DEFINE_OPERATION(name) static inline void name (void)

DEFINE_OPERATION(OP_NULL);   // not yet defined

DEFINE_OPERATION(CLS_00E0);  // clear screen
DEFINE_OPERATION(RET_00EE);  // return from subroutine 

DEFINE_OPERATION(JP_1nnn);   // jump to location nnn
DEFINE_OPERATION(CALL_2nnn); // call subroutine at nnn
DEFINE_OPERATION(SE_3xkk);   // skip next instruction if Vx == kk 
DEFINE_OPERATION(SNE_4xkk);  // skip next instruction if Vx != kk
DEFINE_OPERATION(SE_5xy0);   // skip next instruction if Vx == Vy
DEFINE_OPERATION(LD_6xkk);   // set reg Vx to kk
DEFINE_OPERATION(ADD_7xkk);  // Vx += kk
DEFINE_OPERATION(SNE_9xy0);  // skip next instruction if Vx != Vy

DEFINE_OPERATION(LD_8xy0);	 // set Vx = Vy
DEFINE_OPERATION(OR_8xy1);	 // set Vx = Vx | Vy
DEFINE_OPERATION(AND_8xy2);	 // set Vx = Vx & Vy
DEFINE_OPERATION(XOR_8xy3);	 // set Vx = Vx ^ Vy
DEFINE_OPERATION(ADD_8xy4);	 // set Vx += Vy, VF = carry
DEFINE_OPERATION(SUB_8xy5);	 // set Vx -= Vy, VF = NOT borrow
DEFINE_OPERATION(SHR_8xy6);	 // set Vx = Vx SHR 1
DEFINE_OPERATION(SUBN_8xy7); // set Vx = Vy - Vx, VF = NOT borrow
DEFINE_OPERATION(SHL_8xyE);  // set Vx = Vx SHL 1 

DEFINE_OPERATION(LD_Annn);   // set reg I to xxx
DEFINE_OPERATION(JP_Bnnn);   // jump to nnn + V0
DEFINE_OPERATION(RND_Cxkk);  // Vx = rnd & kk
DEFINE_OPERATION(DRW_Dxyn);  // display n-byte sprite starting at memory location (Vx, Vy)

DEFINE_OPERATION(SKP_Ex9E);  // skip next instruction if key with the value of Vx is pressed
DEFINE_OPERATION(SKNP_ExA1); // skip next instruction if key with the value of Vx is not pressed

DEFINE_OPERATION(LD_Fx07);   // set Vx = delay timer
DEFINE_OPERATION(LD_Fx0a);	 // wait for a key press, store value of key in Vx
DEFINE_OPERATION(LD_Fx15);	 // set delay timer = Vx
DEFINE_OPERATION(LD_Fx18);	 // set sound timer = Vx
DEFINE_OPERATION(ADD_Fx1E);	 // set I += Vx
DEFINE_OPERATION(LD_Fx29);	 // set I = location of sprite for digit Vx
DEFINE_OPERATION(LD_Fx33);	 // store BCD representation of Vx in memory location I, I+1, I+2
DEFINE_OPERATION(LD_Fx55);	 // store registers V0 to Vx in memory starting at location I
DEFINE_OPERATION(LD_Fx65);	 // read registers V0 to Vx in memory starting at location I

/****************/

#define ARR_SIZE(arr) sizeof(arr) / sizeof(arr[0])

void chip8_init() {
	chip8 = (chip8_t){
		.pc = START_ADDRESS,
	};
	
	memcpy(&chip8.memory[FONTSET_START_ADDRESS], fontset, FONTSET_SIZE);
	
	// clear the screen
	CLS_00E0();

	// Chip8 display init
	chip8.sokol_img = sg_make_image(&(sg_image_desc) {
		.width = DISPLAY_WIDTH,
		.height = DISPLAY_HEIGHT,
		.pixel_format = SG_PIXELFORMAT_RGBA8,
		.usage = SG_USAGE_DYNAMIC
	});

	// initialize function pointer table
	// table
	for (int i = 0; i < ARR_SIZE(chip8.table); ++i)
		chip8.table[i] = OP_NULL;
	// table 0x00xx
	for(int i = 0; i < ARR_SIZE(chip8.table_0); ++i)
		chip8.table_0[i] = OP_NULL;
	// table 0x8xxx
	for(int i = 0; i < ARR_SIZE(chip8.table_8); ++i)
		chip8.table_8[i] = OP_NULL;
	// table 0xExxx
	for(int i = 0; i < ARR_SIZE(chip8.table_e); ++i)
		chip8.table_e[i] = OP_NULL;
	// table 0xFxxx
	for(int i = 0; i < ARR_SIZE(chip8.table_f); ++i)
		chip8.table_f[i] = OP_NULL;

	chip8.table[0x0] = goto_table_0;

	chip8.table[0x1] = JP_1nnn;
	chip8.table[0x2] = CALL_2nnn;
	chip8.table[0x3] = SE_3xkk;
	chip8.table[0x4] = SNE_4xkk;
	chip8.table[0x5] = SE_5xy0;
	chip8.table[0x6] = LD_6xkk;
	chip8.table[0x7] = ADD_7xkk;
	chip8.table[0x8] = goto_table_8;
	chip8.table[0x9] = SNE_9xy0;

	chip8.table[0xa] = LD_Annn;
	chip8.table[0xb] = JP_Bnnn;
	chip8.table[0xc] = RND_Cxkk;
	chip8.table[0xd] = DRW_Dxyn;
	chip8.table[0xe] = goto_table_e;
	chip8.table[0xf] = goto_table_f;

	chip8.table_0[0x0] = CLS_00E0;
	chip8.table_0[0xE] = RET_00EE;

	chip8.table_8[0x0] = LD_8xy0;
	chip8.table_8[0x1] = OR_8xy1;
	chip8.table_8[0x2] = AND_8xy2;
	chip8.table_8[0x3] = XOR_8xy3;
	chip8.table_8[0x4] = ADD_8xy4;
	chip8.table_8[0x5] = SUB_8xy5;
	chip8.table_8[0x6] = SHR_8xy6;
	chip8.table_8[0x7] = SUBN_8xy7;
	chip8.table_8[0xE] = SHL_8xyE;

	chip8.table_e[0xE] = SKP_Ex9E;
	chip8.table_e[0x1] = SKNP_ExA1;

	chip8.table_f[0x07] = LD_Fx07;
	chip8.table_f[0x0a] = LD_Fx0a;
	chip8.table_f[0x15] = LD_Fx15;
	chip8.table_f[0x18] = LD_Fx18;
	chip8.table_f[0x1e] = ADD_Fx1E;
	chip8.table_f[0x29] = LD_Fx29;
	chip8.table_f[0x33] = LD_Fx33;
	chip8.table_f[0x55] = LD_Fx55;
	chip8.table_f[0x65] = LD_Fx65;

	srand((u32)time(NULL));
}

int  chip8_load_data(void *data, u32 size) {
	int status = -1;

	memcpy(&chip8.memory[START_ADDRESS], data, size);
	status = 0;
	
	return status;
}

int chip8_load_file(const char *fname) {
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
	size_t items_read = fread(buf, fsize, 1, f);
	if (items_read != 1)
		PANIC("EOF reached before reading whole file", failed_fread);

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

void chip8_update() {
	// fetch
	chip8.opcode = (chip8.memory[chip8.pc] << 8) | chip8.memory[chip8.pc + 1];
	chip8.pc += 2;

	// decode
	// get the upper 4 bits
	u8 decoded = (chip8.opcode & 0xF000U) >> 12;
#ifndef NDEBUG
	printf("instruction [%02x]\n", chip8.opcode);
#endif

	// execute
	chip8.table[decoded]();

	if (chip8.delay_timer > 0)
		chip8.delay_timer--;

	if (chip8.sound_timer > 0)
		chip8.sound_timer--;
}

void chip8_input(const sapp_event *e) {
	if (e->type == SAPP_EVENTTYPE_KEY_DOWN || e->type == SAPP_EVENTTYPE_KEY_UP) {
		u8 is_down = e->type == SAPP_EVENTTYPE_KEY_DOWN;
		switch (e->key_code) {
		case SAPP_KEYCODE_X: chip8.keypad[0] = is_down; break;
		case SAPP_KEYCODE_1: chip8.keypad[1] = is_down; break;
		case SAPP_KEYCODE_2: chip8.keypad[2] = is_down; break;
		case SAPP_KEYCODE_3: chip8.keypad[3] = is_down; break;

		case SAPP_KEYCODE_Q: chip8.keypad[4] = is_down; break;
		case SAPP_KEYCODE_W: chip8.keypad[5] = is_down; break;
		case SAPP_KEYCODE_E: chip8.keypad[6] = is_down; break;

		case SAPP_KEYCODE_A: chip8.keypad[7] = is_down; break;
		case SAPP_KEYCODE_S: chip8.keypad[8] = is_down; break;
		case SAPP_KEYCODE_D: chip8.keypad[9] = is_down; break;

		case SAPP_KEYCODE_Z: chip8.keypad[10] = is_down; break;
		case SAPP_KEYCODE_C: chip8.keypad[11] = is_down; break;

		case SAPP_KEYCODE_4: chip8.keypad[12] = is_down; break;
		case SAPP_KEYCODE_R: chip8.keypad[13] = is_down; break;
		case SAPP_KEYCODE_F: chip8.keypad[14] = is_down; break;
		case SAPP_KEYCODE_V: chip8.keypad[15] = is_down; break;

		default: break;
		}
	}
}

void chip8_render() {
	update_screen();

	sgl_enable_texture();
	sgl_texture(chip8.sokol_img);

	sgl_push_matrix();
		sgl_scale(0.75f, 0.75f, 1.f);
		sgl_begin_quads();
			sgl_v2f_t2f(-1.f, -1.f, 0.f, 1.f);
			sgl_v2f_t2f(-1.f,  1.f, 0.f, 0.f);
			sgl_v2f_t2f( 1.f,  1.f, 1.f, 0.f);
			sgl_v2f_t2f( 1.f, -1.f, 1.f, 1.f);
		sgl_end();
	sgl_pop_matrix();
}

void chip8_cleanup() {

}

static inline void update_screen(void) {
	sg_update_image(chip8.sokol_img, &(sg_image_data) {
		.subimage[0][0] = {
			.ptr = chip8.display,
			.size = DISPLAY_WIDTH * DISPLAY_HEIGHT
		}
	});
}

// == GOTO TABLE ============================================

inline void goto_table_0(void) {
	chip8.table_0[chip8.opcode & 0x000F]();
}

inline void goto_table_8(void) {
	chip8.table_8[chip8.opcode & 0x000F]();
}

inline void goto_table_e(void) {
	chip8.table_e[chip8.opcode & 0x000F]();
}

inline void goto_table_f(void) {
	chip8.table_f[chip8.opcode & 0x00FF]();
}

// == INSTRUCTIONS ============================================

void OP_NULL(void) {
	printf("this operations hasn't been defined yet %02x\n", chip8.opcode);
	exit(-1);
}

void CLS_00E0(void) {
	memset(chip8.display, 0x00, sizeof(u32) * DISPLAY_WIDTH * DISPLAY_HEIGHT);
}

void RET_00EE(void) {
	/* get address at the top of the stack and jump to it */
	chip8.pc = chip8.stack[--chip8.sp];
}

void JP_1nnn(void) {
	/* set program counter to nnn */
	u16 address = chip8.opcode & 0x0FFF;
	chip8.pc = address;
}

void CALL_2nnn(void) {
	/* add address to the top of the stack */
	u16 address = chip8.opcode & 0x0FFF;
	chip8.stack[chip8.sp++] = chip8.pc;
	chip8.pc = address;
}

void SE_3xkk(void) {
	/* skip to next instruction if register Vx == kk */
	u8 vx = (chip8.opcode & 0x0F00) >> 8;
	u8 kk = chip8.opcode & 0x00FF;

	if (chip8.registers[vx] == kk)
		chip8.pc += 2;
}

void SNE_4xkk(void) {
	/* skip to next instruction if register Vx != kk */
	u8 vx = (chip8.opcode & 0x0F00) >> 8;
	u8 kk = chip8.opcode & 0x00FF;

	if (chip8.registers[vx] != kk)
		chip8.pc += 2;
}

void SE_5xy0(void) {
	/* skip to next instruction if register Vx == register Vy */
	u8 vx = (chip8.opcode & 0x0F00) >> 8;
	u8 vy = (chip8.opcode & 0x00F0) >> 4;

	if (chip8.registers[vx] == chip8.registers[vy])
		chip8.pc += 2;
}

void LD_6xkk(void) {
	/* load value kk into register Vx */
	u8 vx = (chip8.opcode & 0x0F00) >> 8;
	u8 value = chip8.opcode & 0x00FF;

	chip8.registers[vx] = value;
}

void ADD_7xkk(void) {
	/* add kk to register Vx */
	u8 vx = (chip8.opcode & 0x0F00) >> 8;
	u8 value = chip8.opcode & 0x00FF;

	chip8.registers[vx] += value;
}

void LD_8xy0(void) {
	/* Vx = Vy */
	u8 vx = (chip8.opcode & 0x0F00) >> 8;
	u8 vy = (chip8.opcode & 0x00F0) >> 4;

	chip8.registers[vx] = chip8.registers[vy];
}

void OR_8xy1(void) {
	/* Vx |= Vy */
	u8 vx = (chip8.opcode & 0x0F00) >> 8;
	u8 vy = (chip8.opcode & 0x00F0) >> 4;

	chip8.registers[vx] |= chip8.registers[vy];
}

void AND_8xy2(void) {
	/* Vx &= Vy */
	u8 vx = (chip8.opcode & 0x0F00) >> 8;
	u8 vy = (chip8.opcode & 0x00F0) >> 4;

	chip8.registers[vx] &= chip8.registers[vy];
}

void XOR_8xy3(void) {
	/* Vx ^= Vy */
	u8 vx = (chip8.opcode & 0x0F00) >> 8;
	u8 vy = (chip8.opcode & 0x00F0) >> 4;

	chip8.registers[vx] ^= chip8.registers[vy];
}

void ADD_8xy4(void) {
	/* Vx += Vy, VF = carry */
	u8 vx = (chip8.opcode & 0x0F00) >> 8;
	u8 vy = (chip8.opcode & 0x00F0) >> 4;

	u16 res = (u16)chip8.registers[vx] + chip8.registers[vy];
	chip8.registers[0xF] = res > 255;
	chip8.registers[vx] = (u8)res;
}

void SUB_8xy5(void) {
	/* Vx += Vy, VF = NOT borrow */
	u8 vx = (chip8.opcode & 0x0F00) >> 8;
	u8 vy = (chip8.opcode & 0x00F0) >> 4;

	chip8.registers[0xF] = chip8.registers[vx] > chip8.registers[vy];
	chip8.registers[vx] -= chip8.registers[vy];
}

void SHR_8xy6(void) {
	/* if Vx least significant bit is 1 then VF is set to 1,
	 * otherwise it is set to 0
	 * Vx is then divided by 2
	 */
	u8 vx = (chip8.opcode & 0x0F00) >> 8;
	u8 vy = (chip8.opcode & 0x00F0) >> 4;

#ifdef USE_ORIGINAL
	chip8.registers[vx] = chip8.registers[vy];
#endif

	chip8.registers[0xF] = chip8.registers[vx] & 0x1;
	chip8.registers[vx] >>= 1;
}

void SUBN_8xy7(void) {
	/* Vx = Vy - Vx, VF = NOT borrow */
	u8 vx = (chip8.opcode & 0x0F00) >> 8;
	u8 vy = (chip8.opcode & 0x00F0) >> 4;

	chip8.registers[0xF] = chip8.registers[vy] > chip8.registers[vx];
	chip8.registers[vx] = chip8.registers[vy] - chip8.registers[vx];
}

void SHL_8xyE(void) {
	/* if Vx most significant bit is 1 then VF is set to 1,
	 * otherwise it is set to 0
	 * Vx is then multiplied by 2
	 */
	u8 vx = (chip8.opcode & 0x0F00) >> 8;

#ifdef USE_ORIGINAL
	u8 vy = (chip8.opcode & 0x00F0) >> 4;
	chip8.registers[vx] = chip8.registers[vy];
#endif

	// set VF to the MSB
	chip8.registers[0xF] = (chip8.registers[vx] & 0x80) >> 7;
	chip8.registers[vx] <<= 1;
}

void SNE_9xy0(void) {
	/* skip to next instruction if register Vx == register Vy */
	u8 vx = (chip8.opcode & 0x0F00) >> 8;
	u8 vy = (chip8.opcode & 0x00F0) >> 4;

	if (chip8.registers[vx] != chip8.registers[vy])
		chip8.pc += 2;
}

void LD_Annn(void) {
	/* load value nnn into register I */
	u16 value = chip8.opcode & 0x0FFF;

	chip8.index = value;
}

void JP_Bnnn(void) {
	/* Set program counter to nnn + V0 */
	u16 address = chip8.opcode & 0x0FFF;

#ifdef USE_ORIGINAL
	u8 vx = (chip8.opcode & 0x0F00) >> 8;
	address += chip8.registers[vx];
#else
	address += chip8.registers[0x0];
#endif

	chip8.pc = address;
}

void RND_Cxkk(void) {
	/* set Vx to a random byte & kk */
	u8 vx = (chip8.opcode & 0x0F00) >> 8;
	u8 kk = chip8.opcode & 0x00FF;
	u8 rnd = (u8)(rand() % 256);
	
	chip8.registers[vx] = rnd & kk;
}

void DRW_Dxyn(void) {
	/* Read n bytes from memory starting at addres stored in I
	 * these bytes are then displayed as sprites on screen at coordinates
	 * stored in registers vx and vy, the coordinates wrap
	 * sprites are XORed onto the display, if this causes any pixels
	 * to be eares VF is set to 1, otherwise to 0
	 * the sprite is guaranteed to be 8 pixels wide
	 */

	u8 vx = (chip8.opcode & 0x0F00) >> 8;
	u8 vy = (chip8.opcode & 0x00F0) >> 4;
	u8 height  =  chip8.opcode & 0x000F;

	u8 x = chip8.registers[vx];
	u8 y = chip8.registers[vy];

	chip8.registers[0xF] = 0;

	for (u8 row = 0; row < height; ++row) {
		u8 sprite_byte = chip8.memory[chip8.index + row];
		for (u8 column = 0; column < 8; ++column) {
			u8 sprite_px = sprite_byte & (0x80 >> column);
			u8 xpos = (x + column) % DISPLAY_WIDTH;
			u8 ypos = (y + row) % DISPLAY_HEIGHT;
			u32 *screen_px = &chip8.display[ypos][xpos];
			
			// if this is a sprite pixel
			if (sprite_px) {
				// if the same pixel is already on, set the 
				// VF register to 1
				if (*screen_px)
					chip8.registers[0xF] = 1;
				
				// XOR pixel
				*screen_px ^= 0xFFFFFFFF;
			}
		}
	}
}

void SKP_Ex9E(void) {
	/* pc += 2 if key Vx is pressed */
	u8 vx = (chip8.opcode & 0x0F00) >> 8;

	if (chip8.keypad[chip8.registers[vx]])
		chip8.pc += 2;
}


void SKNP_ExA1(void) {
	/* pc += 2 if key Vx is NOT pressed */
	u8 vx = (chip8.opcode & 0x0F00) >> 8;

	if (!chip8.keypad[chip8.registers[vx]])
		chip8.pc += 2;
}

void LD_Fx07(void) {
	/* Vx = delay timer */
	u8 vx = (chip8.opcode & 0x0F00) >> 8;
	chip8.registers[vx] = chip8.delay_timer;
}

void LD_Fx0a(void) {
	/* wait for a key to be pressed (by decreasing pc)
	 * the value of the key is stored in Vx
	 */
	u8 vx = (chip8.opcode & 0x0F00) >> 8;
	
	for (u8 i = 0; i < 16; ++i) {
		if (chip8.keypad[i]) {
			chip8.registers[vx] = i;
			return;
		}
	}
	
	chip8.pc -= 2;
}

void LD_Fx15(void) {
	/* delay timer = Vx */
	u8 vx = (chip8.opcode & 0x0F00) >> 8;
	chip8.delay_timer = vx;
}

void LD_Fx18(void) {
	/* sound timer = Vx */
	u8 vx = (chip8.opcode & 0x0F00) >> 8;
	chip8.sound_timer = chip8.registers[vx];
}

void ADD_Fx1E(void) {
	/* register I += Vx */
	u8 vx = (chip8.opcode & 0x0F00) >> 8;
	chip8.index += vx;
}

void LD_Fx29(void) {
	/* returns position in memory of digit Vx from font */
	u8 vx = (chip8.opcode & 0x0F00) >> 8;
	u8 digit = chip8.registers[vx];

	chip8.index = FONTSET_START_ADDRESS + (5 * digit);
}

void LD_Fx33(void) {
	/* store in BCD representation value of Vx in
	 * memory in I, I+1 and I+2.
	 * BCD means:
	 * value = 154
	 * mem[i+0] = 1 -> [1]54
	 * mem[i+1] = 5 -> 1[5]4
	 * mem[i+2] = 4 -> 15[4]
	 */
	
	u8 vx = (chip8.opcode & 0x0F00) >> 8;
	u8 value = chip8.registers[vx];

	chip8.memory[chip8.index + 2] = value % 10;
	value /= 10;

	chip8.memory[chip8.index + 1] = value % 10;
	value /= 10;

	chip8.memory[chip8.index] = value % 10;
}

void LD_Fx55(void) {
	/* store register from V0 to Vx in memory from I */
	u8 vx = (chip8.opcode & 0x0F00) >> 8;

	for (u8 i = 0; i < vx; ++i) {
#ifdef USE_ORIGINAL
		chip8.memory[chip8.index++] = chip8.registers[i];
#else
		chip8.memory[chip8.index + i] = chip8.registers[i];
#endif
	}
}

void LD_Fx65(void) {
	/* read memory from i in registers V0 to Vx */
	u8 vx = (chip8.opcode & 0x0F00) >> 8;

	for (u8 i = 0; i < vx; ++i) {
#ifdef USE_ORIGINAL
		chip8.registers[i] = chip8.memory[chip8.index++];
#else
		chip8.registers[i] = chip8.memory[chip8.index + i];
#endif
	}
}
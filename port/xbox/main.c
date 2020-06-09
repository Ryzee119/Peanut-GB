/**
 * MIT License
 * Copyright (c) 2018 Mahyar Koshkouei
 * Copyright (c) 2020 Ryan Wendland (Original Xbox Port)
 *
 * An example of using the peanut_gb.h library. This example application uses
 * SDL2 to draw the screen and get input.
 */
#ifndef NXDK
#define NXDK
#endif
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <SDL.h>
#include <SDL_ttf.h>


#include "minigb_apu/minigb_apu.h"
#include "../../peanut_gb.h"

#ifndef NXDK
#include "nativefiledialog/src/include/nfd.h"
#else
#define XBOX_SAVE_PATH "D:\\Saves"
#define XBOX_ROM_PATH "D:\\Roms"
#define XBOX_SCREENSHOT_PATH "D:\\Screenshots"
#define MENU_FONT_PATH "D:\\font.ttf"
#define XBOX_SCREEN_WIDTH 640
#define XBOX_SCREEN_HEIGHT 480
extern int nextRow;
extern int nextCol;
#ifdef DEBUG
#define printf(fmt, ...) do { nextRow = 12; debugPrint(fmt, __VA_ARGS__); Sleep(100); } while(0)
#define puts(a) debugPrint(a)
#endif
#include <assert.h>
#include <windows.h>
#include <nxdk/mount.h>
#include <hal/xbox.h>
#include <hal/video.h>
#include <hal/debug.h>
#include "menu.h"
#endif

struct priv_t
{
	/* Pointer to allocated memory holding GB file. */
	uint8_t *rom;
	/* Pointer to allocated memory holding save file. */
	uint8_t *cart_ram;

	/* Colour palette for each BG, OBJ0, and OBJ1. */
	uint16_t selected_palette[3][4];
	uint16_t fb[LCD_HEIGHT][LCD_WIDTH];
};

/**
 * Returns a byte from the ROM file at the given address.
 */
uint8_t gb_rom_read(struct gb_s *gb, const uint_fast32_t addr)
{
	const struct priv_t * const p = gb->direct.priv;
	return p->rom[addr];
}

/**
 * Returns a byte from the cartridge RAM at the given address.
 */
uint8_t gb_cart_ram_read(struct gb_s *gb, const uint_fast32_t addr)
{
	const struct priv_t * const p = gb->direct.priv;
	return p->cart_ram[addr];
}

/**
 * Writes a given byte to the cartridge RAM at the given address.
 */
void gb_cart_ram_write(struct gb_s *gb, const uint_fast32_t addr,
		       const uint8_t val)
{
	const struct priv_t * const p = gb->direct.priv;
	p->cart_ram[addr] = val;
}

/**
 * Returns a pointer to the allocated space containing the ROM. Must be freed.
 */
uint8_t *read_rom_to_ram(const char *file_name)
{
	FILE *rom_file = fopen(file_name, "rb");
	size_t rom_size;
	uint8_t *rom = NULL;

	if(rom_file == NULL)
		return NULL;

	fseek(rom_file, 0, SEEK_END);
	rom_size = ftell(rom_file);
	rewind(rom_file);
	rom = malloc(rom_size);

	if(fread(rom, sizeof(uint8_t), rom_size, rom_file) != rom_size)
	{
		free(rom);
		fclose(rom_file);
		return NULL;
	}

	fclose(rom_file);
	return rom;
}

void read_cart_ram_file(const char *save_file_name, uint8_t **dest,
			const size_t len)
{
	FILE *f;

	/* If save file not required. */
	if(len == 0)
	{
		*dest = NULL;
		return;
	}

	/* Allocate enough memory to hold save file. */
	if((*dest = malloc(len)) == NULL)
	{
		printf("%d: %s\n", __LINE__, strerror(errno));
		exit(EXIT_FAILURE);
	}

	f = fopen(save_file_name, "rb");

	/* It doesn't matter if the save file doesn't exist. We initialise the
	 * save memory allocated above. The save file will be created on exit. */
	if(f == NULL)
	{
		memset(*dest, 0xFF, len);
		return;
	}

	/* Read save file to allocated memory. */
	fread(*dest, sizeof(uint8_t), len, f);
	fclose(f);
}

void write_cart_ram_file(const char *save_file_name, uint8_t **dest,
			 const size_t len)
{
	FILE *f;

	if(len == 0 || *dest == NULL)
		return;

	if((f = fopen(save_file_name, "wb")) == NULL)
	{
		puts("Unable to open save file.");
		printf("%d: %s\n", __LINE__, strerror(errno));
		//exit(EXIT_FAILURE);
		return;
	}

	/* Record save file. */
	fwrite(*dest, sizeof(uint8_t), len, f);
	fclose(f);
}

/**
 * Handles an error reported by the emulator. The emulator context may be used
 * to better understand why the error given in gb_err was reported.
 */
void gb_error(struct gb_s *gb, const enum gb_error_e gb_err, const uint16_t val)
{
	struct priv_t *priv = gb->direct.priv;

	switch(gb_err)
	{
	case GB_INVALID_OPCODE:
		/* We compensate for the post-increment in the __gb_step_cpu
		 * function. */
		fprintf(stdout, "Invalid opcode %#04x at PC: %#06x, SP: %#06x\n",
			val,
			gb->cpu_reg.pc - 1,
			gb->cpu_reg.sp);
		break;

	/* Ignoring non fatal errors. */
	case GB_INVALID_WRITE:
	case GB_INVALID_READ:
		return;

	default:
		printf("Unknown error");
		break;
	}

	fprintf(stderr, "Error. Press q to exit, or any other key to continue.");

	if(getchar() == 'q')
	{
		/* Record save file. */
		write_cart_ram_file("D:\\recovery.sav", &priv->cart_ram,
				    gb_get_save_size(gb));

		free(priv->rom);
		free(priv->cart_ram);
		exit(EXIT_FAILURE);
	}

	return;
}

/**
 * Automatically assigns a colour palette to the game using a given game
 * checksum.
 * TODO: Not all checksums are programmed in yet because I'm lazy.
 */
void auto_assign_palette(struct priv_t *priv, uint8_t game_checksum)
{
	size_t palette_bytes = 3 * 4 * sizeof(uint16_t);

	switch(game_checksum)
	{
	/* Balloon Kid and Tetris Blast */
	case 0x71:
	case 0xFF:
	{
		const uint16_t palette[3][4] =
		{
			{ 0x7FFF, 0x7E60, 0x7C00, 0x0000 }, /* OBJ0 */
			{ 0x7FFF, 0x7E60, 0x7C00, 0x0000 }, /* OBJ1 */
			{ 0x7FFF, 0x7E60, 0x7C00, 0x0000 }  /* BG */
		};
		memcpy(priv->selected_palette, palette, palette_bytes);
		break;
	}

	/* Pokemon Yellow and Tetris */
	case 0x15:
	case 0xDB:
	case 0x95: /* Not officially */
	{
		const uint16_t palette[3][4] =
		{
			{ 0x7FFF, 0x7FE0, 0x7C00, 0x0000 }, /* OBJ0 */
			{ 0x7FFF, 0x7FE0, 0x7C00, 0x0000 }, /* OBJ1 */
			{ 0x7FFF, 0x7FE0, 0x7C00, 0x0000 }  /* BG */
		};
		memcpy(priv->selected_palette, palette, palette_bytes);
		break;
	}

	/* Donkey Kong */
	case 0x19:
	{
		const uint16_t palette[3][4] =
		{
			{ 0x7FFF, 0x7E10, 0x48E7, 0x0000 }, /* OBJ0 */
			{ 0x7FFF, 0x7E10, 0x48E7, 0x0000 }, /* OBJ1 */
			{ 0x7FFF, 0x7E60, 0x7C00, 0x0000 }  /* BG */
		};
		memcpy(priv->selected_palette, palette, palette_bytes);
		break;
	}

	/* Pokemon Blue */
	case 0x61:
	case 0x45:

	/* Pokemon Blue Star */
	case 0xD8:
	{
		const uint16_t palette[3][4] =
		{
			{ 0x7FFF, 0x7E10, 0x48E7, 0x0000 }, /* OBJ0 */
			{ 0x7FFF, 0x329F, 0x001F, 0x0000 }, /* OBJ1 */
			{ 0x7FFF, 0x329F, 0x001F, 0x0000 }  /* BG */
		};
		memcpy(priv->selected_palette, palette, palette_bytes);
		break;
	}

	/* Pokemon Red */
	case 0x14:
	{
		const uint16_t palette[3][4] =
		{
			{ 0x7FFF, 0x3FE6, 0x0200, 0x0000 }, /* OBJ0 */
			{ 0x7FFF, 0x7E10, 0x48E7, 0x0000 }, /* OBJ1 */
			{ 0x7FFF, 0x7E10, 0x48E7, 0x0000 }  /* BG */
		};
		memcpy(priv->selected_palette, palette, palette_bytes);
		break;
	}

	/* Pokemon Red Star */
	case 0x8B:
	{
		const uint16_t palette[3][4] =
		{
			{ 0x7FFF, 0x7E10, 0x48E7, 0x0000 }, /* OBJ0 */
			{ 0x7FFF, 0x329F, 0x001F, 0x0000 }, /* OBJ1 */
			{ 0x7FFF, 0x3FE6, 0x0200, 0x0000 }  /* BG */
		};
		memcpy(priv->selected_palette, palette, palette_bytes);
		break;
	}

	/* Kirby */
	case 0x27:
	case 0x49:
	case 0x5C:
	case 0xB3:
	{
		const uint16_t palette[3][4] =
		{
			{ 0x7D8A, 0x6800, 0x3000, 0x0000 }, /* OBJ0 */
			{ 0x001F, 0x7FFF, 0x7FEF, 0x021F }, /* OBJ1 */
			{ 0x527F, 0x7FE0, 0x0180, 0x0000 }  /* BG */
		};
		memcpy(priv->selected_palette, palette, palette_bytes);
		break;
	}

	/* Donkey Kong Land [1/2/III] */
	case 0x18:
	case 0x6A:
	case 0x4B:
	case 0x6B:
	{
		const uint16_t palette[3][4] =
		{
			{ 0x7F08, 0x7F40, 0x48E0, 0x2400 }, /* OBJ0 */
			{ 0x7FFF, 0x2EFF, 0x7C00, 0x001F }, /* OBJ1 */
			{ 0x7FFF, 0x463B, 0x2951, 0x0000 }  /* BG */
		};
		memcpy(priv->selected_palette, palette, palette_bytes);
		break;
	}

	/* Link's Awakening */
	case 0x70:
	{
		const uint16_t palette[3][4] =
		{
			{ 0x7FFF, 0x03E0, 0x1A00, 0x0120 }, /* OBJ0 */
			{ 0x7FFF, 0x329F, 0x001F, 0x001F }, /* OBJ1 */
			{ 0x7FFF, 0x7E10, 0x48E7, 0x0000 }  /* BG */
		};
		memcpy(priv->selected_palette, palette, palette_bytes);
		break;
	}

	/* Mega Man [1/2/3] & others I don't care about. */
	case 0x01:
	case 0x10:
	case 0x29:
	case 0x52:
	case 0x5D:
	case 0x68:
	case 0x6D:
	case 0xF6:
	{
		const uint16_t palette[3][4] =
		{
			{ 0x7FFF, 0x329F, 0x001F, 0x0000 }, /* OBJ0 */
			{ 0x7FFF, 0x3FE6, 0x0200, 0x0000 }, /* OBJ1 */
			{ 0x7FFF, 0x7EAC, 0x40C0, 0x0000 }  /* BG */
		};
		memcpy(priv->selected_palette, palette, palette_bytes);
		break;
	}

	default:
	{
		const uint16_t palette[3][4] =
		{
			{ 0x7FFF, 0x5294, 0x294A, 0x0000 },
			{ 0x7FFF, 0x5294, 0x294A, 0x0000 },
			{ 0x7FFF, 0x5294, 0x294A, 0x0000 }
		};
		printf("No palette found for 0x%02X.\n", game_checksum);
		memcpy(priv->selected_palette, palette, palette_bytes);
	}
	}
}

/**
 * Assigns a palette. This is used to allow the user to manually select a
 * different colour palette if one was not found automatically, or if the user
 * prefers a different colour palette.
 * selection is the requestion colour palette. This should be a maximum of
 * NUMBER_OF_PALETTES - 1. The default greyscale palette is selected otherwise.
 */
void manual_assign_palette(struct priv_t *priv, uint8_t selection)
{
#define NUMBER_OF_PALETTES 12
	size_t palette_bytes = 3 * 4 * sizeof(uint16_t);

	switch(selection)
	{
	/* 0x05 (Right) */
	case 0:
	{
		const uint16_t palette[3][4] =
		{
			{ 0x7FFF, 0x2BE0, 0x7D00, 0x0000 },
			{ 0x7FFF, 0x2BE0, 0x7D00, 0x0000 },
			{ 0x7FFF, 0x2BE0, 0x7D00, 0x0000 }
		};
		memcpy(priv->selected_palette, palette, palette_bytes);
		break;
	}

	/* 0x07 (A + Down) */
	case 1:
	{
		const uint16_t palette[3][4] =
		{
			{ 0x7FFF, 0x7FE0, 0x7C00, 0x0000 },
			{ 0x7FFF, 0x7FE0, 0x7C00, 0x0000 },
			{ 0x7FFF, 0x7FE0, 0x7C00, 0x0000 }
		};
		memcpy(priv->selected_palette, palette, palette_bytes);
		break;
	}

	/* 0x12 (Up) */
	case 2:
	{
		const uint16_t palette[3][4] =
		{
			{ 0x7FFF, 0x7EAC, 0x40C0, 0x0000 },
			{ 0x7FFF, 0x7EAC, 0x40C0, 0x0000 },
			{ 0x7FFF, 0x7EAC, 0x40C0, 0x0000 }
		};
		memcpy(priv->selected_palette, palette, palette_bytes);
		break;
	}

	/* 0x13 (B + Right) */
	case 3:
	{
		const uint16_t palette[3][4] =
		{
			{ 0x0000, 0x0210, 0x7F60, 0x7FFF },
			{ 0x0000, 0x0210, 0x7F60, 0x7FFF },
			{ 0x0000, 0x0210, 0x7F60, 0x7FFF }
		};
		memcpy(priv->selected_palette, palette, palette_bytes);
		break;
	}

	/* 0x16 (B + Left, DMG Palette) */
	default:
	case 4:
	{
		const uint16_t palette[3][4] =
		{
			{ 0x7FFF, 0x5294, 0x294A, 0x0000 },
			{ 0x7FFF, 0x5294, 0x294A, 0x0000 },
			{ 0x7FFF, 0x5294, 0x294A, 0x0000 }
		};
		memcpy(priv->selected_palette, palette, palette_bytes);
		break;
	}

	/* 0x17 (Down) */
	case 5:
	{
		const uint16_t palette[3][4] =
		{
			{ 0x7FF4, 0x7E52, 0x4A5F, 0x0000 },
			{ 0x7FF4, 0x7E52, 0x4A5F, 0x0000 },
			{ 0x7FF4, 0x7E52, 0x4A5F, 0x0000 }
		};
		memcpy(priv->selected_palette, palette, palette_bytes);
		break;
	}

	/* 0x19 (B + Up) */
	case 6:
	{
		const uint16_t palette[3][4] =
		{
			{ 0x7FFF, 0x7EAC, 0x40C0, 0x0000 },
			{ 0x7FFF, 0x7EAC, 0x40C0, 0x0000 },
			{ 0x7F98, 0x6670, 0x41A5, 0x2CC1 }
		};
		memcpy(priv->selected_palette, palette, palette_bytes);
		break;
	}

	/* 0x1C (A + Right) */
	case 7:
	{
		const uint16_t palette[3][4] =
		{
			{ 0x7FFF, 0x7E10, 0x48E7, 0x0000 },
			{ 0x7FFF, 0x7E10, 0x48E7, 0x0000 },
			{ 0x7FFF, 0x3FE6, 0x0198, 0x0000 }
		};
		memcpy(priv->selected_palette, palette, palette_bytes);
		break;
	}

	/* 0x0D (A + Left) */
	case 8:
	{
		const uint16_t palette[3][4] =
		{
			{ 0x7FFF, 0x7E10, 0x48E7, 0x0000 },
			{ 0x7FFF, 0x7EAC, 0x40C0, 0x0000 },
			{ 0x7FFF, 0x463B, 0x2951, 0x0000 }
		};
		memcpy(priv->selected_palette, palette, palette_bytes);
		break;
	}

	/* 0x10 (A + Up) */
	case 9:
	{
		const uint16_t palette[3][4] =
		{
			{ 0x7FFF, 0x3FE6, 0x0200, 0x0000 },
			{ 0x7FFF, 0x329F, 0x001F, 0x0000 },
			{ 0x7FFF, 0x7E10, 0x48E7, 0x0000 }
		};
		memcpy(priv->selected_palette, palette, palette_bytes);
		break;
	}

	/* 0x18 (Left) */
	case 10:
	{
		const uint16_t palette[3][4] =
		{
			{ 0x7FFF, 0x7E10, 0x48E7, 0x0000 },
			{ 0x7FFF, 0x3FE6, 0x0200, 0x0000 },
			{ 0x7FFF, 0x329F, 0x001F, 0x0000 }
		};
		memcpy(priv->selected_palette, palette, palette_bytes);
		break;
	}

	/* 0x1A (B + Down) */
	case 11:
	{
		const uint16_t palette[3][4] =
		{
			{ 0x7FFF, 0x329F, 0x001F, 0x0000 },
			{ 0x7FFF, 0x3FE6, 0x0200, 0x0000 },
			{ 0x7FFF, 0x7FE0, 0x3D20, 0x0000 }
		};
		memcpy(priv->selected_palette, palette, palette_bytes);
		break;
	}
	}

	return;
}

#if ENABLE_LCD
/**
 * Draws scanline into framebuffer.
 */
void lcd_draw_line(struct gb_s *gb, const uint8_t pixels[160],
		   const uint_fast8_t line)
{
	struct priv_t *priv = gb->direct.priv;

	for(unsigned int x = 0; x < LCD_WIDTH; x++)
	{
		priv->fb[line][x] = priv->selected_palette
				    [(pixels[x] & LCD_PALETTE_ALL) >> 4]
				    [pixels[x] & 3];
	}
}
#endif

/**
 * Saves the LCD screen as a 15-bit BMP file.
 */
void save_lcd_bmp(struct gb_s* gb, uint16_t fb[LCD_HEIGHT][LCD_WIDTH])
{
	/* Should be enough to record up to 828 days worth of frames. */
	static uint_fast32_t file_num = 0;
	char file_name[256];
	char title_str[16];
	FILE* f;
	#ifdef NXDK
	snprintf(file_name, 256, "%s\\%.16s_%010ld.bmp",
		 XBOX_SCREENSHOT_PATH, gb_get_rom_name(gb, title_str), file_num);
	#else
	snprintf(file_name, 32, "%.16s_%010ld.bmp",
		 gb_get_rom_name(gb, title_str), file_num);
	#endif

	f = fopen(file_name, "wb");

	const uint8_t bmp_hdr_rgb555[] = {
		0x42, 0x4d, 0x36, 0xb4, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x36, 0x00, 0x00, 0x00, 0x28, 0x00, 0x00, 0x00, 0xa0, 0x00,
		0x00, 0x00, 0x70, 0xff, 0xff, 0xff, 0x01, 0x00, 0x10, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0xb4, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00
	};

	if (f){
		fwrite(bmp_hdr_rgb555, sizeof(uint8_t), sizeof(bmp_hdr_rgb555), f);
		fwrite(fb, sizeof(uint16_t), LCD_HEIGHT * LCD_WIDTH, f);
		fclose(f);
		file_num++;

		/* Each dot shows a new frame being saved. */
		putc('.', stdout);
		fflush(stdout);
	}
}

int main(int argc, char **argv)
{
	XVideoSetMode(XBOX_SCREEN_WIDTH, XBOX_SCREEN_HEIGHT, 15, REFRESH_DEFAULT);
	size_t fb_size = XBOX_SCREEN_WIDTH * XBOX_SCREEN_HEIGHT * sizeof(uint16_t);
	extern uint8_t* _fb;
	_fb = (uint8_t*)MmAllocateContiguousMemoryEx(fb_size,
	                                            0,
	                                            0xFFFFFFFF,
	                                            0x1000,
	                                            PAGE_READWRITE | PAGE_WRITECOMBINE);
	memset(_fb, 0x00, fb_size);

	#define _PCRTC_START 0xFD600800
	*(unsigned int*)(_PCRTC_START) = (unsigned int)_fb & 0x03FFFFFF;
	BOOL mounted = nxMountDrive('E', "\\Device\\Harddisk0\\Partition1\\");
	assert(mounted);
	CreateDirectoryA(XBOX_SAVE_PATH, NULL);
	CreateDirectoryA(XBOX_SCREENSHOT_PATH, NULL);
	CreateDirectoryA(XBOX_ROM_PATH, NULL);

	struct gb_s gb;
	struct priv_t priv =
	{
		.rom = NULL,
		.cart_ram = NULL
	};
	const double target_speed_ms = 1000.0 / VERTICAL_SYNC;
	double speed_compensation = 0.0;
	unsigned int running = 1;
	SDL_Window *window;
	SDL_Renderer *renderer;
	SDL_Texture *texture;
	SDL_Event event;
	SDL_GameController *controller = NULL;
	uint_fast32_t new_ticks, old_ticks;
	enum gb_init_error_e gb_ret;
	unsigned int fast_mode = 1;
	unsigned int fast_mode_timer = 1;
	/* Record save file every 60 seconds. */
	int save_timer = 60;
	/* Must be freed */
	char *rom_file_name = NULL;
	char *save_file_name = NULL;
	int ret = EXIT_SUCCESS;


	menu_settings settings;
	strcpy(settings.title, "Peanut-GB for Xbox");
	strcpy(settings.sub_title, "A Game Boy (DMG) emulator");
	strcpy(settings.item_path, XBOX_ROM_PATH);
	strcpy(settings.font_path, MENU_FONT_PATH);
	settings.screen_width = XBOX_SCREEN_WIDTH;
	settings.screen_height = XBOX_SCREEN_HEIGHT;
	settings.max_items = 2048;
	init_menu(&settings);
	char* rom_name = malloc(256);
start:
	show_item_selection_menu(rom_name, 256);
	rom_file_name = malloc(strlen(XBOX_ROM_PATH) + 1 + strlen(rom_name) + 1);
	sprintf(rom_file_name,"%s\\%s", XBOX_ROM_PATH, rom_name);
	printf("Opening %s\n",rom_file_name);
	running = 1;
	/* Copy input ROM file to allocated memory. */
	if((priv.rom = read_rom_to_ram(rom_file_name)) == NULL)
	{
		printf("%d: %s\n", __LINE__, strerror(errno));
		ret = EXIT_FAILURE;
		goto out;
	}

	/* TODO: Sanity check input GB file. */

	/* Initialise emulator context. */
	gb_ret = gb_init(&gb, &gb_rom_read, &gb_cart_ram_read, &gb_cart_ram_write,
			 &gb_error, &priv);

	switch(gb_ret)
	{
	case GB_INIT_NO_ERROR:
		break;

	case GB_INIT_CARTRIDGE_UNSUPPORTED:
		puts("Unsupported cartridge.");
		ret = EXIT_FAILURE;
		goto out;

	case GB_INIT_INVALID_CHECKSUM:
		puts("Invalid ROM: Checksum failure.");
		ret = EXIT_FAILURE;
		goto out;

	default:
		printf("Unknown error: %d\n", gb_ret);
		ret = EXIT_FAILURE;
		goto out;
	}

	if(save_file_name == NULL){
		char title_str[32];
		unsigned char headerchecksum =  gb_rom_read(&gb, 0x014D);
		gb_get_rom_name(&gb, title_str);
		save_file_name = malloc(strlen(XBOX_SAVE_PATH) + 1 + strlen(title_str) +
		                 1/*_*/ + 2/*FF*/ + strlen(".sav") + 1/*TERMINATOR*/);
		sprintf(save_file_name,"%s\\%s_%02x.sav", XBOX_SAVE_PATH, title_str, headerchecksum);
	}

	/* Load Save File. */
	read_cart_ram_file(save_file_name, &priv.cart_ram, gb_get_save_size(&gb));

	/* Set the RTC of the game cartridge. Only used by games that support it. */
	{
		time_t rawtime;
		time(&rawtime);
		struct tm *timeinfo;
		timeinfo = localtime(&rawtime);

		if(timeinfo)
			gb_set_rtc(&gb, timeinfo);
	}

	/* Initialise frontend implementation, in this case, SDL2. */
	if(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER | SDL_INIT_AUDIO) < 0)
	{
		printf("Could not initialise SDL: %s\n", SDL_GetError());
		ret = EXIT_FAILURE;
		goto out;
	}

	SDL_AudioDeviceID dev;
	{
		SDL_AudioSpec want, have;

		want.freq = AUDIO_SAMPLE_RATE;
		want.format   = AUDIO_F32SYS,
		want.channels = 2;
		want.samples = AUDIO_SAMPLES;
		want.callback = audio_callback;
		want.userdata = NULL;

		printf("Audio driver: %s\n", SDL_GetAudioDeviceName(0, 0));

		if((dev = SDL_OpenAudioDevice(NULL, 0, &want, &have, 0)) == 0)
		{
			printf("SDL could not open audio device: %s\n", SDL_GetError());
			exit(EXIT_FAILURE);
		}

		audio_init();
		SDL_PauseAudioDevice(dev, 0);
	}

#if ENABLE_LCD
	gb_init_lcd(&gb, &lcd_draw_line);
#endif

	/* Allow the joystick input even if game is in background. */
	SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "1");

	/* Open the first available controller. */
	for(int i = 0; i < SDL_NumJoysticks(); i++)
	{
		if(!SDL_IsGameController(i))
			continue;

		controller = SDL_GameControllerOpen(i);

		if(controller)
		{
			printf("Game Controller %s connected.\n",
					SDL_GameControllerName(controller));
			break;
		}
		else
		{
			printf("Could not open game controller %i: %s\n",
					i, SDL_GetError());
		}
	}

	{
		/* 12 for "Peanut-SDL: " and a maximum of 16 for the title. */
		char title_str[28] = "Peanut-SDL: ";
		printf("ROM: %s\n", gb_get_rom_name(&gb, title_str + 12));
		printf("MBC: %d\n", gb.mbc);

		#ifdef NXDK
		window = SDL_CreateWindow(title_str,
					  SDL_WINDOWPOS_UNDEFINED,
					  SDL_WINDOWPOS_UNDEFINED,
					  XBOX_SCREEN_WIDTH, XBOX_SCREEN_HEIGHT,
					  SDL_WINDOW_SHOWN);
		#else
		window = SDL_CreateWindow(title_str,
					  SDL_WINDOWPOS_UNDEFINED,
					  SDL_WINDOWPOS_UNDEFINED,
					  LCD_WIDTH * 2, LCD_HEIGHT * 2,
					  SDL_WINDOW_RESIZABLE | SDL_WINDOW_INPUT_FOCUS);
		#endif

		if(window == NULL)
		{
			printf("Could not create window: %s\n", SDL_GetError());
			ret = EXIT_FAILURE;
			goto out;
		}
	}


	SDL_SetWindowMinimumSize(window, LCD_WIDTH, LCD_HEIGHT);

	#ifdef NXDK
	renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
	#else
	renderer = SDL_CreateRenderer(window, -1,
				      SDL_RENDERER_PRESENTVSYNC | SDL_RENDERER_ACCELERATED);
	#endif

	if(renderer == NULL)
	{
		printf("Could not create renderer: %s\n", SDL_GetError());
		ret = EXIT_FAILURE;
		goto out;
	}

	if(SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255) < 0)
	{
		printf("Renderer could not draw color: %s\n", SDL_GetError());
		ret = EXIT_FAILURE;
		goto out;
	}

	if(SDL_RenderClear(renderer) < 0)
	{
		printf("Renderer could not clear: %s\n", SDL_GetError());
		ret = EXIT_FAILURE;
		goto out;
	}

	SDL_RenderPresent(renderer);

	/* Use integer scale. */
	SDL_RenderSetLogicalSize(renderer, LCD_WIDTH, LCD_HEIGHT);
	SDL_RenderSetIntegerScale(renderer, 1);

	texture = SDL_CreateTexture(renderer,
				    SDL_PIXELFORMAT_RGB555,
				    SDL_TEXTUREACCESS_STREAMING,
				    LCD_WIDTH, LCD_HEIGHT);

	if(texture == NULL)
	{
		printf("Texture could not be created: %s\n", SDL_GetError());
		ret = EXIT_FAILURE;
		goto out;
	}

	auto_assign_palette(&priv, gb_colour_hash(&gb));

	#ifdef NXDK
	//Force frame skip to target 30fps instead of 60 for og xbox
	gb.direct.frame_skip = 1;
	#endif

	TTF_Init();
	TTF_Font* font = TTF_OpenFont(MENU_FONT_PATH, 20);
	if (font == NULL) {
		debugPrint("Couldn't load font: %s", TTF_GetError());
		while(1);
	}

	SDL_Color White = {255, 255, 255};
	SDL_Color Black = {0, 0, 0};
	SDL_Surface* message_surface;
	SDL_Texture* message_texture;
	SDL_Rect message_rect; //create a rect
	message_rect.x = 0;
	message_rect.y = 0;
	char message_text[256] = {0};
	unsigned int message_timer = 0;

	snprintf(message_text, sizeof(message_text), "STARTING %s", rom_name);
	message_timer = SDL_GetTicks();

	while(running)
	{
		int delay;
		static unsigned int rtc_timer = 0;
		static int selected_palette = 3;
		static unsigned int dump_bmp = 0;

		/* Calculate the time taken to draw frame, then later add a
		 * delay to cap at 60 fps. */
		old_ticks = SDL_GetTicks();

		/* Get joypad input. */
		while(SDL_PollEvent(&event))
		{
			static int fullscreen = 0;

			switch(event.type)
			{
			case SDL_QUIT:
				running = 0;
				break;

			case SDL_CONTROLLERBUTTONDOWN:
			case SDL_CONTROLLERBUTTONUP:
				switch(event.cbutton.button)
				{
				case SDL_CONTROLLER_BUTTON_A:
					gb.direct.joypad_bits.a = !event.cbutton.state;
					break;

				case SDL_CONTROLLER_BUTTON_B:
					gb.direct.joypad_bits.b = !event.cbutton.state;
					break;

				case SDL_CONTROLLER_BUTTON_BACK:
					gb.direct.joypad_bits.select = !event.cbutton.state;
					break;

				case SDL_CONTROLLER_BUTTON_START:
					gb.direct.joypad_bits.start = !event.cbutton.state;
					if (!gb.direct.joypad_bits.select && event.type == SDL_CONTROLLERBUTTONDOWN){
						running = 0;
					}
					break;

				case SDL_CONTROLLER_BUTTON_DPAD_UP:
					gb.direct.joypad_bits.up = !event.cbutton.state;
					if (!gb.direct.joypad_bits.select && event.type == SDL_CONTROLLERBUTTONDOWN){
						if(++selected_palette == NUMBER_OF_PALETTES)
							selected_palette = 0;
						manual_assign_palette(&priv, selected_palette);
						snprintf(message_text, sizeof(message_text), "PALETTE %u", selected_palette);
						message_timer = SDL_GetTicks();
					}
					break;

				case SDL_CONTROLLER_BUTTON_DPAD_RIGHT:
					gb.direct.joypad_bits.right = !event.cbutton.state;
					if (!gb.direct.joypad_bits.select && event.type == SDL_CONTROLLERBUTTONDOWN){
						auto_assign_palette(&priv, gb_colour_hash(&gb));
						snprintf(message_text, sizeof(message_text), "ASSIGN AUTO PALETTE");
						message_timer = SDL_GetTicks();
					}
					break;

				case SDL_CONTROLLER_BUTTON_DPAD_DOWN:
					gb.direct.joypad_bits.down = !event.cbutton.state;
					if (!gb.direct.joypad_bits.select && event.type == SDL_CONTROLLERBUTTONDOWN){
						if(--selected_palette < 0)
							selected_palette = NUMBER_OF_PALETTES - 1;
						manual_assign_palette(&priv, selected_palette);
						snprintf(message_text, sizeof(message_text), "PALETTE %u", selected_palette);
						message_timer = SDL_GetTicks();
					}
					break;

				case SDL_CONTROLLER_BUTTON_DPAD_LEFT:
					gb.direct.joypad_bits.left = !event.cbutton.state;
					if (!gb.direct.joypad_bits.select && event.type == SDL_CONTROLLERBUTTONDOWN){
						auto_assign_palette(&priv, gb_colour_hash(&gb));
						snprintf(message_text, sizeof(message_text), "ASSIGN AUTO PALETTE");
						message_timer = SDL_GetTicks();
					}
					break;

				case SDL_CONTROLLER_BUTTON_LEFTSHOULDER:
					if (fast_mode > 1 && event.type == SDL_CONTROLLERBUTTONDOWN) fast_mode--;
					snprintf(message_text, sizeof(message_text), "SPEED %u", fast_mode);
					message_timer = SDL_GetTicks();
					break;

				case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER:
					if (fast_mode < 8 && event.type == SDL_CONTROLLERBUTTONDOWN) fast_mode++;
					snprintf(message_text, sizeof(message_text), "SPEED %u", fast_mode);
					message_timer = SDL_GetTicks();
					break;

				case SDL_CONTROLLER_BUTTON_Y:
					if (gb.direct.joypad_bits.select && event.type == SDL_CONTROLLERBUTTONDOWN){
						gb.direct.frame_skip = ~gb.direct.frame_skip;

						if(gb.direct.frame_skip)  strncpy(message_text, "FRAMESKIP ON", sizeof(message_text));
						if(!gb.direct.frame_skip) strncpy(message_text, "FRAMESKIP OFF", sizeof(message_text));
						message_timer = SDL_GetTicks();

					} else if (!gb.direct.joypad_bits.select && event.type == SDL_CONTROLLERBUTTONDOWN){
						dump_bmp = ~dump_bmp;
					}
					break;
				}
				break;
			}
		}

		/* Execute CPU cycles until the screen has to be redrawn. */
		gb_run_frame(&gb);

		/* Tick the internal RTC when 1 second has passed. */
		rtc_timer += target_speed_ms / fast_mode;

		if(rtc_timer >= 1000)
		{
			rtc_timer -= 1000;
			gb_tick_rtc(&gb);
		}

		/* Skip frames during fast mode. */
		if(fast_mode_timer > 1)
		{
			fast_mode_timer--;
			/* We continue here since the rest of the logic in the
			 * loop is for drawing the screen and delaying. */
			continue;
		}

		fast_mode_timer = fast_mode;

#if ENABLE_LCD
		/* Copy frame buffer to SDL screen. */
		SDL_UpdateTexture(texture, NULL, &priv.fb, LCD_WIDTH * sizeof(uint16_t));
		SDL_RenderClear(renderer);

		/* Create message box to display text */
		if(SDL_GetTicks() - message_timer > 1000)
			message_text[0]='\0';
		message_surface = TTF_RenderText_Shaded(font, message_text, White, Black);
		message_texture = SDL_CreateTextureFromSurface(renderer, message_surface);
		SDL_QueryTexture(message_texture, NULL, NULL, &message_rect.w, &message_rect.h);
		message_rect.w /= 3;
		message_rect.h /= 3;

		/* Copy the GB frame buffer to the renderer */
		SDL_RenderCopy(renderer, texture, NULL, NULL);

		/* Copy the message box to the renderer */
		SDL_RenderCopy(renderer, message_texture, NULL, &message_rect);

		#ifdef NXDK
		XVideoWaitForVBlank();
		#endif
		SDL_RenderPresent(renderer);

		SDL_FreeSurface(message_surface);
		SDL_DestroyTexture(message_texture);

		if(dump_bmp){
			strncpy(message_text, "SAVING SCREENSHOT...", sizeof(message_text));
			message_timer = SDL_GetTicks();
			save_lcd_bmp(&gb, priv.fb);
			dump_bmp = 0;
		}

#endif

		/* Use a delay that will draw the screen at a rate of 59.7275 Hz. */
		new_ticks = SDL_GetTicks();

		/* Since we can only delay for a maximum resolution of 1ms, we
		 * can accumulate the error and compensate for the delay
		 * accuracy when the delay compensation surpasses 1ms. */
		speed_compensation += target_speed_ms - (new_ticks - old_ticks);

		/* We cast the delay compensation value to an integer, since it
		 * is the type used by SDL_Delay. This is where delay accuracy
		 * is lost. */
		delay = (int)(speed_compensation);

		/* We then subtract the actual delay value by the requested
		 * delay value. */
		speed_compensation -= delay;

		/* Only run delay logic if required. */
		if(delay > 0)
		{
			uint_fast32_t delay_ticks = SDL_GetTicks();
			uint_fast32_t after_delay_ticks;

			/* Tick the internal RTC when 1 second has passed. */
			rtc_timer += delay;

			if(rtc_timer >= 1000)
			{
				rtc_timer -= 1000;
				gb_tick_rtc(&gb);
			}

			/* This will delay for at least the number of
			 * milliseconds requested, so we have to compensate for
			 * error here too. */
			SDL_Delay(delay);

			after_delay_ticks = SDL_GetTicks();
			speed_compensation += (double)delay -
					      (int)(after_delay_ticks - delay_ticks);
		}

		/* If 10 seconds has passed, record save file.
		 * We do this because the external audio library
		 * used contains asserts that will abort the
		 * program without save.
		 * TODO: Remove use of assert in audio library
		 * in release build. */
		/* TODO: Remove all workarounds due to faulty
		 * external libraries. */
		static int save_timer = 0;
		if(SDL_GetTicks() - save_timer > 10000)
		{
#if ENABLE_SOUND_BLARGG
			/* Locking the audio thread to reduce
			 * possibility of abort during save. */
			SDL_LockAudioDevice(dev);
#endif
			write_cart_ram_file(save_file_name,
					    &priv.cart_ram,
					    gb_get_save_size(&gb));
#if ENABLE_SOUND_BLARGG
			SDL_UnlockAudioDevice(dev);
#endif
			save_timer = SDL_GetTicks();
		}
	}

	SDL_PauseAudioDevice(dev, 1);
	SDL_Delay(100);
	SDL_CloseAudioDevice(dev);
	SDL_DestroyRenderer(renderer);
	SDL_DestroyWindow(window);
	SDL_DestroyTexture(texture);
	SDL_GameControllerClose(controller);
	SDL_Quit();
#ifdef ENABLE_SOUND_BLARGG
	audio_cleanup();
#endif

	/* Record save file. */
	write_cart_ram_file(save_file_name, &priv.cart_ram, gb_get_save_size(&gb));

out:
	free(priv.rom);
	free(priv.cart_ram);
	/* If the save file name was automatically generated (which required memory
	 * allocated on the help), then free it here. */
	if(save_file_name)
		free(save_file_name);

	if(rom_file_name)
		free(rom_file_name);


	goto start;

	return ret;
}

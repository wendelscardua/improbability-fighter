/* Based on ...
 *  example of MMC3 for cc65
 *	Doug Fraker 2019
 */

#include "lib/neslib.h"
#include "lib/nesdoug.h"
#include "lib/unrle.h"
#include "mmc3/mmc3_code.h"
#include "mmc3/mmc3_code.c"
#include "sprites.h"
#include "../assets/nametables.h"

#define BG_0 0
#define BG_1 1
#define BG_2 2
#define BG_3 3
#define SPRITE_0 4
#define SPRITE_1 6

#pragma bss-name(push, "ZEROPAGE")

// GLOBAL VARIABLES
unsigned char arg1;
unsigned char arg2;
unsigned char pad1;
unsigned char pad1_new;
unsigned char double_buffer_index;

unsigned char temp, i, unseeded, temp_x, temp_y;
unsigned int temp_int;

enum game_state {
                 Title,
                 GamePlay,
                 GameEnd
} current_game_state;

enum ship_mode {
                Default
} current_ship_mode;

unsigned char enemy_area_x, enemy_area_y;
unsigned char player_x, player_y;

#pragma bss-name(pop)
// should be in the regular 0x300 ram now

unsigned char irq_array[32];
unsigned char double_buffer[32];

#pragma bss-name(push, "XRAM")
// extra RAM at $6000-$7fff
unsigned int wram_start;

#pragma bss-name(pop)

// the fixed bank

#pragma rodata-name ("RODATA")
#pragma code-name ("CODE")

const unsigned char palette[16]={ 0x0f,0x00,0x10,0x30,0x0f,0x01,0x21,0x31,0x0f,0x06,0x16,0x26,0x0f,0x09,0x19,0x29 };

void draw_sprites (void);

void init_wram (void) {
  if (wram_start != 0xcafe)
    memfill(&wram_start,0,0x2000);
  wram_start = 0xcafe;
}

void load_enemy_formation (unsigned char index) {
  vram_adr(NTADR_A(0,0));
  unrle(enemy_formations[index]);
  enemy_area_x = 0;
  enemy_area_y = 0xa0;
  set_scroll_x(enemy_area_x);
  set_scroll_y(enemy_area_y);
}

void init_ship (void) {
  current_ship_mode = Default;
  player_x = 0x80;
  player_y = 0xa0;
}

void start_game (void) {
  if (unseeded) {
    seed_rng();
    unseeded = 0;
  }

  pal_fade_to(4, 0);
  ppu_off(); // screen off
  pal_bg(palette); // load the BG palette
  pal_spr(palette); // load the sprite palette

  // draw some things
  load_enemy_formation(0);
  vram_adr(NTADR_C(0,0));
  unrle(empty_nametable);
  ppu_on_all();

  pal_fade_to(0, 4);
  current_game_state = GamePlay;

  init_ship();
}

void go_to_title (void) {
  pal_fade_to(4, 0);
  ppu_off(); // screen off
  // draw some things
  vram_adr(NTADR_A(0,0));
  unrle(title_nametable);
  music_play(0);

  draw_sprites();
  ppu_on_all(); //	turn on screen
  pal_fade_to(0, 4);
  current_game_state = Title;
}

void main (void) {
  set_mirroring(MIRROR_HORIZONTAL);
  bank_spr(1);
  irq_array[0] = 0xff; // end of data
  set_irq_ptr(irq_array); // point to this array

  init_wram();

  ppu_off(); // screen off
  pal_bg(palette); //	load the BG palette
  pal_spr(palette); // load the sprite palette
  // load red alpha and drawing as bg chars
  // and unused as sprites
  set_chr_mode_2(BG_0);
  set_chr_mode_3(BG_1);
  set_chr_mode_4(BG_2);
  set_chr_mode_5(BG_3);
  set_chr_mode_0(SPRITE_0);
  set_chr_mode_1(SPRITE_1);

  go_to_title();

  unseeded = 1;

  set_vram_buffer();
  clear_vram_buffer();

  while (1){ // infinite loop
    ppu_wait_nmi();
    clear_vram_buffer();
    pad_poll(0);
    rand16();


    double_buffer_index = 0;

    switch (current_game_state) {
    case Title:
      ++temp;
      if (get_pad_new(0) & (PAD_START | PAD_A)) {
        start_game();
      }
      break;
    case GamePlay:
      set_scroll_x(enemy_area_x);
      set_scroll_y(enemy_area_y);
      break;
    case GameEnd:
      if (get_pad_new(0) & PAD_START) {
        go_to_title();
      }
      break;
    }


    // load the irq array with values it parse
    // ! CHANGED it, double buffered so we aren't editing the same
    // array that the irq system is reading from


    double_buffer[double_buffer_index++] = 0xff; // end of data

    draw_sprites();

    // wait till the irq system is done before changing it
    // this could waste a lot of CPU time, so we do it last
    while(!is_irq_done() ){ // have we reached the 0xff, end of data
      // is_irq_done() returns zero if not done
      // do nothing while we wait
    }

    // copy from double_buffer to the irq_array
    // memcpy(void *dst,void *src,unsigned int len);
    memcpy(irq_array, double_buffer, sizeof(irq_array));
  }
}

void draw_sprites (void) {
  oam_clear();
  if (current_game_state != GamePlay) return;

  switch(current_ship_mode) {
  case Default:
    oam_meta_spr(player_x, player_y, default_ship_sprite);
    break;
  }
}

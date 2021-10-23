/* Based on ...
 *  example of MMC3 for cc65
 *	Doug Fraker 2019
 */

#include "lib/neslib.h"
#include "lib/nesdoug.h"
#include "lib/unrle.h"
#include "mmc3/mmc3_code.h"
#include "mmc3/mmc3_code.c"
#include "bullets.h"
#include "sprites.h"
#include "../assets/nametables.h"

#define SFX_TOGGLE 0
#define SFX_SELECT 1
#define SFX_START 2
#define SFX_THE_END 3
#define SFX_HIT 4
#define SFX_PEW 5
#define SFX_CHAOS 6

#define BG_0 0
#define BG_1 1
#define BG_2 2
#define BG_3 3
#define SPRITE_0 4
#define SPRITE_1 6

#define FP(integer,fraction) (((integer)<<8)|((fraction)>>0))
#define INT(unsigned_fixed_point) ((unsigned_fixed_point>>8)&0xff)
#define FRAC(unsigned_fixed_point) ((unsigned_fixed_point)&0xff)

#define HIT_CHAOS(amount) if (chaos_counter >= (amount)) { chaos_counter -= (amount); } else { chaos_counter = 0; }

#define HUD_HEIGHT 0x28

#define PLAYER_SPEED FP(1, 128)
#define TETRO_SPEED FP(8, 128)

#define MAX_ENEMIES 4
#define MAX_FORMATIONS 2

#define IS_PLAYER_BULLET(index) (bullets_type[index] != EnemyBullet)

#pragma bss-name(push, "ZEROPAGE")

typedef enum  {
               Trio,
               Targeter
} pattern_type;

typedef struct {
  unsigned char x;
  unsigned char y;
  unsigned char width;
  unsigned char height;
} collidable;

typedef struct {
  unsigned char column, row, width, height;
  unsigned char hp;
  pattern_type pattern;
} enemy;

// GLOBAL VARIABLES
unsigned char double_buffer_index;
unsigned char arg1;
unsigned char arg2;
unsigned char pad1;
unsigned char pad1_new;

unsigned char temp, i, unseeded, temp_x, temp_y;
unsigned int temp_int, temp_int_x, temp_int_y;
unsigned char hud_scanline, hud_skip_scanline;

enum game_state {
                 Title,
                 GamePlay
} current_game_state;

enum ship_mode {
                Default,
                Tree,
                Tetro,
                None
} current_ship_mode, old_ship_mode;

unsigned char enemy_area_x;
unsigned int enemy_area_y;
unsigned int enemy_rel_y;
unsigned int player_x, player_y;
signed char player_speed;
unsigned char player_shoot_cd, player_bullets_cd, player_bullet_count;
unsigned char player_blink, tetro_buffer;
unsigned char health, chaos;
unsigned char chaos_counter;

collidable temp_collidable_a, temp_collidable_b, player_collidable;

unsigned char num_enemies;

unsigned char current_enemy_formation;
unsigned char enemy_formation_index;
unsigned char enemy_row_movement;

#pragma bss-name(pop)
// should be in the regular 0x300 ram now

char irq_array[32];
unsigned char double_buffer[32];

unsigned char enemy_index[MAX_ENEMIES];
unsigned char enemy_hp[MAX_ENEMIES];
unsigned char enemy_shoot_cd[MAX_ENEMIES];
unsigned char enemy_bullets_cd[MAX_ENEMIES];
unsigned char enemy_bullet_count[MAX_ENEMIES];
unsigned char enemy_x[MAX_ENEMIES];
unsigned char enemy_y[MAX_ENEMIES];
unsigned char enemy_width[MAX_ENEMIES];
unsigned char enemy_height[MAX_ENEMIES];
unsigned char enemy_pattern[MAX_ENEMIES];

#pragma bss-name(push, "XRAM")
// extra RAM at $6000-$7fff

unsigned int wram_start;

#pragma bss-name(pop)

// the fixed bank

#pragma rodata-name ("RODATA")
#pragma code-name ("CODE")

const char palette_bg[16]={ 0x0f,0x00,0x10,0x30,0x0f,0x01,0x21,0x31,0x0f,0x06,0x16,0x26,0x0f,0x11,0x2c,0x30 };

const char palette_spr[16]={ 0x0f,0x00,0x10,0x30,0x0f,0x01,0x21,0x31,0x0f,0x06,0x16,0x26,0x0f,0x17,0x19,0x29 };

const char emptiness[] = { 0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00 };

const enemy enemies[] = { //c,  r, w, h, hp, pattern
                         { 14, 24, 4, 2, 10, Trio }, //1-1

                         {  6, 16, 4, 2, 10, Trio }, //1-2
                         { 22, 16, 4, 2, 10, Trio },

                         {  4,  6, 4, 2, 12, Trio }, //1-3
                         { 10,  8, 4, 2, 12, Trio },
                         { 18,  8, 4, 2, 12, Trio },
                         { 24,  6, 4, 2, 12, Trio },

                         { 12, 23, 8, 3, 12, Targeter }, //2-1

                         {  4, 15, 8, 3, 12, Targeter }, //2-2
                         { 20, 15, 8, 3, 12, Targeter },

                         {  4,  6, 8, 3, 12, Targeter }, //2-3
                         { 12,  7, 8, 3, 12, Targeter },
                         { 20,  6, 8, 3, 12, Targeter }

};

const unsigned char formations[2][] = {
                                       {1, 0, 2, 1, 2, 4, 3, 4, 5, 6, 0},
                                       {1, 7, 2, 8, 9, 3, 10, 11, 12, 0}
};

void draw_sprites (void);
void update_health (void);
void update_chaos (void);

void init_wram (void) {
  //if (wram_start != 0xcafe)
  memfill(&wram_start,0,0x2000);
  //wram_start = 0xcafe;
}

void load_enemy_formation (unsigned char index);

unsigned char load_enemy_row (void) {
  temp = formations[current_enemy_formation][enemy_formation_index++];
  if (temp == 0) {
    ++current_enemy_formation;
    if (current_enemy_formation >= MAX_FORMATIONS) {
      sfx_play(SFX_THE_END, 0);
      return 0;
    } else {
      while(!is_irq_done() ){}
      irq_array[0] = 0xff;
      double_buffer[0] = 0xff;

      enemy_area_x = 0;
      enemy_area_y = 0xa0;
      enemy_rel_y = enemy_area_y;
      set_scroll_x(enemy_area_x);
      set_scroll_y(enemy_area_y);

      clear_vram_buffer();
      pal_fade_to(4, 0);
      ppu_off(); // screen off
      load_enemy_formation(current_enemy_formation);
      reset_bullets();
      oam_clear();
      update_health();
      update_chaos();
      flush_vram_update_nmi();
      ppu_on_all();
      pal_fade_to(0, 4);
      enemy_row_movement = 0;
      return 1;
    }
  }
  num_enemies = 0;
  while(num_enemies < temp) {
    i = formations[current_enemy_formation][enemy_formation_index++];
    enemy_index[num_enemies] = i;
    enemy_hp[num_enemies] = enemies[i].hp;
    enemy_shoot_cd[num_enemies] = 0;
    enemy_bullets_cd[num_enemies] = 0;
    enemy_bullet_count[num_enemies] = 0;
    enemy_x[num_enemies] = enemies[i].column * 8 + 2;
    enemy_y[num_enemies] = enemies[i].row * 8 + 2;
    enemy_width[num_enemies] = enemies[i].width * 8 - 4;
    enemy_height[num_enemies] = enemies[i].height * 8 - 4;
    enemy_pattern[num_enemies] = enemies[i].pattern;

    ++num_enemies;
  }
  enemy_row_movement = 64;
  return 1;
}

void load_enemy_formation (unsigned char index) {
  current_enemy_formation = index;
  enemy_formation_index = 0;
  vram_adr(NTADR_A(0,0));
  unrle(enemy_formation_nametables[index]);
  enemy_area_x = 0;
  enemy_area_y = 0xa0;
  enemy_rel_y = enemy_area_y;
  set_scroll_x(enemy_area_x);
  set_scroll_y(enemy_area_y);
  reset_bullets();
  load_enemy_row();
}

#define HITBOX_WIDTH 8
#define HITBOX_HEIGHT 8

void init_ship (void) {
  current_ship_mode = Default;
  player_x = FP(0x80, 0);
  player_y = FP(0xa0, 0);
  player_shoot_cd = 0;
  player_bullet_count = 0;
  player_bullets_cd = 0;
  player_speed = 0;
  player_blink = 0;
  tetro_buffer = 0;
  health = 16;
  chaos = 0;
  chaos_counter = 240;
  player_collidable.width = HITBOX_WIDTH;
  player_collidable.height = HITBOX_HEIGHT;
  player_collidable.x = INT(player_x) - HITBOX_WIDTH/2;
  player_collidable.y = INT(player_y) - HITBOX_HEIGHT/2;
}

void update_health (void) {
  temp_x = 11;
  for(i = 0; i < 16; i += 4) {
    if (i > health) {
      temp = 0;
    } else {
      temp = health - i;
      if (temp > 4) temp = 4;
    }
    one_vram_buffer(0x62 + temp, NTADR_A(temp_x, 2));
    ++temp_x;
  }
}

void update_chaos (void) {
  temp_x = 24;
  for(i = 0; i < 16; i += 4) {
    if (i > chaos) {
      temp = 0;
    } else {
      temp = chaos - i;
      if (temp > 4) temp = 4;
    }
    one_vram_buffer(0x62 + temp, NTADR_A(temp_x, 2));
    ++temp_x;
  }
}

void delete_enemy (void) {
  --num_enemies;
  i = enemy_index[temp];
  enemy_index[temp] = enemy_index[num_enemies];
  enemy_hp[temp] = enemy_hp[num_enemies];
  enemy_shoot_cd[temp] = enemy_shoot_cd[num_enemies];
  enemy_bullets_cd[temp] = enemy_bullets_cd[num_enemies];
  enemy_bullet_count[temp] = enemy_bullet_count[num_enemies];
  enemy_x[temp] = enemy_x[num_enemies];
  enemy_y[temp] = enemy_y[num_enemies];
  enemy_width[temp] = enemy_width[num_enemies];
  enemy_height[temp] = enemy_height[num_enemies];
  enemy_pattern[temp] = enemy_pattern[num_enemies];

  --temp;

  temp_x = enemies[i].width;
  for(temp_y = enemies[i].height; temp_y > 0; --temp_y) {
    multi_vram_buffer_horz(emptiness, temp_x, NTADR_A(enemies[i].column, enemies[i].row + temp_y - 1));
  }
  // TODO kaboom

  if (num_enemies == 0) {
    load_enemy_row();
  }
}

void compute_collisions (void) {
  // XXX: hardcoded bullet size
  temp_collidable_b.width = 4;
  temp_collidable_b.height = 8;

  for(i = get_frame_count() % 4; i < get_num_bullets(); i+=4) {
    if (health == 0 || current_enemy_formation == MAX_FORMATIONS) continue;

    temp_collidable_b.x = bullets_x[i] + 2;
    temp_collidable_b.y = bullets_y[i];
    if (IS_PLAYER_BULLET(i)) {
      if (temp_collidable_b.y > 0x64) continue;
      for(temp = 0; temp < num_enemies; ++temp) {
        temp_collidable_a.x = enemy_x[temp];
        temp_collidable_a.y = enemy_y[temp] - enemy_rel_y;
        temp_collidable_a.width = enemy_width[temp];
        temp_collidable_a.height = enemy_height[temp];

        if (check_collision(&temp_collidable_a, &temp_collidable_b)) {
          sfx_play(SFX_HIT, 0);
          delete_bullet(i);
          --i;
          --enemy_hp[temp];
          if (enemy_hp[temp] == 0) {
            delete_enemy();
            break;
          }
        }
      }
    } else {
      if (player_blink) continue;

      if (check_collision(&player_collidable, &temp_collidable_b)) {
        sfx_play(SFX_HIT, 0);
        delete_bullet(i);
        --i;
        if (health > 0) {
          --health;
          player_blink = 45;
          update_health();
          if (health == 0) {
            // TODO game over
            break;
          }
        }
      }
    }
  }
}

void start_game (void) {
  if (unseeded) {
    seed_rng();
    unseeded = 0;
  }

  pal_fade_to(4, 0);
  ppu_off(); // screen off
  pal_bg(palette_bg); // load the BG palette
  pal_spr(palette_spr); // load the sprite palette

  // draw some things
  load_enemy_formation(0);
  vram_adr(NTADR_C(0,0));
  unrle(empty_nametable);
  ppu_on_all();

  pal_fade_to(0, 4);
  current_game_state = GamePlay;

  init_ship();

  enemy_row_movement = 0;
}

void go_to_title (void) {
  current_game_state = Title;

  if (irq_array[0] != 0xff) {
    while(!is_irq_done() ){}
    irq_array[0] = 0xff;
    double_buffer[0] = 0xff;
  }

  enemy_area_x = 0;
  enemy_area_y = 0xa0;
  enemy_rel_y = enemy_area_y;
  set_scroll_x(enemy_area_x);
  set_scroll_y(enemy_area_y);

  clear_vram_buffer();

  pal_fade_to(4, 0);
  ppu_off(); // screen off
  // draw some things
  vram_adr(NTADR_A(0,0));
  unrle(title_nametable);
  music_play(0);

  set_scroll_x(0);
  set_scroll_y(0);

  draw_sprites();
  ppu_on_all(); //	turn on screen
  pal_fade_to(0, 4);
}

void player_shoot (void) {
  if (player_shoot_cd > 0 ||
      get_num_bullets() >= MAX_BULLETS ||
      enemy_row_movement > 0 ||
      player_blink) return;

  switch(current_ship_mode) {
  case Default:
    if (player_bullet_count >= 3) return;
    ++player_bullet_count;
    player_shoot_cd = 8;
    player_bullets_cd = 60;
    set_bullet_x(player_x - FP(4, 0));
    set_bullet_y(player_y - FP(8, 0));
    set_bullet_type(PlayerBullet);
    set_bullet_delta_x(FP(0, 0));
    set_bullet_delta_y(-FP(2, 128));
    inc_bullets();
    HIT_CHAOS(4);
    break;
  case Tree:
    if (player_bullet_count >= 1) return;
    ++player_bullet_count;
    player_shoot_cd = 12;
    player_bullets_cd = 45;
    set_bullet_x(player_x - FP(4, 0));
    set_bullet_y(player_y - FP(8, 0));
    set_bullet_type(PlayerApple);
    if (player_speed >= 0) {
      set_bullet_delta_x(FP(0, 32));
    } else {
      set_bullet_delta_x(-FP(0, 32));
    }
    set_bullet_delta_y(FP(2, 0));
    inc_bullets();
    HIT_CHAOS(16);
    break;
  case Tetro:
    if (player_bullet_count >= 4) return;
    ++player_bullet_count;
    player_shoot_cd = 8;
    player_bullets_cd = 75;
    set_bullet_x(player_x - FP(8, 0));
    set_bullet_y(player_y - FP(12, 0));
    set_bullet_type(PlayerBlock);
    set_bullet_delta_x(FP(0, 0));
    set_bullet_delta_y(-FP(1, 0));
    inc_bullets();
    HIT_CHAOS(8);
    break;
  }
  sfx_play(SFX_PEW, 0);
  return;
}

void enemy_shoot (void) {
  if (enemy_shoot_cd[temp] > 0 || get_num_bullets() >= MAX_BULLETS || enemy_row_movement > 0) return;
  if (enemy_bullet_count[temp] == 0 && rand8() > 32) return;

  temp_int_x = FP(enemy_x[temp] + enemy_width[temp] / 2 - 4, 0);
  temp_int_y = FP(enemy_y[temp] + enemy_height[temp] / 2 - enemy_rel_y, 0);

  switch(enemy_pattern[temp]) {
  case Trio:
    if (enemy_bullet_count[temp] >= 2) return;
    ++enemy_bullet_count[temp];
    enemy_shoot_cd[temp] = 12;
    enemy_bullets_cd[temp] = 90;
    set_bullet_x(temp_int_x);
    set_bullet_y(temp_int_y + FP(8, 0));
    set_bullet_type(EnemyBullet);
    set_bullet_delta_x(FP(0, 0));
    set_bullet_delta_y(FP(2, 0));
    inc_bullets();
    if (get_num_bullets() >= MAX_BULLETS) break;

    set_bullet_x(temp_int_x + FP(8, 0));
    set_bullet_y(temp_int_y + FP(8, 0));
    set_bullet_type(EnemyBullet);
    set_bullet_delta_x(FP(0, 96));
    set_bullet_delta_y(FP(2, 0));
    inc_bullets();
    if (get_num_bullets() >= MAX_BULLETS) break;

    set_bullet_x(temp_int_x - FP(8, 0));
    set_bullet_y(temp_int_y + FP(8, 0));
    set_bullet_type(EnemyBullet);
    set_bullet_delta_x(-FP(0, 96));
    set_bullet_delta_y(FP(2, 0));
    inc_bullets();
    break;
  case Targeter:
    if (enemy_bullet_count[temp] >= 3) return;
    ++enemy_bullet_count[temp];
    enemy_shoot_cd[temp] = 15;
    enemy_bullets_cd[temp] = 90;
    temp_int = temp_int_x - FP(0x10, 0);
    set_bullet_x(temp_int);
    set_bullet_y(temp_int_y + FP(8, 0));
    set_bullet_type(EnemyBullet);
    if (player_x >= temp_int) {
      set_bullet_delta_x(FP(0, 64));
    } else {
      set_bullet_delta_x(-FP(0, 128));
    }
    set_bullet_delta_y(FP(2, 0));
    inc_bullets();
    if (get_num_bullets() >= MAX_BULLETS) break;

    temp_int = temp_int_x + FP(0x10, 0);
    set_bullet_x(temp_int);
    set_bullet_y(temp_int_y + FP(8, 0));
    set_bullet_type(EnemyBullet);
    if (player_x >= temp_int) {
      set_bullet_delta_x(FP(0, 128));
    } else {
      set_bullet_delta_x(-FP(0, 64));
    }
    set_bullet_delta_y(FP(2, 0));
    inc_bullets();
    break;
  }
  return;
}

void hud_stuff (void) {
  hud_scanline = 0xf0 - HUD_HEIGHT;

  if (enemy_area_y == 0) {
    enemy_area_y = HUD_HEIGHT;
  } else if (enemy_area_y < HUD_HEIGHT) {
    enemy_area_y = sub_scroll_y(HUD_HEIGHT, enemy_area_y) & 0x1ff;
  }

  hud_skip_scanline = 0xff;

  if (enemy_area_y > 0x100 + HUD_HEIGHT) {
    hud_skip_scanline = 0xf0 - (enemy_area_y - 0x100);
  }
  // TODO compute skip scanline

  if (hud_skip_scanline != 0xff && hud_scanline > hud_skip_scanline + 1) {
    double_buffer[double_buffer_index++] = hud_skip_scanline - 1;
    double_buffer[double_buffer_index++] = 0xfd;
    double_buffer[double_buffer_index++] = 0xf6;
    temp_int = 0x2000 + 0x4 * HUD_HEIGHT;
    double_buffer[double_buffer_index++] = (temp_int>>8);
    double_buffer[double_buffer_index++] = temp_int;

    hud_scanline -= (hud_skip_scanline + 1);
  }

  set_scroll_x(enemy_area_x);
  set_scroll_y(enemy_area_y);

  // scroll to hud at the end
  double_buffer[double_buffer_index++] = hud_scanline - 1;
  double_buffer[double_buffer_index++] = 0xfd;
  double_buffer[double_buffer_index++] = 0xf6;
  double_buffer[double_buffer_index++] = 0x20;
  double_buffer[double_buffer_index++] = 0x00;
  double_buffer[double_buffer_index++] = 0xf1;
  double_buffer[double_buffer_index++] = 0x08;
  return;
}

void main (void) {
  set_mirroring(MIRROR_HORIZONTAL);
  bank_spr(1);
  irq_array[0] = 0xff; // end of data
  set_irq_ptr(irq_array); // point to this array

  init_wram();

  ppu_off(); // screen off
  pal_bg(palette_bg); //	load the BG palette
  pal_spr(palette_spr); // load the sprite palette
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

    double_buffer_index = 0;

    switch (current_game_state) {
    case Title:
      for(i = 0; i < 16; i++) {
        rand16();
        if (get_pad_new(0) & (PAD_START | PAD_A)) {
          sfx_play(SFX_START, 0);
          start_game();
          break;
        }
        pad_poll(0);
      }
      break;
    case GamePlay:
      if (enemy_row_movement > 0 && get_num_bullets() == 0) {
        --enemy_row_movement;
        enemy_area_y = sub_scroll_y(1, enemy_area_y);
        --enemy_rel_y;
      }

      if (player_blink > 0) --player_blink;

      HIT_CHAOS(1);

      if (health > 0 && current_enemy_formation < MAX_FORMATIONS && chaos_counter == 0) {
        if (chaos < 16) {
          ++chaos;
          update_chaos();
        } else {
          chaos = 0;
          update_chaos();
          // TODO: random if more than these 2

          old_ship_mode = current_ship_mode;
          do {
            current_ship_mode = rand8() % 4;
          } while (current_ship_mode == old_ship_mode || current_ship_mode >= None) ;
        }
        if (chaos < 16) {
          switch(rand8() % 4) {
          case 0:
            chaos_counter = 45;
            break;
          case 1:
          case 2:
            chaos_counter = 90;
            break;
          case 3:
            chaos_counter = 120;
            break;
          }
        } else {
          sfx_play(SFX_CHAOS, 0);
          chaos_counter = 45;
        }
      }
      update_bullets();
      compute_collisions();

      if (player_shoot_cd > 0) --player_shoot_cd;
      if (player_bullets_cd > 0) {
        --player_bullets_cd;
        if (player_bullets_cd == 0) {
          player_bullet_count = 0;
        }
      }

      for(temp = 0; temp < num_enemies; ++temp) {
        if (enemy_shoot_cd[temp] > 0) --enemy_shoot_cd[temp];
        if (enemy_bullets_cd[temp] > 0) {
          --enemy_bullets_cd[temp];
          if (enemy_bullets_cd[temp] == 0) {
            enemy_bullet_count[temp] = 0;
          }
        }
        enemy_shoot();
      }

#define TETRO_DELAY 24
#define TETRO_SUBDELAY 16
#define X_MARGIN 0x18
#define TOP_MARGIN 0x60
#define BOTTOM_MARGIN 0x38

      if (current_ship_mode == Tetro) {
        if (tetro_buffer < TETRO_DELAY && (pad_state(0) & (PAD_UP|PAD_DOWN|PAD_LEFT|PAD_RIGHT))) {
          tetro_buffer++;
        }
        if (get_pad_new(0) & (PAD_LEFT)) {
          tetro_buffer = 0;
          HIT_CHAOS(8);
          player_speed = -1;
          if (player_x > FP(X_MARGIN, 0)) {
            player_x -= TETRO_SPEED;
            player_collidable.x = INT(player_x) - HITBOX_WIDTH/2;
          }
        } else if (get_pad_new(0) & (PAD_RIGHT)) {
          tetro_buffer = 0;
          HIT_CHAOS(8);
          player_speed = 1;
          if (player_x < FP(0xff - X_MARGIN, 0)) {
            player_x += TETRO_SPEED;
            player_collidable.x = INT(player_x) - HITBOX_WIDTH/2;
          }
        } else if (get_pad_new(0) & (PAD_UP)) {
          tetro_buffer = 0;
          HIT_CHAOS(8);
          if (player_y > FP(TOP_MARGIN, 0)) {
            player_y -= TETRO_SPEED;
            player_collidable.y = INT(player_y) - HITBOX_HEIGHT/2;
          }
        } else if (get_pad_new(0) & (PAD_DOWN)) {
          tetro_buffer = 0;
          HIT_CHAOS(8);
          if (player_y < FP(0xef - BOTTOM_MARGIN, 0)) {
            player_y += TETRO_SPEED;
            player_collidable.y = INT(player_y) - HITBOX_HEIGHT/2;
          }
        } else if (tetro_buffer == TETRO_DELAY) {
          if (pad_state(0) & (PAD_LEFT)) {
            tetro_buffer -= TETRO_SUBDELAY;
            HIT_CHAOS(8);
            player_speed = -1;
            if (player_x > FP(X_MARGIN, 0)) {
              player_x -= TETRO_SPEED;
              player_collidable.x = INT(player_x) - HITBOX_WIDTH/2;
            }
          }
          if (pad_state(0) & (PAD_RIGHT)) {
            tetro_buffer -= TETRO_SUBDELAY;
            HIT_CHAOS(8);
            player_speed = 1;
            if (player_x < FP(0xff - X_MARGIN, 0)) {
              player_x += TETRO_SPEED;
              player_collidable.x = INT(player_x) - HITBOX_WIDTH/2;
            }
          }
          if (pad_state(0) & (PAD_UP)) {
            tetro_buffer -= TETRO_SUBDELAY;
            HIT_CHAOS(8);
            if (player_y > FP(TOP_MARGIN, 0)) {
              player_y -= TETRO_SPEED;
              player_collidable.y = INT(player_y) - HITBOX_HEIGHT/2;
            }
          }
          if (pad_state(0) & (PAD_DOWN)) {
            tetro_buffer -= TETRO_SUBDELAY;
            HIT_CHAOS(8);
            if (player_y < FP(0xef - BOTTOM_MARGIN, 0)) {
              player_y += TETRO_SPEED;
              player_collidable.y = INT(player_y) - HITBOX_HEIGHT/2;
            }
          }
        }
      } else {
        if (pad_state(0) & (PAD_LEFT)) {
          HIT_CHAOS(1);
          player_speed = -1;
          if (player_x > FP(X_MARGIN, 0)) {
            player_x -= PLAYER_SPEED;
            player_collidable.x = INT(player_x) - HITBOX_WIDTH/2;
          }
        }
        if (pad_state(0) & (PAD_RIGHT)) {
          HIT_CHAOS(1);
          player_speed = 1;
          if (player_x < FP(0xff - X_MARGIN, 0)) {
            player_x += PLAYER_SPEED;
            player_collidable.x = INT(player_x) - HITBOX_WIDTH/2;
          }
        }
        if (pad_state(0) & (PAD_UP)) {
          HIT_CHAOS(1);
          if (player_y > FP(TOP_MARGIN, 0)) {
            player_y -= PLAYER_SPEED;
            player_collidable.y = INT(player_y) - HITBOX_HEIGHT/2;
          }
        }
        if (pad_state(0) & (PAD_DOWN)) {
          HIT_CHAOS(1);
          if (player_y < FP(0xef - BOTTOM_MARGIN, 0)) {
            player_y += PLAYER_SPEED;
            player_collidable.y = INT(player_y) - HITBOX_HEIGHT/2;
          }
        }
      }

      hud_stuff();

      if (health == 0 || current_enemy_formation == MAX_FORMATIONS) {
        if (get_pad_new(0) & (PAD_START)) {
          go_to_title();
        }
        break;
      }
      if ((pad_state(0) & (PAD_A)) && chaos < 16) {
        player_shoot();
      }
#ifdef DEBUG
      if (pad_state(0) & PAD_B) {
        if (pad_state(0) & PAD_A) {
          for(i = 0; i < num_enemies; i++) {
            enemy_hp[i] = 1;
          }
        }
        if (pad_state(0) & PAD_UP) {
          enemy_area_y = sub_scroll_y(1, enemy_area_y);
          enemy_area_y &= 0x1ff;
          --enemy_rel_y;
        }
        if (pad_state(0) & PAD_DOWN) {
          enemy_area_y = add_scroll_y(1, enemy_area_y);
          enemy_area_y &= 0x1ff;
          ++enemy_rel_y;
        }
        if (pad_state(0) & PAD_SELECT) {
          chaos = 15;
        }
      }
#endif

      break;
    }

    // load the irq array with values it parse
    // ! CHANGED it, double buffered so we aren't editing the same
    // array that the irq system is reading from


    double_buffer[double_buffer_index++] = 0xff; // end of data

    draw_sprites();

    // wait till the irq system is done before changing it
    // this could waste a lot of CPU time, so we do it last
    while(!is_irq_done() ){}

    // copy from double_buffer to the irq_array
    // memcpy(void *dst,void *src,unsigned int len);
    memcpy(irq_array, double_buffer, sizeof(irq_array));
  }
}

void draw_ship (void) {
  if (player_blink && ((get_frame_count() & 0xa) != 0x0)) return;

  temp_x = INT(player_x);
  temp_y = INT(player_y);
  if (current_enemy_formation == MAX_FORMATIONS) {
    oam_meta_spr(temp_x, temp_y, victory_sprite);
  } else if (health > 0) {
    if (chaos == 16) {
      oam_meta_spr(temp_x, temp_y, glitch_sprite);
    } else {
      switch(current_ship_mode) {
      case Default:
        oam_meta_spr(temp_x, temp_y, default_ship_sprite);
        break;
      case Tree:
        oam_meta_spr(temp_x, temp_y, tree_ship_sprite);
        break;
      case Tetro:
        oam_meta_spr(temp_x, temp_y, tetro_ship_sprite);
        break;
      }
    }
  } else {
    oam_meta_spr(temp_x, temp_y, game_over_sprite);
  }
}

void draw_sprites (void) {
  oam_clear();
  if (current_game_state != GamePlay) return;

  draw_bullets();

  draw_ship();
}

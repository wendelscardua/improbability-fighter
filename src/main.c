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
unsigned char arg1;
unsigned char arg2;
unsigned char pad1;
unsigned char pad1_new;
unsigned char double_buffer_index;

unsigned char temp, i, unseeded, temp_x, temp_y;
unsigned int temp_int, temp_int_x, temp_int_y;

enum game_state {
                 Title,
                 GamePlay,
                 GameEnd
} current_game_state;

enum ship_mode {
                Default,
                Tree,
                Tetro,
                None
} current_ship_mode, old_ship_mode;

unsigned char enemy_area_x, enemy_area_y;
unsigned int player_x, player_y;
signed char player_speed;
unsigned char player_shoot_cd, player_bullets_cd, player_bullet_count;
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

const char palette[16]={ 0x0f,0x00,0x10,0x30,0x0f,0x01,0x21,0x31,0x0f,0x06,0x16,0x26,0x0f,0x17,0x19,0x29 };

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
      clear_vram_buffer();
      pal_fade_to(4, 0);
      ppu_off(); // screen off
      load_enemy_formation(current_enemy_formation);
      ppu_on_all();
      update_health();
      update_chaos();
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
  set_scroll_x(enemy_area_x);
  set_scroll_y(enemy_area_y);
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
  health = 10;
  chaos = 0;
  chaos_counter = 240;
  player_collidable.width = HITBOX_WIDTH;
  player_collidable.height = HITBOX_HEIGHT;
  player_collidable.x = INT(player_x) - HITBOX_WIDTH/2;
  player_collidable.y = INT(player_y) - HITBOX_HEIGHT/2;
}

void update_health (void) {
  if (health == 10) return;
  // XXX: cheating, if updating health it's below 10
  one_vram_buffer(0x10, NTADR_A(11, 2));
  one_vram_buffer(0x10 + health, NTADR_A(12, 2));
}

void update_chaos (void) {
  if (chaos == 10) {
    one_vram_buffer(0x11, NTADR_A(26, 2));
    one_vram_buffer(0x10, NTADR_A(27, 2));
  } else {
    one_vram_buffer(0x10, NTADR_A(26, 2));
    one_vram_buffer(0x10 + chaos, NTADR_A(27, 2));
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

    temp_collidable_b.x = bullets_x[i];
    temp_collidable_b.y = bullets_y[i];
    if (IS_PLAYER_BULLET(i)) {
      if (temp_collidable_b.y > 0x64) continue;
      for(temp = 0; temp < num_enemies; ++temp) {
        temp_collidable_a.x = enemy_x[temp];
        temp_collidable_a.y = enemy_y[temp] - enemy_area_y;
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
      if (check_collision(&player_collidable, &temp_collidable_b)) {
        sfx_play(SFX_HIT, 0);
        delete_bullet(i);
        --i;
        if (health > 0) {
          --health;
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

  reset_bullets();

  enemy_row_movement = 0;
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

void player_shoot (void) {
  if (player_shoot_cd > 0 || get_num_bullets() >= MAX_BULLETS) return;

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
    break;
  case Tetro:
    if (player_bullet_count >= 4) return;
    ++player_bullet_count;
    player_shoot_cd = 8;
    player_bullets_cd = 75;
    set_bullet_x(player_x - FP(4, 0));
    set_bullet_y(player_y - FP(8, 0));
    set_bullet_type(PlayerBlock);
    set_bullet_delta_x(FP(0, 0));
    set_bullet_delta_y(-FP(1, 0));
    inc_bullets();
    break;
  }
  sfx_play(SFX_PEW, 0);
  return;
}

void enemy_shoot (void) {
  if (enemy_shoot_cd[temp] > 0 || get_num_bullets() >= MAX_BULLETS || enemy_row_movement > 0) return;
  if (enemy_bullet_count[temp] == 0 && rand8() > 32) return;

  temp_int_x = FP(enemy_x[temp] + enemy_width[temp] / 2 - 4, 0);
  temp_int_y = FP(enemy_y[temp] + enemy_height[temp] / 2 - enemy_area_y, 0);

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
      set_scroll_x(0);
      set_scroll_y(0);
      if (get_pad_new(0) & (PAD_START | PAD_A)) {
        sfx_play(SFX_START, 0);
        start_game();
      }
      break;
    case GamePlay:
      double_buffer[double_buffer_index++] = 0xc8;
      double_buffer[double_buffer_index++] = 0xf6;
      double_buffer[double_buffer_index++] = 0x20;
      double_buffer[double_buffer_index++] = 0x00;

      if (enemy_row_movement > 0) {
        --enemy_row_movement;
        --enemy_area_y;
      }

      if (--chaos_counter == 0) {
        if (chaos < 10) {
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
        if (chaos < 10) {
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

      set_scroll_x(enemy_area_x);
      set_scroll_y(enemy_area_y);

#define X_MARGIN 0x10
#define TOP_MARGIN 0x60
#define BOTTOM_MARGIN 0x20
      if (current_ship_mode == Tetro) {
        if (get_pad_new(0) & (PAD_LEFT)) {
          player_speed = -1;
          if (player_x > FP(X_MARGIN, 0)) {
            player_x -= TETRO_SPEED;
            player_collidable.x = INT(player_x) - HITBOX_WIDTH/2;
          }
        }
        if (get_pad_new(0) & (PAD_RIGHT)) {
          player_speed = 1;
          if (player_x < FP(0xff - X_MARGIN, 0)) {
            player_x += TETRO_SPEED;
            player_collidable.x = INT(player_x) - HITBOX_WIDTH/2;
          }
        }
        if (get_pad_new(0) & (PAD_UP)) {
          if (player_y > FP(TOP_MARGIN, 0)) {
            player_y -= TETRO_SPEED;
            player_collidable.y = INT(player_y) - HITBOX_HEIGHT/2;
          }
        }
        if (get_pad_new(0) & (PAD_DOWN)) {
          if (player_y < FP(0xef - BOTTOM_MARGIN, 0)) {
            player_y += TETRO_SPEED;
            player_collidable.y = INT(player_y) - HITBOX_HEIGHT/2;
          }
        }
      } else {
        if (pad_state(0) & (PAD_LEFT)) {
          player_speed = -1;
          if (player_x > FP(X_MARGIN, 0)) {
            player_x -= PLAYER_SPEED;
            player_collidable.x = INT(player_x) - HITBOX_WIDTH/2;
          }
        }
        if (pad_state(0) & (PAD_RIGHT)) {
          player_speed = 1;
          if (player_x < FP(0xff - X_MARGIN, 0)) {
            player_x += PLAYER_SPEED;
            player_collidable.x = INT(player_x) - HITBOX_WIDTH/2;
          }
        }
        if (pad_state(0) & (PAD_UP)) {
          if (player_y > FP(TOP_MARGIN, 0)) {
            player_y -= PLAYER_SPEED;
            player_collidable.y = INT(player_y) - HITBOX_HEIGHT/2;
          }
        }
        if (pad_state(0) & (PAD_DOWN)) {
          if (player_y < FP(0xef - BOTTOM_MARGIN, 0)) {
            player_y += PLAYER_SPEED;
            player_collidable.y = INT(player_y) - HITBOX_HEIGHT/2;
          }
        }
      }

      if (health == 0 || current_enemy_formation == MAX_FORMATIONS) {
        if (get_pad_new(0) & (PAD_START)) {
          go_to_title();
        }
        break;
      }
      if (pad_state(0) & (PAD_A)) {
        player_shoot();
      }
#ifdef DEBUG
      if (get_pad_new(0) & PAD_B) {
        for(i = 0; i < num_enemies; i++) {
          enemy_hp[i] = 1;
        }
      }
#endif

      break;
    case GameEnd:
      if (get_pad_new(0) & PAD_START) {
        sfx_play(SFX_SELECT, 0);
        go_to_title();
      }
      break;
    }

#ifdef DEBUG
    gray_line();
#endif

    // load the irq array with values it parse
    // ! CHANGED it, double buffered so we aren't editing the same
    // array that the irq system is reading from


    double_buffer[double_buffer_index++] = 0xff; // end of data

    draw_sprites();

#ifdef DEBUG
    gray_line();
#endif

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

void draw_ship (void) {
  temp_x = INT(player_x);
  temp_y = INT(player_y);
  if (current_enemy_formation == MAX_FORMATIONS) {
    oam_meta_spr(temp_x, temp_y, victory_sprite);
  } else if (health > 0) {
    if (chaos == 10) {
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

void draw_bullets (void) {
  for(i = get_frame_count() % 2; i < get_num_bullets(); i += 2) {
    switch(bullets_type[i]) {
    case PlayerBullet:
      oam_spr(bullets_x[i], bullets_y[i], 0x00, 0x01);
      break;
    case PlayerApple:
      oam_spr(bullets_x[i], bullets_y[i], 0x01, 0x02);
      break;
    case EnemyBullet:
      oam_spr(bullets_x[i], bullets_y[i], 0x00, 0x02);
      break;
    case PlayerBlock:
      oam_spr(bullets_x[i], bullets_y[i] & 0xf8, 0x8a, 0x03);
      break;
    }
  }
}

void draw_sprites (void) {
  oam_clear();
  if (current_game_state != GamePlay) return;

  draw_ship();

  draw_bullets();
}

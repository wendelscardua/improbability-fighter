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

#define FP(integer,fraction) (((integer)<<4)|((fraction)>>4))
#define INT(unsigned_fixed_point) ((unsigned_fixed_point>>4)&0xff)

#define PLAYER_SPEED FP(1, 128)

#define MAX_BULLETS 64
#define MAX_ENEMIES 4

#define IS_PLAYER_BULLET(index) (bullets_type[index] == PlayerBullet || bullets_type[index] == PlayerApple)

#pragma bss-name(push, "ZEROPAGE")

typedef enum {
              PlayerBullet,
              EnemyBullet,
              PlayerApple
} bullet_type;

typedef enum  {
               Trio
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
                Default
} current_ship_mode;

unsigned char enemy_area_x, enemy_area_y;
unsigned int player_x, player_y;
unsigned char player_shoot_cd, player_bullets_cd, player_bullet_count;

collidable temp_collidable_a, temp_collidable_b;

unsigned char num_bullets;
unsigned char num_enemies;

unsigned char current_enemy_formation;
unsigned char enemy_formation_index;
unsigned char enemy_row_movement;

#pragma bss-name(pop)
// should be in the regular 0x300 ram now

unsigned char irq_array[32];
unsigned char double_buffer[32];

#pragma bss-name(push, "XRAM")
// extra RAM at $6000-$7fff

unsigned int wram_start;
int bullets_x[MAX_BULLETS];
int bullets_y[MAX_BULLETS];
bullet_type bullets_type[MAX_BULLETS];
int bullets_delta_x[MAX_BULLETS];
int bullets_delta_y[MAX_BULLETS];

unsigned char enemy_index[MAX_ENEMIES];
unsigned char enemy_hp[MAX_ENEMIES];
unsigned char enemy_shoot_cd[MAX_ENEMIES];
unsigned char enemy_bullets_cd[MAX_ENEMIES];
unsigned char enemy_bullet_count[MAX_ENEMIES];
unsigned char enemy_x[MAX_ENEMIES];
unsigned char enemy_y[MAX_ENEMIES];
unsigned char enemy_width[MAX_ENEMIES];
unsigned char enemy_height[MAX_ENEMIES];

#pragma bss-name(pop)

// the fixed bank

#pragma rodata-name ("RODATA")
#pragma code-name ("CODE")

const unsigned char palette[16]={ 0x0f,0x00,0x10,0x30,0x0f,0x01,0x21,0x31,0x0f,0x06,0x16,0x26,0x0f,0x09,0x19,0x29 };

const unsigned char emptiness[] = { 0x00,0x00,0x00,0x00 };

const enemy enemies[] = { //c,  r, w, h, hp, pattern
                         { 14, 24, 4, 2, 10, Trio }, //1-1

                         {  6, 16, 4, 2, 10, Trio }, //1-2
                         { 22, 16, 4, 2, 10, Trio },

                         {  4,  6, 4, 2, 12, Trio }, //1-3
                         { 10,  8, 4, 2, 12, Trio },
                         { 18,  8, 4, 2, 12, Trio },
                         { 24,  6, 4, 2, 12, Trio }
};

const unsigned char formations[1][] = {
                                       {1, 0, 2, 1, 2, 4, 3, 4, 5, 6, 0}
};

void draw_sprites (void);

void init_wram (void) {
  //if (wram_start != 0xcafe)
  memfill(&wram_start,0,0x2000);
  //wram_start = 0xcafe;
}

unsigned char load_enemy_row (void) {
  temp = formations[current_enemy_formation][enemy_formation_index++];
  if (temp == 0) return 0;
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

    ++num_enemies;
  }
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

void init_ship (void) {
  current_ship_mode = Default;
  player_x = FP(0x80, 0);
  player_y = FP(0xa0, 0);
  player_shoot_cd = 0;
  player_bullet_count = 0;
}

void delete_bullet (void) {
  --num_bullets;
  bullets_x[i] = bullets_x[num_bullets];
  bullets_y[i] = bullets_y[num_bullets];
  bullets_delta_x[i] = bullets_delta_x[num_bullets];
  bullets_delta_y[i] = bullets_delta_y[num_bullets];
  bullets_type[i] = bullets_type[num_bullets];
  --i;
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

  --temp;

  temp_x = enemies[i].width;
  for(temp_y = enemies[i].height; temp_y > 0; --temp_y) {
    multi_vram_buffer_horz(emptiness, temp_x, NTADR_A(enemies[i].column, enemies[i].row + temp_y - 1));
  }
  // TODO kaboom

  if (num_enemies == 0) {
    if (load_enemy_row()) {
      enemy_row_movement = 64;
    }
  }
}

void update_bullets (void) {
  for(i = 0; i < num_bullets; ++i) {
    bullets_x[i] += bullets_delta_x[i];
    bullets_y[i] += bullets_delta_y[i];
    if (bullets_x[i] < FP(0, 0) || bullets_x[i] > FP(255, 0) ||
        bullets_y[i] < FP(0, 0) || bullets_y[i] > FP(240, 0)) {
      // delete bullet
      delete_bullet();
      continue;
    }
  }

  // XXX: hardcoded bullet size
  temp_collidable_b.width = 4;
  temp_collidable_b.height = 8;

  for(temp = 0; temp < num_enemies; ++temp) {
    temp_collidable_a.x = enemy_x[temp];
    temp_collidable_a.y = enemy_y[temp] - enemy_area_y;
    temp_collidable_a.width = enemy_width[temp];
    temp_collidable_a.height = enemy_height[temp];

    for(i = 0; i < num_bullets; ++i) {
      if (IS_PLAYER_BULLET(i)) {
        temp_collidable_b.x = INT(bullets_x[i]);
        temp_collidable_b.y = INT(bullets_y[i]);
        if (check_collision(&temp_collidable_a, &temp_collidable_b)) {
          delete_bullet();
          --enemy_hp[temp];
          if (enemy_hp[temp] == 0) {
            delete_enemy();
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

  num_bullets = 0;
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
  if (player_shoot_cd > 0 || num_bullets >= MAX_BULLETS) return;

  switch(current_ship_mode) {
  case Default:
    if (player_bullet_count >= 3) return;
    ++player_bullet_count;
    player_shoot_cd = 8;
    player_bullets_cd = 60;
    bullets_x[num_bullets] = player_x - FP(3, 0);
    bullets_y[num_bullets] = player_y - FP(8, 0);
    bullets_type[num_bullets] = PlayerBullet;
    bullets_delta_x[num_bullets] = FP(0, 0);
    bullets_delta_y[num_bullets] = -FP(2, 128);
    num_bullets++;
    break;
  }
  return;
}

void enemy_shoot (void) {
  if (enemy_shoot_cd[temp] > 0 || num_bullets >= MAX_BULLETS) return;
  temp_int_x = FP(enemy_x[temp] + enemy_width[temp] / 2 - 4, 0);
  temp_int_y = FP(enemy_y[temp] + enemy_height[temp] / 2 - enemy_area_y, 0);

  switch(enemies[i].pattern) {
  case Trio:
    if (enemy_bullet_count[temp] >= 2) return;
    ++enemy_bullet_count[temp];
    enemy_shoot_cd[temp] = 12;
    enemy_bullets_cd[temp] = 90;
    bullets_x[num_bullets] = temp_int_x;
    bullets_y[num_bullets] = temp_int_y + FP(8, 0);
    bullets_type[num_bullets] = EnemyBullet;
    bullets_delta_x[num_bullets] = FP(0, 0);
    bullets_delta_y[num_bullets] = FP(2, 0);
    num_bullets++;

    bullets_x[num_bullets] = temp_int_x + FP(8, 0);
    bullets_y[num_bullets] = temp_int_y + FP(8, 0);
    bullets_type[num_bullets] = EnemyBullet;
    bullets_delta_x[num_bullets] = FP(0, 96);
    bullets_delta_y[num_bullets] = FP(2, 0);
    num_bullets++;

    bullets_x[num_bullets] = temp_int_x - FP(8, 0);
    bullets_y[num_bullets] = temp_int_y + FP(8, 0);
    bullets_type[num_bullets] = EnemyBullet;
    bullets_delta_x[num_bullets] = -FP(0, 96);
    bullets_delta_y[num_bullets] = FP(2, 0);
    num_bullets++;
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
      ++temp;
      if (get_pad_new(0) & (PAD_START | PAD_A)) {
        start_game();
      }
      break;
    case GamePlay:
      if (enemy_row_movement > 0) {
        --enemy_row_movement;
        --enemy_area_y;
      }

      update_bullets();

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
      if (pad_state(0) & (PAD_LEFT)) {
        player_x -= PLAYER_SPEED;
      }
      if (pad_state(0) & (PAD_RIGHT)) {
        player_x += PLAYER_SPEED;
      }
      if (pad_state(0) & (PAD_UP)) {
        player_y -= PLAYER_SPEED;
      }
      if (pad_state(0) & (PAD_DOWN)) {
        player_y += PLAYER_SPEED;
      }
      if (pad_state(0) & (PAD_A)) {
        player_shoot();
      }
      // TODO bounded movement

      break;
    case GameEnd:
      if (get_pad_new(0) & PAD_START) {
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

  temp_x = INT(player_x);
  temp_y = INT(player_y);

  switch(current_ship_mode) {
  case Default:
    oam_meta_spr(temp_x, temp_y, default_ship_sprite);
    break;
  }

  for(i = 0; i < num_bullets; ++i) {
    switch(bullets_type[i]) {
    case PlayerBullet:
      oam_spr(INT(bullets_x[i]), INT(bullets_y[i]), 0x00, 0x01);
      break;
    case PlayerApple:
      oam_spr(INT(bullets_x[i]), INT(bullets_y[i]), 0x01, 0x02);
      break;
    case EnemyBullet:
      oam_spr(INT(bullets_x[i]), INT(bullets_y[i]), 0x00, 0x02);
      break;
    }
  }
}

#define MAX_BULLETS 32

typedef enum {
              PlayerBullet,
              EnemyBullet,
              PlayerApple,
              PlayerBlock
} bullet_type;

extern unsigned char bullets_x[MAX_BULLETS];
extern unsigned char bullets_sx[MAX_BULLETS];
extern unsigned char bullets_y[MAX_BULLETS];
extern unsigned char bullets_sy[MAX_BULLETS];

extern bullet_type bullets_type[MAX_BULLETS];

extern unsigned char bullets_delta_x[MAX_BULLETS];
extern unsigned char bullets_delta_sx[MAX_BULLETS];
extern unsigned char bullets_delta_y[MAX_BULLETS];
extern unsigned char bullets_delta_sy[MAX_BULLETS];

void __fastcall__ delete_bullet (unsigned char index);
unsigned char __fastcall__ get_num_bullets (void);
void __fastcall__ inc_bullets (void);
void __fastcall__ reset_bullets (void);
void __fastcall__ update_bullets (void);
void __fastcall__ set_bullet_x (unsigned int x);
void __fastcall__ set_bullet_y (unsigned int y);
void __fastcall__ set_bullet_type (bullet_type type);
void __fastcall__ set_bullet_delta_x (int delta_x);
void __fastcall__ set_bullet_delta_y (int delta_y);

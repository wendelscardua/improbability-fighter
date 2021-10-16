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

void delete_bullet (unsigned char index);
unsigned char get_num_bullets (void);
void inc_bullets (void);
void reset_bullets (void);
void update_bullets (void);

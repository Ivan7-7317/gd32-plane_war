#ifndef GAME_H
#define GAME_H

#include <stdint.h>
#include <stdbool.h>
#include "lvgl.h"
#include "drivers.h"

#define MAX_ENEMIES 10
#define MAX_ENEMY_BULLETS 40

// 敌机结构体 (引入真正的物理坐标)
typedef struct {
    lv_obj_t *obj;
    bool active;
    int hp;
    int x;
    int y;
    int dir_x;
    int dir_y;
    int move_counter;
    int shoot_counter;
    int move_mode;
    int mode_timer;
} enemy_t;

// 敌机子弹结构体 (区分直线与S型)
typedef struct {
    lv_obj_t *obj;
    bool active;
    int x;
    int y;
    int type;   // 0: 直线, 1: S型
    int base_x; // S型子弹基准X
    int angle;  // S型子弹当前角度
} enemy_bullet_t;

// Boss结构体
typedef struct {
    lv_obj_t *obj;
    bool active;
    int hp;
    int x;
    int y;
    int dir_x;
    int shoot_counter;
} boss_t;

void game_init(void);
void show_game_screen(void);
void game_update_health(int32_t health);
void init_game_entities(void);
void game_shoot(void);

extern int player_hp;
extern int player_x;
extern int player_y;

#endif

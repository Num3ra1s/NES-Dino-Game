#include "neslib.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

// 0 = horizontal mirroring
// 1 = vertical mirroring
#define NES_MIRRORING 1

// VRAM update buffer
#include "vrambuf.h"
//#link "vrambuf.c"

extern const byte title_pal[16];
extern const byte title_rle[];

// link the pattern table into CHR ROM
//#link "chr_generic.s"

// link title screen palette and RLE nametable
//#link "title.s"


/// GLOBAL VARIABLES

word x_scroll;		// x scroll amount in pixels
unsigned int distance;	// distance player has gone 
bool draw_obs;		// bool for randomization of obstacle drawing
bool game_over = false;	// game status
bool intro = true;	// intro status
byte loop_count;	// loop counter

typedef struct Obstacle {
  int x;
  int y;
  bool drawn;
} Obstacle;

Obstacle obstacles[3];

typedef struct Player {
  int x;
  int y;
  int vel;
  int acc;
  bool jumped;
} Player;

Player player;

///// METASPRITES

#define DINO 0xd8
#define GROUND1 0xe6
#define GROUND2 0xe8
#define GROUND3 0xea
#define GROUND4 0xec
#define GROUND5 0xee
#define GROUND6 0xf0
#define CACTUS 0xd2
#define ATTR 0

const unsigned char dino[]={
        0,      0,      0,        ATTR, 
        0,      8,      DINO+1,   ATTR, 
        0,      16,     DINO+2,   ATTR, 
        8,      0,      DINO+3,   ATTR, 
  	8,	8,	DINO+4,   ATTR,
  	8,	16,	DINO+5,	  ATTR,
  	16,     0,      DINO+6,   ATTR, 
  	16,	8,	DINO+7,   ATTR,
  	16,	16,	0,	  ATTR,
        128};

const unsigned char dinostep1[]={
        0,      0,      0,        ATTR, 
        0,      8,      DINO+1,   ATTR, 
        0,      16,     DINO+8,   ATTR, 
        8,      0,      DINO+3,   ATTR, 
  	8,	8,	DINO+4,   ATTR,
  	8,	16,	DINO+9,	  ATTR,
  	16,     0,      DINO+6,   ATTR, 
  	16,	8,	DINO+7,   ATTR,
  	16,	16,	0,	  ATTR,
        128};

const unsigned char dinostep2[]={
        0,      0,      0,        ATTR, 
        0,      8,      DINO+1,   ATTR, 
        0,      16,     DINO+10,  ATTR, 
        8,      0,      DINO+3,   ATTR, 
  	8,	8,	DINO+4,   ATTR,
  	8,	16,	DINO+11,  ATTR,
  	16,     0,      DINO+6,   ATTR, 
  	16,	8,	DINO+7,   ATTR,
  	16,	16,	0,	  ATTR,
        128};

const unsigned char dinodead[]={
        0,      0,      0,        ATTR, 
        0,      8,      DINO+1,   ATTR, 
        0,      16,     DINO+2,   ATTR, 
        8,      0,      DINO+12,  ATTR, 
  	8,	8,	DINO+4,   ATTR,
  	8,	16,	DINO+5,	  ATTR,
  	16,     0,      DINO+13,  ATTR, 
  	16,	8,	DINO+7,   ATTR,
  	16,	16,	0,	  ATTR,
        128};

const unsigned char cactus[]={
	0,	0,	CACTUS,	  ATTR,
  	0,	8,	CACTUS+1, ATTR,
  	0,	16, 	CACTUS+2, ATTR,
	8,	0,	CACTUS+3, ATTR,
  	8,	8,	CACTUS+4, ATTR,
  	8,	16, 	CACTUS+5, ATTR,
	128};

/*{pal:"nes",layout:"nes"}*/
const char PALETTE[32] = { 
  0x00,	// screen color

  0x00,0x00,0x00,0x00,	// background 1
  0x00,0x00,0x00,0x00,	// background 2
  0x00,0x00,0x00,0x00,	// background 3
  0x00,0x00,0x00,0x00,	// background 4

  0x0C,0x0C,0x0C,0x00,	// sprite 1
  0x00,0x00,0x00,0x00,	// sprite 2
  0x00,0x00,0x00,0x00,	// sprite 3
  0x00,0x00,0x00	// sprite 4
};

// number of rows in scrolling playfield (without status bar)
#define PLAYROWS 24

// buffers that hold vertical slices of nametable data
char ntbuf1[PLAYROWS];	// left side
char ntbuf2[PLAYROWS];	// right side

/// FUNCTIONS

// function to write a string into the name table
//   adr = start address in name table
//   str = pointer to string
void put_str(unsigned int adr, const char *str) {
  vram_adr(adr); // set PPU read/write address
  vram_write(str, strlen(str)); // write bytes to PPU
}

// draw ground metatile into nametable buffers
// 	y is the metatile coordinate (row * 2)
// 	ch is the starting tile index in the pattern table
void set_ground_metatile(byte y, byte ch) {
  ntbuf1[y*2] = ch;
  ntbuf2[y*2] = ch+1;
}

// draw cactus metatile into nametable buffers
// 	y is the metatile coordinate (row * 2)
// 	ch is the starting tile index in the pattern table 
void set_cactus_metatile(byte y, byte ch) {
  ntbuf1[y*2] = ch;
  ntbuf1[y*2+1] = ch+1;
  ntbuf1[y*2+2] = ch+2;
  ntbuf2[y*2] = ch+3;
  ntbuf2[y*2+1] = ch+4;
  ntbuf2[y*2+2] = ch+5;
}

// fill ntbuf with tile data
void fill_buffer() {
  byte y, i, ground_rand;
  // clear nametable buffers
  memset(ntbuf1, 0, sizeof(ntbuf1));
  memset(ntbuf2, 0, sizeof(ntbuf2));
  
  //draw floor
  y = PLAYROWS/2-1-0;
  ground_rand = rand() % (14);
  if(ground_rand <= 4){
    set_ground_metatile(y, GROUND4);
  }
  else if(ground_rand <= 9){
    set_ground_metatile(y, GROUND6);
  }
  else if(ground_rand <= 10){
    set_ground_metatile(y, GROUND1);
  }
  else if(ground_rand <= 11){
    set_ground_metatile(y, GROUND2);
  }
  else if(ground_rand <= 12){
    set_ground_metatile(y, GROUND3);
  }
  else {
    set_ground_metatile(y, GROUND5);
  }
  
  //draw obs
  for(i = 0; i < 3; i++) {
    if(obstacles[i].drawn == false) {
      if(draw_obs) {
        obstacles[i].x = 240; 
        obstacles[i].y = 10;
        obstacles[i].drawn = true;
        set_cactus_metatile(obstacles[i].y, CACTUS);
        break;
      }
    }
  }
}

// update the nametable offscreen
void update_offscreen() {
  register word addr;
  byte x;
  
  // divide x_scroll by 8
  // to get nametable X position
  x = (x_scroll/8 + 32) & 63;
  // randomized whether to draw obstacle or not
  draw_obs = (rand() % (100 + 1) + 1) % 8 == 0;
  // fill the ntbuf arrays with tiles
  fill_buffer();
  // get address in either nametable A or B
  if (x < 32)
    addr = NTADR_A(x, 4);
  else
    addr = NTADR_B(x&31, 4);
  // draw vertical slice from ntbuf arrays to name table
  // starting with leftmost slice
  vrambuf_put(addr | VRAMBUF_VERT, ntbuf1, PLAYROWS);
  // then the rightmost slice
  vrambuf_put((addr+1) | VRAMBUF_VERT, ntbuf2, PLAYROWS);
}

// scrolls the screen left one pixel
void scroll_left() {
  byte i;
  
  // update nametable every 16 pixels
  if ((x_scroll & 15) == 0) {
    update_offscreen();
  }
  
  // update x coordinates of obstacles
  for(i = 0; i < 3; i++) {
    if(obstacles[i].x >= 0) {
      obstacles[i].x--;
    } else {
      obstacles[i].drawn = false;
    }
  }
  
  // obstacle collision detection
  for(i = 0; i < 3; i++) { 
    if(player.x-2 >= obstacles[i].x &&
       player.x <= obstacles[i].x + 18 &&
       player.y >= obstacles[i].y + 184 - 24 &&
       player.y <= obstacles[i].y + 184) {
      game_over = true;
    }
  }
  
  // increment x_scroll
  ++x_scroll;
}

// fade in title screen
void fade_in() {
  byte vb;
  for (vb=0; vb<=4; vb++) {
    // set virtual bright value
    pal_bright(vb);
    // wait for 4/60 sec
    ppu_wait_frame();
    ppu_wait_frame();
    ppu_wait_frame();
    ppu_wait_frame();
  }
}

// display title screen
//	pal is the palette
//	rle is the rle compressed nametable
void show_title_screen(const byte* pal, const byte* rle) {
  // disable rendering
  ppu_off();
  // set palette, virtual bright to 0 (total black)
  pal_bg(pal);
  pal_bright(0);
  // unpack nametable into the VRAM
  vram_adr(0x2000);
  vram_unrle(rle);
  // enable rendering
  ppu_on_all();
  // fade in from black
  fade_in();
}

// main loop, scrolls left continuously
void dino_game() {
  bool start = false;
  int hover = -1;
  char sbuf[4];
  int animation = 0;
  distance = 0; 
 
  // init game data
  x_scroll = 0;
  player.x = 50;
  player.y = 192;
  player.acc = -2;
  player.jumped = false;
  loop_count = 0;
  
  // clear start text
  vrambuf_put(NTADR_A(2, 1), "", 0);
  
  // infinite loop, called ~29780 times per frame, 60 frames per second
  while (true) {
    // check for game start
    if(!start){
      // start game when start button pressed
      char pad = pad_poll(0);
      if(pad&PAD_START){
        start = true;
      }
    } 
    // game started
    else {
      // game playing
      if(game_over == false) {
        // get controller
        char pad = pad_poll(0);

        // ensure VRAM buffer is cleared
        ppu_wait_nmi();
        vrambuf_clear();

        // split at sprite zero and sets X scroll register
        split(x_scroll, 0);

        // scroll to the left
        scroll_left();
        scroll_left();

        // player jump     
        if(pad&PAD_A && !player.jumped){
          player.jumped = true;
          player.vel = 13;
        }
        if(player.jumped && loop_count % 2 == 0){
          player.y -= player.vel;
          player.vel += player.acc;
        }
        if(player.jumped && player.y >= 192) {
          player.y = 192;
          player.jumped = false;
        }

        // draw player
        if(animation % 2 == 0){
          oam_meta_spr(player.x, player.y, 4, dinostep1);
        }
        else {
          oam_meta_spr(player.x, player.y, 4, dinostep2);
        }

        // increase animation counter
        if(loop_count % 15 == 0) {
          animation++;
        }

        // increase distance counter
        if(loop_count % 45 == 0) {
          distance++;

          //write distance
          sprintf(sbuf, "Distance: %d", distance);
          vrambuf_put(NTADR_A(2, 2), sbuf, strlen(sbuf));
        }
	
        // increase loop counter
        loop_count++;
        if(loop_count > 60) {
          loop_count = 1;
        }
      }
      // game ended
      else {      
        // ensure VRAM buffer is cleared
        ppu_wait_nmi();
        vrambuf_clear();

        // split at sprite zero and set X scroll
        split(x_scroll, 0);

        // draw player
        oam_meta_spr(player.x, player.y, 4, dinodead); 

        // write distance
        sprintf(sbuf, "Distance: %d", distance);
        vrambuf_put(NTADR_A(2, 2), sbuf, strlen(sbuf));

        // write game over message
        vrambuf_put(NTADR_A(2, 3), "GAME OVER!", 9);
      }
    } 
  }
}

// main function, run after console reset
void main(void) {
  byte i;
  
  // set palette colors
  pal_all(PALETTE);
  
  // set sprite 0
  oam_clear();
  oam_spr(1, 30, 0xa0, 0, 0);
  
  // clear vram buffer
  vrambuf_clear();
  set_vram_update(updbuf);
  
  // enable PPU rendering (turn on screen)
  ppu_on_all();
  
  // init obstacle array
  for(i = 0; i < 3; i++) {
    obstacles[i].x = -1;
    obstacles[i].y = 0;
    obstacles[i].drawn = false;
  }
  
  // show title screen
  show_title_screen(title_pal, title_rle);
  
  // play game
  dino_game();
}
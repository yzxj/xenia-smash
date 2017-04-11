/*
-----------------------------------------------------------------------------
-- Copyright (C) 2005 IMEC                                                  -
--                                                                          -
-- Redistribution and use in source and binary forms, with or without       -
-- modification, are permitted provided that the following conditions       -
-- are met:                                                                 -
--                                                                          -
-- 1. Redistributions of source code must retain the above copyright        -
--    notice, this list of conditions and the following disclaimer.         -
--                                                                          -
-- 2. Redistributions in binary form must reproduce the above               -
--    copyright notice, this list of conditions and the following           -
--    disclaimer in the documentation and/or other materials provided       -
--    with the distribution.                                                -
--                                                                          -
-- 3. Neither the name of the author nor the names of contributors          -
--    may be used to endorse or promote products derived from this          -
--    software without specific prior written permission.                   -
--                                                                          -
-- THIS CODE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS''           -
-- AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED        -
-- TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A          -
-- PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR       -
-- CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,             -
-- SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT         -
-- LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF         -
-- USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND      -
-- ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,       -
-- OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT       -
-- OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF       -
-- SUCH DAMAGE.                                                             -
--                                                                          -
-----------------------------------------------------------------------------
-----------------------------------------------------------------------------
-- File           : threads_RR.c
-----------------------------------------------------------------------------
-- Description    : C code
-- --------------------------------------------------------------------------
-- Author         : Kristof Loots
-- Date           : 14/09/2006
-- Version        : V1.0
-- Change history :
-----------------------------------------------------------------------------
*/
/***************************** Include Files ********************************/

#include "xtft.h"
#include "xparameters.h"
#include "xmk.h"
#include "sys/init.h"
#include "xgpio.h"
#include "xuartps.h"
#include "xmbox.h"
#include <sys/timer.h> // for using sleep. need to set config_time to true
#include <sys/intr.h> // xilkernel api for interrupts
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/************************** Constant Definitions ****************************/
/**
* The following constants map to the XPAR parameters created in the
* xparameters.h file. They are defined here such that a user can easily
* change all the needed parameters in one place.
*/
#define TFT_DEVICE_ID    XPAR_TFT_0_DEVICE_ID
#define DDR_HIGH_ADDR    XPAR_PS7_DDR_0_S_AXI_HIGHADDR
#define XST_SUCCESS      0L
#define XST_FAILURE      1L

/**
* User has to specify a 2MB memory space for filling the frame data.
* This constant has to be updated based on the memory map of the
* system.
*/
#define TFT_FRAME_ADDR        0x10000000

#ifndef DDR_HIGH_ADDR
#warning "CHECK FOR THE VALID DDR ADDRESS IN XPARAMETERS.H"
#endif

#define DISPLAY_COLUMNS  640
#define DISPLAY_ROWS     480
#define WHITE            0x00ffffff
#define BLACK            0x00000000
#define RED              0x003f0000
#define ORANGE           0x003f2500
#define YELLOW           0x003f3f00
#define GREEN            0x001f3f1f
#define BLUE             0x00001f3f
#define PURPLE           0x003f1f3f
#define PINK             0x0032cd32
#define ASCII_ZERO       48

#define SCOREAREA_LEFT 		535
#define SCOREAREA_TOP 		60
#define SCOREAREA_RIGHT 	600
#define SCOREAREA_BOTTOM 	420
#define PLAYAREA_LEFT 		60
#define PLAYAREA_TOP 		60
#define PLAYAREA_WIDTH 		455
#define PLAYAREA_HEIGHT 	360
#define PLAYAREA_RIGHT 		(PLAYAREA_LEFT+PLAYAREA_WIDTH)
#define PLAYAREA_BOTTOM 	(PLAYAREA_TOP+PLAYAREA_HEIGHT)
#define BRICK_WIDTH 		40
#define BRICK_HEIGHT 		15
#define BRICK_GAP 			5
#define BRICK_ROWS 			8
#define BRICK_COLS 			10
#define BRICK_TOTAL 		(BRICK_ROWS*BRICK_COLS)
#define BAR_TOP_Y 			(PLAYAREA_TOP + PLAYAREA_HEIGHT - BAR_HEIGHT - 10)
#define BAR_HEIGHT 			5
#define BAR_WIDTH_TOTAL 	80
#define BAR_WIDTH_N 		40
#define BAR_WIDTH_S 		10
#define BAR_WIDTH_A 		10
#define BALL_RADIUS 		7
#define BAR_WAIT_CLOCKS 	25
#define BAR_SPEED_1 		25
#define BAR_SPEED_2 		8

/**************************** Type Definitions ******************************/
#define MSG_MAX_DESTROYED		8
#define MSG_BYTES				7*4 + MSG_MAX_DESTROYED*2*4
typedef struct {
  int old_gold_col, new_gold_col;
  int ball_x_pos, ball_y_pos;
  int game_won;
  int total_score;
  int destroyed_num;
  int destroyed_x[MSG_MAX_DESTROYED];
  int destroyed_y[MSG_MAX_DESTROYED];
} state_msg;

typedef struct {
  int bar1_x, bar2_x;
} bar_msg;

/************************** Function Prototypes *****************************/

int TftInit(u32 TftDeviceId);
int TftPrint();
int TftDrawCircle(XTft* Tft, int cx, int cy, int radius, u32 col);
void* thread_func_0();
void* thread_func_1();
void* main_prog(void *arg);

/************************** Variable Definitions ****************************/

/*	Mailbox Declaration	*/
#define MY_CPU_ID 			  XPAR_CPU_ID
#define MBOX_DEVICE_ID		  XPAR_MBOX_0_DEVICE_ID
static XMbox Mbox;			//Instance of the Mailbox driver

static XTft TftInstance;
XGpio gpPB; //PB device instance.

struct sched_param sched_par;
pthread_attr_t attr;

pthread_t tid0, tid1;
volatile int taskrunning;
volatile int ball_x = DISPLAY_COLUMNS / 2;
volatile int ball_y = DISPLAY_ROWS / 2;
volatile int ball_prev_x = DISPLAY_COLUMNS / 2 - 1;
volatile int ball_prev_y = DISPLAY_ROWS / 2 - 1;
volatile int score = 0;
volatile int time_elapsed = 0;
volatile int start_time = 0;
volatile int ball_speed = 10;
volatile int bricks_left = BRICK_TOTAL;
volatile int destroyed[80] = {0};
volatile int bar_x = (PLAYAREA_LEFT + PLAYAREA_RIGHT - BAR_WIDTH_TOTAL)/2;
volatile int col_golden[] = {1,2};

volatile int button_pressed = 0;
volatile int button_quick_pressed;
int button_pressed_time = 0;

/************************** Function Definitions ****************************/
int TftWriteWord(char word[]) {
  int i;
  int len = strlen(word);
  for (i=0; i<len; i++) {
    XTft_Write(&TftInstance, word[i]);
  }
  return 0;
}

int TftPrintScoreWords() {
  // Current score, Time elapsed, Current ball speed, Current bricks left

  // Score area is written in black on white background
  XTft_SetColor(&TftInstance, BLACK, WHITE);

  // Write "Score"
  XTft_SetPosChar(&TftInstance, SCOREAREA_LEFT, SCOREAREA_TOP);
  char score_word[] = "Score";
  TftWriteWord(score_word);

  // Write "Time Elapsed"
  XTft_SetPosChar(&TftInstance, SCOREAREA_LEFT, SCOREAREA_TOP + 50);
  char time_word[] = "Time Elapsed";
  TftWriteWord(time_word);

  // Write "Ball Speed"
  XTft_SetPosChar(&TftInstance, SCOREAREA_LEFT, SCOREAREA_TOP + 100);
  char speed_word[] = "Ball Speed";
  TftWriteWord(speed_word);

  // Write "Bricks Left"
  XTft_SetPosChar(&TftInstance, SCOREAREA_LEFT, SCOREAREA_TOP + 150);
  char bricks_word[] = "Bricks Left";
  TftWriteWord(bricks_word);

  return 0;
}

int TftPrintScore() {
  // Current score, Time elapsed, Current ball speed, Current bricks left
  int i;

  // Score area is written in black on white background
  XTft_SetColor(&TftInstance, BLACK, WHITE);

  // Write score value
  XTft_SetPosChar(&TftInstance, SCOREAREA_LEFT, SCOREAREA_TOP + 20);
  for (i=100; i>=1; i/=10) {
    // Write each digit, starting with 100s place.
    XTft_Write(&TftInstance, (char)(ASCII_ZERO + (score / i) % 10));
  }

  // Write time value
  XTft_SetPosChar(&TftInstance, SCOREAREA_LEFT, SCOREAREA_TOP + 70);
  for (i=1000; i>=1; i/=10) {
    // Write each digit, starting with 1000s place.
    XTft_Write(&TftInstance, (char)(ASCII_ZERO + (time_elapsed / i) % 10));
  }

  // Write speed value
  XTft_SetPosChar(&TftInstance, SCOREAREA_LEFT, SCOREAREA_TOP + 120);
  for (i=1000; i>=1; i/=10) {
    // Write each digit, starting with 1000s place.
    XTft_Write(&TftInstance, (char)(ASCII_ZERO + (ball_speed / i) % 10));
  }

  // Write bricks_left value
  XTft_SetPosChar(&TftInstance, SCOREAREA_LEFT, SCOREAREA_TOP + 170);
  for (i=100; i>=1; i/=10) {
    // Write each digit, starting with 100s place.
    XTft_Write(&TftInstance, (char)(ASCII_ZERO + (bricks_left / i) % 10));
  }

  return 0;
}

int TftDrawBall(XTft* Tft, int x, int y, u32 col) {
  int i,j;
  for (i=-BALL_RADIUS; i<BALL_RADIUS; i++) {
    for (j=-BALL_RADIUS; j<BALL_RADIUS; j++) {
      if (i*i + j*j <= BALL_RADIUS*BALL_RADIUS) {
        XTft_SetPixel(Tft, x+i, y+j, col);
      }
    }
  }
  return 0;
}

int TftDrawRect(XTft* Tft, int x, int y, int width, int height, u32 col) {
  XTft_FillScreen(Tft, x,y, x+width,y+height, col);
  return 0;
}

int TftDrawBar(XTft* Tft, int x, int y) {
  int bx = x;
  TftDrawRect(Tft, bx,y, BAR_WIDTH_A, BAR_HEIGHT, RED);
  bx += BAR_WIDTH_A;
  TftDrawRect(Tft, bx,y, BAR_WIDTH_S, BAR_HEIGHT, ORANGE);
  bx += BAR_WIDTH_S;
  TftDrawRect(Tft, bx,y, BAR_WIDTH_N, BAR_HEIGHT, YELLOW);
  bx += BAR_WIDTH_N;
  TftDrawRect(Tft, bx,y, BAR_WIDTH_S, BAR_HEIGHT, ORANGE);
  bx += BAR_WIDTH_S;
  TftDrawRect(Tft, bx,y, BAR_WIDTH_A, BAR_HEIGHT, RED);
  return 0;
}

int TftDrawColumn(XTft* Tft, int col_num, u32 col) {
	int i;
	int start_i = col_num * BRICK_ROWS;
	int rect_x = PLAYAREA_LEFT + BRICK_GAP + col_num*(BRICK_WIDTH+BRICK_GAP);
	for (i=0; i<BRICK_ROWS; i++) {
		int curr_i = start_i + i;
		if (!destroyed[curr_i]) {
			int rect_y = PLAYAREA_TOP + BRICK_GAP + i*(BRICK_HEIGHT+BRICK_GAP);
			TftDrawRect(Tft, rect_x, rect_y, BRICK_WIDTH, BRICK_HEIGHT, col);
		}
	}

	return 0;
}

int TftDrawColumns(XTft* Tft) {
	int i;
	for (i=0; i<BRICK_COLS; i++) {
		if (i==col_golden[0] || i==col_golden[1]) {
			TftDrawColumn(Tft, i, ORANGE);
		} else {
			TftDrawColumn(Tft, i, GREEN);
		}
	}
	return 0;
}

void shift_bar(int shift_x) {
  int bar_left = bar_x + shift_x;
  int bar_right = bar_left + BAR_WIDTH_TOTAL;
  if (bar_left<=PLAYAREA_LEFT) {
	bar_x = PLAYAREA_LEFT+1;
  } else if (bar_right>=PLAYAREA_RIGHT) {
    bar_x = PLAYAREA_RIGHT - BAR_WIDTH_TOTAL - 1;
  } else {
	  bar_x += shift_x;
  }
}

void shift_bar_and_redraw(int btn, int amount){
	TftDrawRect(&TftInstance, bar_x, BAR_TOP_Y, BAR_WIDTH_TOTAL, BAR_HEIGHT, BLACK); // TODO: BG_COL
	switch (btn) {
	  case 4:
	  shift_bar(-amount);
	  break;
	  case 8:
	  shift_bar(amount);
	  break;
	}
	TftDrawBar(&TftInstance, bar_x, BAR_TOP_Y);
}

int clocks_since(int last_time) {
	return xget_clock_ticks() - last_time;
}

/* ----------------------------------------------------
* Function to send data structs over to MB1 via mailbox
* ----------------------------------------------------
*/
void send(int bar1_location_x, int bar2_location_x) {
  struct bar_msg send_bar_msg;
  send_msg.bar1_x = bar1_location_x;
  send_msg.bar2_x = bar2_location_x;
  XMbox_WriteBlocking(&Mbox, &send_bar_msg, 8);
}


void* thread_func_0 () {
  while(1) {
	if (button_quick_pressed && !button_pressed && clocks_since(button_pressed_time)<BAR_WAIT_CLOCKS) {
		shift_bar_and_redraw(button_quick_pressed, BAR_SPEED_1);
		button_quick_pressed = 0;
	} else if (button_pressed && clocks_since(button_pressed_time) >= BAR_WAIT_CLOCKS) {
    	shift_bar_and_redraw(button_pressed, BAR_SPEED_2);
    }
    sleep(40);
  }
}

void update_state(state_msg data) {
  col_golden[0] = data.old_gold_col;
  col_golden[1] = data.new_gold_col;
  ball_x = data.ball_x_pos;
  ball_y = data.ball_y_pos;
  score = data.total_score;
  // TODO: Destroy bricks?
  time_elapsed = (xget_clock_ticks() - start_time) / 100;
}

static void Mailbox_Receive(XMbox *MboxInstancePtr) {
  state_msg update_data;
  u32 bytes_read;
	XMbox_Read(MboxInstancePtr, &update_data, MSG_BYTES, &bytes_read);
	if (bytes_read==MSG_BYTES) {
		// Remove old Screen Components and draw new ones
		// TODO: #define COL_BG = BLACK;
		TftDrawBall(&TftInstance, ball_x,ball_y, BLACK);
		TftDrawBall(&TftInstance, update_data.ball_x_pos,update_data.ball_y_pos, WHITE);
		// TODO: Destroy bricks
		// TODO: 2 bars 2 balls
		if (update_data.old_gold_col != col_golden[0] || update_data.new_gold_col != col_golden[1]){
			TftDrawColumn(&TftInstance, col_golden[0], GREEN);
			TftDrawColumn(&TftInstance, col_golden[1], GREEN);
			TftDrawColumn(&TftInstance, update_data.old_gold_col, ORANGE);
			TftDrawColumn(&TftInstance, update_data.new_gold_col, ORANGE);
		}
		// Save new data, including score/time/speed/bricksleft
		update_state(update_data);
		// Print score last, since update_state might have affected print values
		TftPrintScore();
	}
}

void* thread_func_1 () {
  while(1) {
    //TODO: send(bar1_x, bar2_x);     define second bar
    Mailbox_Receive(&Mbox);
	sleep(40);
  }
}

#define DEBOUNCE_CLOCKS 5
static void gpPBIntHandler(void *arg) //Should be very short (in time). In a practical program, don't print etc.
{
  // clear the interrupt flag. if this is not done, gpio will keep interrupting the microblaze.--
  // --Possible to use (XGpio*)arg instead of &gpPB
  XGpio_InterruptClear(&gpPB,1);
  // Read the state of the push buttons.
  button_pressed = XGpio_DiscreteRead(&gpPB, 1);
  // Shift bar by 25 on press if released within a short period.
  // Wait 250ms before moving 8px per interval.
  int time_diff = clocks_since(button_pressed_time);
  if (button_pressed != 0 && time_diff > DEBOUNCE_CLOCKS) {
	  // TODO: Problem with bouncing release
	button_quick_pressed = button_pressed;
	button_pressed_time = xget_clock_ticks();
  }
}

int game_screen_init() {
  XTft_SetPos(&TftInstance, 0,0);
  TftDrawRect(&TftInstance, PLAYAREA_LEFT, PLAYAREA_TOP, PLAYAREA_WIDTH,PLAYAREA_HEIGHT, BLACK);
  TftDrawColumns(&TftInstance);
  // TODO: Add second bar and ball
  TftDrawBar(&TftInstance, bar_x,BAR_TOP_Y);
  TftDrawBall(&TftInstance, ball_x,ball_y, WHITE);
  TftPrintScoreWords();
  TftPrintScore();
  start_time = xget_clock_ticks();
  return 0;
}

int mailbox_init() {
  int Status;
  XMbox_Config *ConfigPtr;

  // Configure and init mailbox
  ConfigPtr = XMbox_LookupConfig(MBOX_DEVICE_ID );
  if (ConfigPtr == (XMbox_Config *)NULL) {
	print("-- Error configuring Mbox uB0 receiver--\r\n");
	return XST_FAILURE;
  }

  Status = XMbox_CfgInitialize(&Mbox, ConfigPtr, ConfigPtr->BaseAddress);
  if (Status != XST_SUCCESS) {
	print("-- Error initializing Mbox uB0 receiver--\r\n");
	return XST_FAILURE;
  }

  return XST_SUCCESS;

}

int pb_init() {
  int Status;

  xil_printf("Initializing PB\r\n");
  // Initialise the PB instance
  Status = XGpio_Initialize(&gpPB, XPAR_GPIO_0_DEVICE_ID);
  // set PB gpio direction to input.
  XGpio_SetDataDirection(&gpPB, 1, 0x000000FF);

  xil_printf("Enabling PB interrupts\r\n");
  //global enable
  XGpio_InterruptGlobalEnable(&gpPB);
  // interrupt enable. both global enable and this function should be called to enable gpio interrupts.
  XGpio_InterruptEnable(&gpPB,1);
  //register the handler with xilkernel
  register_int_handler(XPAR_MICROBLAZE_0_AXI_INTC_AXI_GPIO_0_IP2INTC_IRPT_INTR, gpPBIntHandler, &gpPB);
  //enable the interrupt in xilkernel
  enable_interrupt(XPAR_MICROBLAZE_0_AXI_INTC_AXI_GPIO_0_IP2INTC_IRPT_INTR);
  return Status;
}


// This thread is statically created and has priority 0 (This is the highest possible)
void* main_prog(void *arg) {
  print("-- Entering main_prog() --\r\n");
  int ret;

  // Initialize Mailbox
  mailbox_init();

  // Initialize PushButton
  pb_init();

  // Initialize Game Screen
  game_screen_init();

  pthread_attr_init(&attr);

  // Priority 1 for thread 0
  sched_par.sched_priority = 1;
  pthread_attr_setschedparam(&attr, &sched_par);
  // Start thread 0
  ret = pthread_create (&tid0, &attr, (void*)thread_func_0, NULL);
  if (ret != 0) {
    xil_printf ("-- ERROR (%d) launching thread_func_0...\r\n", ret);
  } else {
    xil_printf ("Thread 0 launched with ID %d \r\n",tid0);
  }

  // Priority 2 for thread 1
  sched_par.sched_priority = 2;
  pthread_attr_setschedparam(&attr, &sched_par);
  // Start thread 1
  ret = pthread_create (&tid1, &attr, (void*)thread_func_1, NULL);
  if (ret != 0) {
    xil_printf ("-- ERROR (%d) launching thread_func_1...\r\n", ret);
  } else {
    xil_printf ("Thread 1 launched with ID %d \r\n",tid1);
  }

  while(1)
  {
    sleep(1000); //to kill time
  }
}

int TftInit(u32 TftDeviceId) {
  int Status;
  XTft_Config *TftConfigPtr;
  unsigned int *col;
  unsigned char c;

  print("Begin initializing TFT\n\r");
  /*
  * Get address of the XTft_Config structure for the given device id.
  */
  TftConfigPtr = XTft_LookupConfig(TftDeviceId);
  if (TftConfigPtr == (XTft_Config *)NULL) {
    return XST_FAILURE;
  }

  /*
  * Initialize all the TftInstance members and fills the screen with
  * default background color.
  */
  Status = XTft_CfgInitialize(&TftInstance, TftConfigPtr, TftConfigPtr->BaseAddress);
  if (Status != XST_SUCCESS) {
    return XST_FAILURE;
  }

  /*
  * Wait till Vsync(Video address latch) status bit is set before writing
  * the frame address into the Address Register. This ensures that the
  * current frame has been displayed and we can display a new frame of
  * data. Checking the Vsync state ensures that there is no data flicker
  * when displaying frames in real time though there is some delay due to
  * polling.
  */
  while (XTft_GetVsyncStatus(&TftInstance) !=
  XTFT_IESR_VADDRLATCH_STATUS_MASK);

  /*
  * Change the Video Memory Base Address from default value to
  * a valid Memory Address and clear the screen, setting it to white.
  */
  XTft_SetFrameBaseAddr(&TftInstance, TFT_FRAME_ADDR);
  XTft_SetColor(&TftInstance, WHITE, WHITE);
  XTft_ClearScreen(&TftInstance);

  print("Finish initializing TFT\n\r");
  return Status;
}

int main (void) {
  print("-- Entering main() --\r\n");

  // Initialize screen
  int Status;
  Status = TftInit(TFT_DEVICE_ID);
  if (Status != XST_SUCCESS) {
    return XST_FAILURE;
  }

  // Initialize Xilkernel
  xilkernel_init();

  // Add main_prog as the static thread that will be invoked by Xilkernel
  xmk_add_static_thread(main_prog, 0);

  // Start Xilkernel
  xilkernel_start();

  // Control does not reach here
  return 0;
}

/* ================================================*
* Author         : Lee-Hong Lau and Justin Yeo	   *
* Date           : 22/03/2017					   *
* Version        : V1.0						   *
* License        : MIT 						   *
* ================================================*
*/
#include "sys/init.h"
#include "xmk.h"
#include "xmbox.h"
#include "xmutex.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <time.h>
#include <math.h>
#include <xparameters.h>
#include <pthread.h>
#include <errno.h>
#include <sys/msg.h>
#include <sys/ipc.h>
#include <sys/timer.h>

#define DISPLAY_COLUMNS         640
#define DISPLAY_ROWS            480
#define RECT_WIDTH 	            40
#define RECT_HEIGHT				15
#define RECT_LENGTH             240
#define RECT_GAP				10
#define X1						50
#define X2						X1+RECT_WIDTH+RECT_GAP
#define X3						X2+RECT_WIDTH+RECT_GAP
#define X4						X3+RECT_WIDTH+RECT_GAP
#define Y1						50
#define INIT_BALL_X				288
#define INIT_BALL_Y 			397
#define INIT_BALL_SPEED_X       10
#define INIT_BALL_SPEED_Y       10
#define MAX_BALL_SPEED          40
#define MIN_BALL_SPEED          2
#define BALL_INIT_DIR 	        0
#define BALL_INIT_SPEED         250
#define BALL_MIN_SPEED			50
#define BALL_MAX_SPEED			1000
#define BALL_RADIUS				7

#define MSG_COLUMN	            1
#define MSG_BALL				2

/*	Mailbox Declaration	*/
#define MY_CPU_ID				XPAR_CPU_ID
#define MBOX_DEVICE_ID			XPAR_MBOX_0_DEVICE_ID
static XMbox Mbox;			//Instance of the Mailbox driver

/*	MUTEX ID PARAMETER for HW Mutex	*/
#define MUTEX_DEVICE_ID			XPAR_MUTEX_0_IF_1_DEVICE_ID
#define MUTEX_NUM 			        0

XMutex Mutex;

struct msg {
  int id, old_gold_col, new_gold_col, ball_x_pos, ball_y_pos, total_score;
};

struct msg_game_status {
  int status;
};

struct collision_msg {
  int brick_col, brick_row;
};

typedef struct {
  int dir,x,y;
  uint32_t col;
} ball_msg;

pthread_attr_t attr;
struct sched_param sched_par;
pthread_t mailbox_controller, ball, col1, col2, col3, col4, col5, col6, col7, col8, col9, col10, scoreboard;
pthread_mutex_t mutex, uart_mutex;

// declare the semaphore
sem_t sem;

volatile int ball_dir = 0; 								// 0: up, 180: down
volatile int ballspeed = 50;
volatile int new_ball_x = INIT_BALL_X;
volatile int new_ball_y = INIT_BALL_Y;
volatile int ballspeed_x = INIT_BALL_SPEED_X;
volatile int ballspeed_y = INIT_BALL_SPEED_Y;
volatile int oldgold_id = 0;
volatile int newgold_id = 0;

volatile int total_score = 0;
volatile int tenpt_counter = 0;
volatile int change_golden_status = 1;					// initialize the first 2 golden columns
volatile int twocol_counter = 2;						// track the number of golden columns we are changing

volatile int game_status = -1;							// [-1: In-progress], [0: Lose], [1: Win]
volatile int columns_destroyed = 0;						// tracks the number of columns the user has destroyed
volatile int ball_beyond_y = 0;						// flag to check if ball has gone beyond lower screen limit of y (ie. lose)


/*  -----------------------------------------
*	Function that checks for brick collisions
*	-----------------------------------------
*/
int check_collision(int col_num) {

  int circle_distance_x, circle_distance_y, corner_distance_sq;
  int i=0;

  while (i<8) {
    circle_distance_x = abs(new_ball_x - (65+(45*(col_num-1))));
    circle_distance_y = abs(new_ball_y - (65+(20*i)));											// loop down y-axis of the bricks

    if (circle_distance_x <= (RECT_WIDTH/2)) {
    	return i;
    } else if (circle_distance_y <= (RECT_HEIGHT/2)) {
    	return i;
    } else {
      corner_distance_sq = pow((circle_distance_x - RECT_WIDTH/2),2) + pow((circle_distance_y - RECT_HEIGHT/2),2);
      if (corner_distance_sq <= (BALL_RADIUS*BALL_RADIUS)) {return -i;}			// return negative of row # indicating a corner strike
      else
      i++;
    }
  }
  return 0;
}

/* ----------------------------------------------------------
* Function that checks if player has incremented score by 10
* ----------------------------------------------------------
*/
void check_tenpt(void) {
  if (tenpt_counter >= 10) {
    tenpt_counter -= 10;							//	remove 10 from the score
    change_golden_status = 1;						//	set golden flag to allow brick threads to compete for turning gold
    // TODO: include increase speed function (25px/sec)
  }
  else
  return;
}

/* ----------------------------------------------------
* Functions to send data structs over to MB1 via mailbox
* ----------------------------------------------------
*/
void send(int id, int old_gold_col, int new_gold_col, int ball_x_pos, int ball_y_pos, int total_score) {

  struct msg send_msg;
  send_msg.id = id;
  send_msg.old_gold_col = old_gold_col;
  send_msg.new_gold_col = new_gold_col;
  send_msg.ball_x_pos = ball_x_pos;
  send_msg.ball_y_pos = ball_y_pos;
  send_msg.total_score = total_score;

  XMbox_WriteBlocking(&Mbox, &send_msg, 24);
}

void send_game_status(int status) {

  struct msg_game_status send_msg;
  send_msg.status = status;

  XMbox_WriteBlocking(&Mbox, &send_msg, 4);
}

/* ------------------------------------------------------------------
* Function to compete for counting semaphore, and attain gold status
* ------------------------------------------------------------------
*/
void compete_gold(int ID) {
  int randomizer = rand() % 3;

  if ((randomizer == 1) && (twocol_counter > 0)) {
    sem_wait(&sem);																// Decrement the value of semaphore s by 1 (use up 1 sema resource)

    pthread_mutex_lock (&mutex);
    oldgold_id=newgold_id;
    newgold_id=ID;
    pthread_mutex_unlock (&mutex);

    // FIXME: What's the point of this semaphore then?
    // Also, sleeping in compete gold means the whole brick thread will sleep.
    sleep(1000);
    sem_post(&sem);
    twocol_counter--;
  }

  if (twocol_counter == 0) {													// we have changed two new columns to golden
    change_golden_status = 0;													// we no longer need and allow brick threads to compete for gold status
    twocol_counter = 2;															// reset twocol_counter for next iteration
  }
  sleep(100);
}

void* thread_mb_controller () {
  while(1) {
    send(1, oldgold_id, newgold_id, new_ball_x, new_ball_y, total_score);		// send mailbox to MB0

    // FIXME: Is there a need to send game status separately from the above send?
    // We also need to send bricks destroyed
    if(columns_destroyed == 10) {												// check if all 10 brick columns are destroyed
      game_status = 1;
      send_game_status(game_status);											// send mailbox to MB0 to update winning UI
    }
    if (ball_beyond_y) {
      game_status = 0;
      send_game_status(game_status);
    }
    sleep(40);
  }
}

void* thread_ball () {
  int i = 0;
  int msgid;
  struct collision_msg receive_msg;
  while(1) {
    int hit_angle_plus=0;
    int hit_angle_minus=0;
    int hit_speed_plus=0;
    int hit_speed_minus=0;
    int hit_zone_bottom=0;
    int hit_zone_top=0;
    int hit_zone_leftright=0;
    int hit_brick_topbottom=0;
    int hit_brick_leftright=0;
    int hit_brick_corner=0;

    if (hit_angle_plus) {
      // increase by 15deg up to 75 (0 is up, 75 is 165 with respect to bar)
    }
    if (hit_angle_minus) {
      // decrease by 15deg up down to 285
    }
    if (hit_speed_plus) {
      ballspeed = (ballspeed > BALL_MAX_SPEED-100) ? BALL_MAX_SPEED : ballspeed + 100;
      // TODO: update ball angle
    }
    if (hit_speed_minus) {
      ballspeed = (ballspeed < BALL_MIN_SPEED+100) ? BALL_MIN_SPEED : ballspeed - 100;
    }

    // TODO: Update ballspeed_x/y according to speed/angle?
    // TODO: How to read where it hit brick?

    if (hit_zone_top || hit_brick_topbottom) {
      ballspeed_y = -ballspeed_y;
    }
    if (hit_zone_leftright || hit_brick_leftright) {
      ballspeed_x = -ballspeed_x;
    }

    if (hit_brick_corner) {
      int temp = ballspeed_x;
      ballspeed_x = ballspeed_y;
      ballspeed_y = temp;
    }

    // Lose condition
    if (hit_zone_bottom) {
      // TODO: Lose game
      pthread_exit(0);
    }

    new_ball_x += ballspeed_x;
    new_ball_y += ballspeed_y;

    if (new_ball_y >= 413) {				// check if ball's y-pos is below the set limit
      ball_beyond_y = 1;
      pthread_exit(0);						  // no longer update the ball positioning
    }

    for (i=0; i<10; i++) {
      msgid = msgget(i, IPC_CREAT);
      msgrcv(msgid, &receive_msg, 8, 0, 0);
      // [receive_msg.brick_col: the brick column], [receive_msg.brick_row: the brick row (-ve means corner strike on that brick)]
      //TODO: ball deflection based on receive_msg
    }
    sleep(40);
  }
}

void* thread_brick_col_1 () {
  // FIXME: If the contents of these functions are mostly similar, you ought to pull them out to a function.
  // Now to fix your errors you need to modify them everywhere
  int brick_counter = 8;
  int collision_row = 0;
  int id = 1;
  while(1) {
    if(change_golden_status)
    compete_gold(0);

    if(brick_counter == 0) {
      columns_destroyed++;
      pthread_exit(0);
    }

    collision_row = check_collision(id);					// check collision for col number 1

    if(collision_row != 0) {								// [+ve: surface strike], [-ve: corner strike]
      struct collision_msg send_collision_msg;
      int msgid;
      send_collision_msg.brick_col = id;
      send_collision_msg.brick_row = collision_row;
      msgid = msgget(id, IPC_CREAT ) ;
      msgsnd(msgid, &send_collision_msg, 8, 0);
      // FIXME: I don't think writeBlocking is a good idea.
      // There'll be a deadlock because control does not hand over to thread_ball
      XMbox_WriteBlocking(&Mbox, &send_collision_msg, 8);	// mailbox to remove the display bricks
      if (newgold_id == id) || (oldgold_id == id)
      total_score +=2;
      else
      total_score +=1;
      brick_counter--;									// reduce number of bricks "alive" in this column
    }
  }
}

void* thread_brick_col_2 () {
  int brick_counter = 8;
  int collision_row = 0;
  int id = 2;
  while(1) {
    if(change_golden_status)
    compete_gold(1);

    if(brick_counter == 0) {
      columns_destroyed++;
      pthread_exit(0);
    }

    collision_row = check_collision(id);					// check collision for col number 2

    if(collision_row != 0) {								// [+ve: surface strike], [-ve: corner strike]
      struct collision_msg send_collision_msg;
      int msgid;
      send_collision_msg.brick_col = id;
      send_collision_msg.brick_row = collision_row;
      msgid = msgget(id, IPC_CREAT ) ;
      msgsnd(msgid, &send_collision_msg, 8, 0);
      XMbox_WriteBlocking(&Mbox, &send_collision_msg, 8);	// mailbox to remove the display bricks
      if (newgold_id == id) || (oldgold_id == id)
      total_score +=2;
      else
      total_score +=1;
      brick_counter--;									// reduce number of bricks "alive" in this column
    }
  }
}

void* thread_brick_col_3 () {
  int brick_counter = 8;
  int collision_row = 0;
  int id = 3;
  while(1) {
    if(change_golden_status)
    compete_gold(2);

    if(brick_counter == 0) {
      columns_destroyed++;
      pthread_exit(0);
    }

    collision_row = check_collision(id);					// check collision for col number 3

    if(collision_row != 0) {								// [+ve: surface strike], [-ve: corner strike]
      struct collision_msg send_collision_msg;
      int msgid;
      send_collision_msg.brick_col = id;
      send_collision_msg.brick_row = collision_row;
      msgid = msgget(id, IPC_CREAT ) ;
      msgsnd(msgid, &send_collision_msg, 8, 0);
      XMbox_WriteBlocking(&Mbox, &send_collision_msg, 8);	// mailbox to remove the display bricks
      if (newgold_id == id) || (oldgold_id == id)
      total_score +=2;
      else
      total_score +=1;
      brick_counter--;									// reduce number of bricks "alive" in this column
    }
  }
}

void* thread_brick_col_4 () {
  int brick_counter = 8;
  int collision_row = 0;
  int id = 4;
  while(1) {
    if(change_golden_status)
    compete_gold(3);

    if(brick_counter == 0) {
      columns_destroyed++;
      pthread_exit(0);
    }

    collision_row = check_collision(id);					// check collision for col number 4

    if(collision_row != 0) {								// [+ve: surface strike], [-ve: corner strike]
      struct collision_msg send_collision_msg;
      int msgid;
      send_collision_msg.brick_col = id;
      send_collision_msg.brick_row = collision_row;
      msgid = msgget(id, IPC_CREAT ) ;
      msgsnd(msgid, &send_collision_msg, 8, 0);
      XMbox_WriteBlocking(&Mbox, &send_collision_msg, 8);	// mailbox to remove the display bricks
      if (newgold_id == id) || (oldgold_id == id)
      total_score +=2;
      else
      total_score +=1;
      brick_counter--;									// reduce number of bricks "alive" in this column
    }
  }
}

void* thread_brick_col_5 () {
  int brick_counter = 8;
  int collision_row = 0;
  int id = 5;
  while(1) {
    if(change_golden_status)
    compete_gold(4);

    if(brick_counter == 0) {
      columns_destroyed++;
      pthread_exit(0);
    }

    collision_row = check_collision(id);					// check collision for col number 5

    if(collision_row != 0) {								// [+ve: surface strike], [-ve: corner strike]
      struct collision_msg send_collision_msg;
      int msgid;
      send_collision_msg.brick_col = id;
      send_collision_msg.brick_row = collision_row;
      msgid = msgget(id, IPC_CREAT ) ;
      msgsnd(msgid, &send_collision_msg, 8, 0);
      XMbox_WriteBlocking(&Mbox, &send_collision_msg, 8);	// mailbox to remove the display bricks
      if (newgold_id == id) || (oldgold_id == id)
      total_score +=2;
      else
      total_score +=1;
      brick_counter--;									// reduce number of bricks "alive" in this column
    }
  }
}

void* thread_brick_col_6 () {
  int brick_counter = 8;
  int collision_row = 0;
  int id = 6;
  while(1) {
    if(change_golden_status)
    compete_gold(5);

    if(brick_counter == 0) {
      columns_destroyed++;
      pthread_exit(0);
    }

    collision_row = check_collision(id);					// check collision for col number 6

    if(collision_row != 0) {								// [+ve: surface strike], [-ve: corner strike]
      struct collision_msg send_collision_msg;
      int msgid;
      send_collision_msg.brick_col = id;
      send_collision_msg.brick_row = collision_row;
      msgid = msgget(id, IPC_CREAT ) ;
      msgsnd(msgid, &send_collision_msg, 8, 0);
      XMbox_WriteBlocking(&Mbox, &send_collision_msg, 8);	// mailbox to remove the display bricks
      if (newgold_id == id) || (oldgold_id == id)
      total_score +=2;
      else
      total_score +=1;
      brick_counter--;									// reduce number of bricks "alive" in this column
    }
  }
}

void* thread_brick_col_7 () {
  int brick_counter = 8;
  int collision_row = 0;
  int id = 7;
  while(1) {
    if(change_golden_status)
    compete_gold(6);

    if(brick_counter == 0) {
      columns_destroyed++;
      pthread_exit(0);
    }

    collision_row = check_collision(id);					// check collision for col number 7

    if(collision_row != 0) {								// [+ve: surface strike], [-ve: corner strike]
      struct collision_msg send_collision_msg;
      int msgid;
      send_collision_msg.brick_col = id;
      send_collision_msg.brick_row = collision_row;
      msgid = msgget(id, IPC_CREAT ) ;
      msgsnd(msgid, &send_collision_msg, 8, 0);
      XMbox_WriteBlocking(&Mbox, &send_collision_msg, 8);	// mailbox to remove the display bricks
      if (newgold_id == id) || (oldgold_id == id)
      total_score +=2;
      else
      total_score +=1;
      brick_counter--;									// reduce number of bricks "alive" in this column
    }
  }
}

void* thread_brick_col_8 () {
  int brick_counter = 8;
  int collision_row = 0;
  int id = 8;
  while(1) {
    if(change_golden_status)
    compete_gold(7);

    if(brick_counter == 0) {
      columns_destroyed++;
      pthread_exit(0);
    }

    collision_row = check_collision(id);					// check collision for col number 8

    if(collision_row != 0) {								// [+ve: surface strike], [-ve: corner strike]
      struct collision_msg send_collision_msg;
      int msgid;
      send_collision_msg.brick_col = id;
      send_collision_msg.brick_row = collision_row;
      msgid = msgget(id, IPC_CREAT ) ;
      msgsnd(msgid, &send_collision_msg, 8, 0);
      XMbox_WriteBlocking(&Mbox, &send_collision_msg, 8);	// mailbox to remove the display bricks
      if (newgold_id == id) || (oldgold_id == id)
      total_score +=2;
      else
      total_score +=1;
      brick_counter--;									// reduce number of bricks "alive" in this column
    }
  }
}

void* thread_brick_col_9 () {
  int brick_counter = 8;
  int collision_row = 0;
  int id = 9;
  while(1) {
    if(change_golden_status)
    compete_gold(8);

    if(brick_counter == 0) {
      columns_destroyed++;
      pthread_exit(0);
    }

    collision_row = check_collision(id);					// check collision for col number 9

    if(collision_row != 0) {								// [+ve: surface strike], [-ve: corner strike]
      struct collision_msg send_collision_msg;
      int msgid;
      send_collision_msg.brick_col = id;
      send_collision_msg.brick_row = collision_row;
      msgid = msgget(id, IPC_CREAT ) ;
      msgsnd(msgid, &send_collision_msg, 8, 0);
      XMbox_WriteBlocking(&Mbox, &send_collision_msg, 8);	// mailbox to remove the display bricks
      if (newgold_id == id) || (oldgold_id == id)
      total_score +=2;
      else
      total_score +=1;
      brick_counter--;									// reduce number of bricks "alive" in this column
    }
  }
}

void* thread_brick_col_10 () {
  int brick_counter = 8;
  int collision_row = 0;
  int id = 10;
  while(1) {
    if(change_golden_status)
    compete_gold(9);

    if(brick_counter == 0) {
      columns_destroyed++;
      pthread_exit(0);
    }

    collision_row = check_collision(id);					// check collision for col number 10

    if(collision_row != 0) {								// [+ve: surface strike], [-ve: corner strike]
      struct collision_msg send_collision_msg;
      int msgid;
      send_collision_msg.brick_col = id;
      send_collision_msg.brick_row = collision_row;
      msgid = msgget(id, IPC_CREAT ) ;
      msgsnd(msgid, &send_collision_msg, 8, 0);
      XMbox_WriteBlocking(&Mbox, &send_collision_msg, 8);	// mailbox to remove the display bricks
      if (newgold_id == id) || (oldgold_id == id)
      total_score +=2;
      else
      total_score +=1;
      brick_counter--;									// reduce number of bricks "alive" in this column
    }
  }
}


void* thread_scoreboard () {
  while(1) {
    // Increase ball speed by 25fps for every 10 points gained
    if ((ballspeed_x != MAX_BALL_SPEED) && (ballspeed_y != MAX_BALL_SPEED)) {
      //      ballspeed_x = INIT_BALL_SPEED_X + (total_score/10) + bar_region ;
      //      ballspeed_y = INIT_BALL_SPEED_Y + (total_score/10) + bar_region ;
    }
  }
}

/**
*  Main - Inititialization for Semaphore, HW+SW Mutex, GPIOs, Mailbox and Threads
*/

int main_prog(void) {   // This thread is statically created (as configured in the kernel configuration) and has priority 0 (This is the highest possible)

  int ret;
  int Status;
  XMutex_Config *MutexConfigPtr;
  XMbox_Config *ConfigPtr;

  // Initialize semaphore for resource competion
  if( sem_init(&sem, 1, 2) < 0 ) {
    print("Error while initializing semaphore sem.\r\n");
  }

  // Init SW Mutex
  ret = pthread_mutex_init (&uart_mutex, NULL);
  if (ret != 0) {
    xil_printf ("-- ERROR (%d) init uart_mutex...\r\n", ret);
  }
  print("--Initialized SW Mutex-- uB0 \r\n");

  // Init HW Mutex
  MutexConfigPtr = XMutex_LookupConfig(MUTEX_DEVICE_ID);
  if (MutexConfigPtr == (XMutex_Config *)NULL){
    ("B1-- ERROR  init HW mutex...\r\n");
  }
  Status = XMutex_CfgInitialize(&Mutex, MutexConfigPtr, MutexConfigPtr->BaseAddress);
  if (Status != XST_SUCCESS){
    xil_printf ("B1-- ERROR  init HW mutex...\r\n");
  }

  print("-- Entering main_prog() uB0 RECEIVER--\r\n");

  // Configure and init mailbox
  ConfigPtr = XMbox_LookupConfig(MBOX_DEVICE_ID);
  if (ConfigPtr == (XMbox_Config *)NULL) {
    print("-- Error configuring Mbox uB0 receiver--\r\n");
    return XST_FAILURE;
  }

  Status = XMbox_CfgInitialize(&Mbox, ConfigPtr, ConfigPtr->BaseAddress);
  if (Status != XST_SUCCESS) {
    print("-- Error initializing Mbox uB0 receiver--\r\n");
    return XST_FAILURE;
  }


  /** Initialize Threads
  * -----------------------------------------------------------------------------------
  * thread_mb_controller  (highest priority)  : Mailbox Controller to pipe data to MB1
  * thread_ball                               : Ball
  * thread_brick_col_1 ~ thread_brick_col_10  : Brick Columns
  * thread_scoreboard     (lowest priority)   : Scoreboard
  * -----------------------------------------------------------------------------------
  */
  pthread_attr_init (&attr);

  // Priority 1 for thread_mb_controller
  sched_par.sched_priority = 1;
  pthread_attr_setschedparam(&attr,&sched_par);
  //start thread_mb_controller
  ret = pthread_create (&mailbox_controller, &attr, (void*)thread_mb_controller, NULL);
  if (ret != 0) {
    xil_printf ("-- ERROR (%d) launching thread_mb_controller...\r\n", ret);
  }
  else {
    xil_printf ("Mailbox Controller Thread launched with ID %d \r\n",mailbox_controller);
  }

  // Priority 2 for thread_ball
  sched_par.sched_priority = 2;
  pthread_attr_setschedparam(&attr,&sched_par);
  //start thread_ball
  ret = pthread_create (&ball, &attr, (void*)thread_ball, NULL);
  if (ret != 0) {
    xil_printf ("-- ERROR (%d) launching thread_ball...\r\n", ret);
  }
  else {
    xil_printf ("Ball Thread launched with ID %d \r\n",ball);
  }

  // Priority 3 for thread_brick_col_1
  sched_par.sched_priority = 3;
  pthread_attr_setschedparam(&attr,&sched_par);
  //start thread_brick_col_1
  ret = pthread_create (&col1, &attr, (void*)thread_brick_col_1, NULL);
  if (ret != 0) {
    xil_printf ("-- ERROR (%d) launching thread_brick_col_1...\r\n", ret);
  }
  else {
    xil_printf ("Brick Column 1 Thread launched with ID %d \r\n",col1);
  }

  // Priority 4 for thread_brick_col_2
  sched_par.sched_priority = 3;
  pthread_attr_setschedparam(&attr,&sched_par);
  //start thread_brick_col_2
  ret = pthread_create (&col2, &attr, (void*)thread_brick_col_2, NULL);
  if (ret != 0) {
    xil_printf ("-- ERROR (%d) launching thread_brick_col_2...\r\n", ret);
  }
  else {
    xil_printf ("Brick Column 2 Thread launched with ID %d \r\n",col2);
  }

  // Priority 5 for thread_brick_col_3
  sched_par.sched_priority = 3;
  pthread_attr_setschedparam(&attr,&sched_par);
  //start thread 5
  ret = pthread_create (&col3, &attr, (void*)thread_brick_col_3, NULL);
  if (ret != 0) {
    xil_printf ("-- ERROR (%d) launching thread_brick_col_3...\r\n", ret);
  }
  else {
    xil_printf ("Brick Column 3 Thread launched with ID %d \r\n",col3);
  }

  // Priority 6 for thread_brick_col_4
  sched_par.sched_priority = 3;
  pthread_attr_setschedparam(&attr,&sched_par);
  //start thread_brick_col_4
  ret = pthread_create (&col4, &attr, (void*)thread_brick_col_4, NULL);
  if (ret != 0) {
    xil_printf ("-- ERROR (%d) launching thread_brick_col_4...\r\n", ret);
  }
  else {
    xil_printf ("Brick Column 4 launched with ID %d \r\n",col4);
  }

  // Priority 7 for thread_brick_col_5
  sched_par.sched_priority = 3;
  pthread_attr_setschedparam(&attr,&sched_par);
  //start thread_brick_col_5
  ret = pthread_create (&col5, &attr, (void*)thread_brick_col_5, NULL);
  if (ret != 0) {
    xil_printf ("-- ERROR (%d) launching thread_brick_col_5...\r\n", ret);
  }
  else {
    xil_printf ("Brick Column 5 launched with ID %d \r\n",col5);
  }

  // Priority 8 for thread_brick_col_6
  sched_par.sched_priority = 3;
  pthread_attr_setschedparam(&attr,&sched_par);
  //start thread_brick_col_6
  ret = pthread_create (&col6, &attr, (void*)thread_brick_col_6, NULL);
  if (ret != 0) {
    xil_printf ("-- ERROR (%d) launching thread_brick_col_6...\r\n", ret);
  }
  else {
    xil_printf ("Brick Column 6 launched with ID %d \r\n",col6);
  }

  // Priority 9 for thread_brick_col_7
  sched_par.sched_priority = 3;
  pthread_attr_setschedparam(&attr,&sched_par);
  //start thread_brick_col_7
  ret = pthread_create (&col7, &attr, (void*)thread_brick_col_7, NULL);
  if (ret != 0) {
    xil_printf ("-- ERROR (%d) launching thread_brick_col_7...\r\n", ret);
  }
  else {
    xil_printf ("Brick Column 7 launched with ID %d \r\n",col7);
  }

  // Priority 10 for thread_brick_col_8
  sched_par.sched_priority = 3;
  pthread_attr_setschedparam(&attr,&sched_par);
  //start thread_brick_col_8
  ret = pthread_create (&col8, &attr, (void*)thread_brick_col_8, NULL);
  if (ret != 0) {
    xil_printf ("-- ERROR (%d) launching thread_brick_col_8...\r\n", ret);
  }
  else {
    xil_printf ("Brick Column 8 launched with ID %d \r\n",col8);
  }

  // Priority 11 for thread_brick_col_9
  sched_par.sched_priority = 3;
  pthread_attr_setschedparam(&attr,&sched_par);
  //start thread_brick_col_9
  ret = pthread_create (&col9, &attr, (void*)thread_brick_col_9, NULL);
  if (ret != 0) {
    xil_printf ("-- ERROR (%d) launching thread_brick_col_9...\r\n", ret);
  }
  else {
    xil_printf ("Brick Column 9 launched with ID %d \r\n",col9);
  }

  // Priority 12 for thread_brick_col_10
  sched_par.sched_priority = 3;
  pthread_attr_setschedparam(&attr,&sched_par);
  //start thread_brick_col_10
  ret = pthread_create (&col10, &attr, (void*)thread_brick_col_10, NULL);
  if (ret != 0) {
    xil_printf ("-- ERROR (%d) launching thread_brick_col_10...\r\n", ret);
  }
  else {
    xil_printf ("Brick Column 10 launched with ID %d \r\n",col10);
  }

  // Priority 13 for thread_scoreboard
  sched_par.sched_priority = 4;
  pthread_attr_setschedparam(&attr,&sched_par);
  //start thread_scoreboard
  ret = pthread_create (&scoreboard, &attr, (void*)thread_scoreboard, NULL);
  if (ret != 0) {
    xil_printf ("-- ERROR (%d) launching thread_scoreboard...\r\n", ret);
  }
  else {
    xil_printf ("Scoreboard Thread launched with ID %d \r\n",scoreboard);
  }

  return 0;
}

int main (void) {
  print("-- Entering main() uB0 RECEIVER--\r\n");
  xilkernel_init();
  xmk_add_static_thread(main_prog,0);
  xilkernel_start();
  //Start Xilkernel
  xilkernel_main ();

  //Control does not reach here
  return 0;
}

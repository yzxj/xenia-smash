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

/*	Mailbox Declaration	*/
#define MY_CPU_ID				XPAR_CPU_ID
#define MBOX_DEVICE_ID			XPAR_MBOX_0_DEVICE_ID
static XMbox Mbox;			//Instance of the Mailbox driver

/*	MUTEX ID PARAMETER for HW Mutex	*/
#define MUTEX_DEVICE_ID			XPAR_MUTEX_0_IF_1_DEVICE_ID
#define MUTEX_NUM 			        0

XMutex Mutex;

#define MSG_MAX_DESTROYED		8
#define MSG_SIZE				7*4 + MSG_MAX_DESTROYED*2*4
struct msg {
  int old_gold_col, new_gold_col;
  int ball_x_pos, ball_y_pos;
  int game_won;
  int total_score;
  int destroyed_num;
  int destroyed_x[MSG_MAX_DESTROYED];
  int destroyed_y[MSG_MAX_DESTROYED];
};

struct collision_msg {
  int type;
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
sem_t sem_gold;
sem_t sem_bricks;

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

volatile int game_status = 0;							// [0: In-progress], [-1: Lose], [1: Win]
volatile int columns_destroyed = 0;						// tracks the number of columns the user has destroyed
volatile int ball_beyond_y = 0;						// flag to check if ball has gone beyond lower screen limit of y (ie. lose)

volatile int brick_destroyed[10][8];

volatile int destroyed_col[8];
volatile int destroyed_row[8];
volatile int destroyed_index = 0;

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
void send_data_to_mb0() {
  struct msg send_msg;
  send_msg.old_gold_col = oldgold_id;
  send_msg.new_gold_col = newgold_id;
  send_msg.ball_x_pos = new_ball_x;
  send_msg.ball_y_pos = new_ball_y;
  send_msg.total_score = total_score;

  XMbox_WriteBlocking(&Mbox, &send_msg, 24);
}

/* ------------------------------------------------------------------
* Function to compete for counting semaphore, and attain gold status
* ------------------------------------------------------------------
*/
void compete_gold(int ID) {
  int randomizer = rand() % 3;
  if (randomizer == 1 && sem_trywait(&sem_gold) == 0) {
    pthread_mutex_lock(&mutex);
    oldgold_id=newgold_id;
    newgold_id=ID;
    pthread_mutex_unlock(&mutex);
  }
}

void* thread_mb_controller () {
  while(1) {
	game_status = (columns_destroyed == 10);
	send_data_to_mb0();

	if (game_status) {
		pthread_exit(0);
	}
    sleep(40);
  }
}

void* thread_ball () {
  int i = 0;
  int msgid;
  struct collision_msg receive_msg;
  while(1) {
    int hit_angle_plus	=0;
    int hit_angle_minus	=0;
    int hit_speed_plus	=0;
    int hit_speed_minus	=0;
    int hit_zone_bottom	=0;
    int hit_zone_top	=0;
    int hit_zone_leftright	=0;
    int hit_brick_topbottom	=0;
    int hit_brick_leftright	=0;
    int hit_brick_corner	=0;

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

    // Brick-related should be separate. Need MQ loop
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

// Returns 1 if collided, 0 if not
// Saves collision type: 0: nothing. 1: top/bottom. 2: left/right. 3-6: corners, clockwise from top right
int collided(int col, int row, int *collision_type) {
	double circle_distance_x, circle_distance_y, corner_distance_sq;
	double rect_x = 65 + col*(BRICK_GAP+BRICK_WIDTH)  + BRICK_WIDTH/2.0;
	double rect_y = 65 + col*(BRICK_GAP+BRICK_HEIGHT) + BRICK_HEIGHT/2.0;

	circle_distance_x = fabs(new_ball_x - rect_x);
	circle_distance_y = fabs(new_ball_y - rect_y);
	*collision_type = 0;

	// Easy case: Ball too far
	if (circle_distance_x > (BRICK_WIDTH/2.0 + BALL_RADIUS) || circle_distance_y > (BRICK_HEIGHT/2.0 + BALL_RADIUS)) {
		return 0;
	}

	// Ball within brick's width -> must be above or below
	if (circle_distance_x <= BRICK_WIDTH/2.0) {
		*collision_type = 1;
		return 1;
	}
	// Ball within brick's height -> must be left or right
	if (circle_distance_y <= BRICK_HEIGHT/2.0) {
		*collision_type = 2;
		return 1;
	}

	// Ball within radius of corner
	corner_distance_sq = pow((circle_distance_x - BRICK_WIDTH/2.0),2) + pow((circle_distance_y - BRICK_HEIGHT/2.0),2);
	if (corner_distance_sq <= pow(BALL_RADIUS, 2)) {
		if (new_ball_x > rect_x) {
			if (new_ball_y > rect_y) {
				*collision_type = 3;
			} else {
				*collision_type = 4;
			}
		} else {
			if (new_ball_y < rect_y) {
				*collision_type = 5;
			} else {
				*collision_type = 6;
			}
		}
		return 1;
	}

	// Ball slightly outside of corner
	return 0;
}

void inform_ball_thread(int *collision_type) {
    struct collision_msg send_collision_msg;
    int msgid;
    send_collision_msg.type = *collision_type;
    msgsnd(msgid, &send_collision_msg, 4, 0);
}

void update_score(int col) {
  if((newgold_id == col) || (oldgold_id == col)) {
    total_score +=2;
  } else {
    total_score +=1;
  }
}

void save_destroyed_brick(int row, col, index) {
    pthread_mutex_lock (&mutex);
    destroyed_col[index] = col;
    destroyed_row[index] = row;
    pthread_mutex_unlock (&mutex);
}

void check_collisions_send_updates(int col, int *bricks_left) {
	// for each brick
	// if not destroyed
	// check for collision
	// if so: update destroyed bricks, score, and send collision data
  int row, collision_type; 
  int index = 0;
  for(row=0; row<8; row++) {
	  // only check collision if the specific brick is alive
	  if(!brick_destroyed[col][row] && collided(col, row, &collision_type)){
		  brick_destroyed[col][row] = 1;
		  *bricks_left = *bricks_left - 1;
		  inform_ball_thread(collision_type);
		  update_score(col);
      save_destroyed_brick(row, col, index);
      index++;
    }
  }
}

void exit_brickthread_if_zero(int bricks_left) {
    if(bricks_left == 0) {
	  sem_wait(&sem_bricks);
	  columns_destroyed++;
	  sem_post(&sem_bricks);
	  pthread_exit(0);
	}
}

void brick_thread_logic(int col, int *bricks_left) {
	compete_gold(col);
	check_collisions_send_updates(col, bricks_left);
	exit_brickthread_if_zero(*bricks_left);
}

void* thread_brick_col_1 () {
  int bricks_left = 8;
  while(1) {
	  // NOTE: 0-based indexing.
	  brick_thread_logic(0, &bricks_left);
	  sleep(40);
  }
}

void* thread_brick_col_2 () {
  int bricks_left = 8;
  while(1) {
	  brick_thread_logic(1, &bricks_left);
	  sleep(40);
  }
}

void* thread_brick_col_3 () {
  int bricks_left = 8;
  while(1) {
	  brick_thread_logic(2, &bricks_left);
	  sleep(40);
  }
}

void* thread_brick_col_4 () {
  int bricks_left = 8;
  while(1) {
	  brick_thread_logic(3, &bricks_left);
	  sleep(40);
  }
}

void* thread_brick_col_5 () {
  int bricks_left = 8;
  while(1) {
	  brick_thread_logic(4, &bricks_left);
	  sleep(40);
  }
}

void* thread_brick_col_6 () {
  int bricks_left = 8;
  while(1) {
	  brick_thread_logic(5, &bricks_left);
	  sleep(40);
  }
}

void* thread_brick_col_7 () {
  int bricks_left = 8;
  while(1) {
	  brick_thread_logic(6, &bricks_left);
	  sleep(40);
  }
}

void* thread_brick_col_8 () {
  int bricks_left = 8;
  while(1) {
	  brick_thread_logic(7, &bricks_left);
	  sleep(40);
  }
}

void* thread_brick_col_9 () {
  int bricks_left = 8;
  while(1) {
	  brick_thread_logic(8, &bricks_left);
	  sleep(40);
  }
}

void* thread_brick_col_10 () {
  int bricks_left = 8;
  while(1) {
	  brick_thread_logic(9, &bricks_left);
	  sleep(40);
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
  if( sem_init(&sem_gold, 1, 2) < 0 ) {
    print("Error while initializing semaphore sem_gold.\r\n");
  }

  // Initialize semaphore for resource competion
  if( sem_init(&sem_bricks, 1, 1) < 0 ) {
    print("Error while initializing semaphore sem_bricks.\r\n");
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

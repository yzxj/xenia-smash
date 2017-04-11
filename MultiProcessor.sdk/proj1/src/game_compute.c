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

#define FPS					25

#define PI					3.14159
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
#define INIT_BALL_X				((PLAYAREA_LEFT + PLAYAREA_RIGHT)/2)
#define INIT_BALL_Y 			BAR_TOP_Y-30
#define BALL_INIT_DIR 	        0
#define BALL_INIT_SPEED         250
#define BALL_MIN_SPEED			50		// TODO: Use in code
#define BALL_MAX_SPEED			1000
#define BALL_RADIUS				7

/*	Mailbox Declaration	*/
#define MY_CPU_ID				XPAR_CPU_ID
#define MBOX_DEVICE_ID			XPAR_MBOX_0_DEVICE_ID
#define MBOX_DEVICE_ID_1      	XPAR_MBOX_1_DEVICE_ID
static XMbox Mbox;			   	//Instance of the Mailbox driver
static XMbox receive_box; 		//Instance of the receiver Mailbox driver

/*	MUTEX ID PARAMETER for HW Mutex	*/
#define MUTEX_DEVICE_ID			XPAR_MUTEX_0_IF_1_DEVICE_ID
#define MUTEX_NUM 			        0

XMutex Mutex;

#define MSG_MAX_DESTROYED		6
#define STATE_MSG_BYTES			8*4 + MSG_MAX_DESTROYED*2*4
#define BAR_MSG_BYTES       	8

typedef struct msg {
  int old_gold_col, new_gold_col;
  int ball_x_pos, ball_y_pos;
  int ballspeed;
  int game_won;
  int total_score;
  int destroyed_num;
  int destroyed_x0;
  int destroyed_y0;
  int destroyed_x1;
  int destroyed_y1;
  int destroyed_x2;
  int destroyed_y2;
  int destroyed_x3;
  int destroyed_y3;
  int destroyed_x4;
  int destroyed_y4;
  int destroyed_x5;
  int destroyed_y5;
};

struct collision_msg {
  int type;
};

typedef struct {
  int dir,x,y;
  uint32_t col;
} ball_msg;

typedef struct {
  int bar1_x, bar2_x;
} bar_msg;

pthread_attr_t attr;
struct sched_param sched_par;
pthread_t mailbox_controller, ball, col1, col2, col3, col4, col5, col6, col7, col8, col9, col10;
pthread_mutex_t mutex, uart_mutex;

// declare the semaphore
sem_t sem_gold;
sem_t sem_bricks;

volatile int ball_dir = 0; 								// 0: up, 180: down
volatile int ballspeed = 250;
volatile int new_ball_x = INIT_BALL_X;
volatile int new_ball_y = INIT_BALL_Y;
volatile int ballspeed_x = 0;
volatile int ballspeed_y = 250;
volatile int oldgold_id = 0;
volatile int newgold_id = 0;

volatile int total_score = 0;
volatile int tenpt_counter = 0;
volatile int twocol_counter = 2;						// track the number of golden columns we are changing

volatile int game_status = 0;							// [0: In-progress], [-1: Lose], [1: Win]
volatile int columns_destroyed = 0;						// tracks the number of columns the user has destroyed
volatile int ball_beyond_y = 0;						// flag to check if ball has gone beyond lower screen limit of y (ie. lose)

volatile int brick_destroyed[10][8];

volatile int destroyed_col[MSG_MAX_DESTROYED];
volatile int destroyed_row[MSG_MAX_DESTROYED];
volatile int destroyed_index = 0;

volatile int bar_x[2] = {(PLAYAREA_LEFT + PLAYAREA_RIGHT - BAR_WIDTH_TOTAL)/2, (PLAYAREA_LEFT + PLAYAREA_RIGHT - BAR_WIDTH_TOTAL)/2};
volatile int bar_y[2] = {BAR_TOP_Y, BAR_TOP_Y - 10};

/* ----------------------------------------------------------
* Function that checks if player has incremented score by 10
* ----------------------------------------------------------
*/
void check_tenpt(void) {
  if (total_score - tenpt_counter >= 10) {
	  xil_printf("in tenpt\r\n");
	// Keep track of the last 'tens' value
    tenpt_counter = total_score - total_score%10;
    // Release the resources for golden bricks
    sem_post(&sem_gold);
    sem_post(&sem_gold);
    // Increase ball speed
    // TODO: semaphore?
    ballspeed = (ballspeed+25>1000) ? 1000 : ballspeed+25;
  }
}

/* ----------------------------------------------------
* Functions to send data structs over to MB1 via mailbox
* ----------------------------------------------------
*/
void send_data_to_mb0() {
  struct msg send_msg;
	xil_printf("newgold: %d\r\n", newgold_id);
  send_msg.old_gold_col = oldgold_id;
  send_msg.new_gold_col = newgold_id;
  send_msg.ball_x_pos = new_ball_x;
  send_msg.ball_y_pos = new_ball_y;
  send_msg.ballspeed = ballspeed;
  send_msg.total_score = total_score;
  send_msg.destroyed_num = destroyed_index;
  send_msg.destroyed_x0 = destroyed_col[0];
  send_msg.destroyed_y0 = destroyed_row[0];
  send_msg.destroyed_x1 = destroyed_col[1];
  send_msg.destroyed_y1 = destroyed_row[1];
  send_msg.destroyed_x2 = destroyed_col[2];
  send_msg.destroyed_y2 = destroyed_row[2];
  send_msg.destroyed_x3 = destroyed_col[3];
  send_msg.destroyed_y3 = destroyed_row[3];
  send_msg.destroyed_x4 = destroyed_col[4];
  send_msg.destroyed_y4 = destroyed_row[4];
  send_msg.destroyed_x5 = destroyed_col[5];
  send_msg.destroyed_y5 = destroyed_row[5];

  XMbox_WriteBlocking(&Mbox, &send_msg, STATE_MSG_BYTES);
}

/* ------------------------------------------------------------------
* Function to compete for counting semaphore, and attain gold status
* ------------------------------------------------------------------
*/
void compete_gold(int ID) {
  int randomizer = rand() % 3;
  if (randomizer == 1 && sem_trywait(&sem_gold) == 0) {
    oldgold_id=newgold_id;
    newgold_id=ID;
  }
}

static void mailbox_receive(XMbox *MboxInstancePtr, bar_msg *bar_location_pointer) {
 	u32 received_bytes;
 	XMbox_Read(MboxInstancePtr, bar_location_pointer, 8, &received_bytes);
	  if (received_bytes == BAR_MSG_BYTES) {
		pthread_mutex_lock(&mutex);
		bar_x[0] = bar_location_pointer->bar1_x;
		bar_x[1] = bar_location_pointer->bar2_x;
		pthread_mutex_unlock(&mutex);
	  }
}

void* thread_mb_controller () {
 	bar_msg bar;
	while(1) {
		game_status = game_status || (columns_destroyed == 10);
		send_data_to_mb0();
		// TODO: send destroyed_index also
		// TODO: add ball2 to send
		destroyed_index = 0;
		if (game_status) {
		    xil_printf("You Lose\r\n");
			pthread_exit(0);
		}
		mailbox_receive(&Mbox, &bar);												// read bar locations from display processor (MB0)
		sleep(40);
	}
}

// Returns 1 if collided, 0 if not
// Saves collision type: 0: nothing. 1: top/bottom. 2: left/right. 3-6: corners, anti-clockwise from bottom right
int collided(int circle_x, int circle_y, double rect_x, double rect_y, int rect_width, int rect_height, int *collision_type) {
	double circle_distance_x, circle_distance_y, corner_distance_sq;

	circle_distance_x = fabs(circle_x - rect_x);
	circle_distance_y = fabs(circle_y - rect_y);
	*collision_type = 0;

	// Easy case: Circle too far
	if (circle_distance_x > (rect_width/2.0 + BALL_RADIUS) || circle_distance_y > (rect_height/2.0 + BALL_RADIUS)) {
		return 0;
	}

	// Circle within rect's width -> must be above or below
	if (circle_distance_x <= rect_width/2.0) {
		*collision_type = 1;
		return 1;
	}
	// Circle within rect's height -> must be left or right
	if (circle_distance_y <= rect_height/2.0) {
		*collision_type = 2;
		return 1;
	}

	// Circle within radius of corner
	corner_distance_sq = pow((circle_distance_x - rect_width/2.0),2) + pow((circle_distance_y - rect_height/2.0),2);
	if (corner_distance_sq <= pow(BALL_RADIUS, 2)) {
		if (circle_x > rect_x) {
			if (circle_y > rect_y) {
				*collision_type = 3;
			} else {
				*collision_type = 4;
			}
		} else {
			if (circle_y < rect_y) {
				*collision_type = 5;
			} else {
				*collision_type = 6;
			}
		}
		return 1;
	}

	// Circle slightly outside of corner
	return 0;
}
int brick_collided(int col, int row, int *collision_type) {
	double rect_x = 65 + col*(BRICK_GAP+BRICK_WIDTH)  + BRICK_WIDTH/2.0;
	double rect_y = 65 + row*(BRICK_GAP+BRICK_HEIGHT) + BRICK_HEIGHT/2.0;
	return collided(new_ball_x, new_ball_y, rect_x, rect_y, BRICK_WIDTH, BRICK_HEIGHT, collision_type);
}

int reflect_about(int mirror_dir, int ball_dir) {
	  xil_printf("angle %d hit mirror %d at %d\r\n", ball_dir, mirror_dir, xget_clock_ticks());
	return (360 - ball_dir + 2*mirror_dir)%360;
}

void* thread_ball () {
  int i = 0;
  int msgid;
  struct collision_msg receive_msg;
  while(1) {
	// Deal with brick deflection first
	msgid = msgget(0, IPC_CREAT);
	for (i=0; i<MSG_MAX_DESTROYED; i++) {
	  if (msgrcv(msgid, &receive_msg, 4, 0, IPC_NOWAIT) == 4) {
		  switch(receive_msg.type) {
		  case 1:
			  ball_dir = reflect_about(90, ball_dir);
		      break;
		  case 2:
		      ball_dir = reflect_about(0, ball_dir);
			  break;
		  case 3:
			  ball_dir = reflect_about(45, ball_dir);
			  break;
		  case 4:
			  ball_dir = reflect_about(315, ball_dir);
			  break;
		  case 5:
			  ball_dir = reflect_about(225, ball_dir);
			  break;
		  case 6:
			  ball_dir = reflect_about(135, ball_dir);
			  break;
		  }
	  }
	}
	// Deal with hitting the bar next (shouldn't happen if hitting bricks)
	// TODO: check bar 2.
	int bar_collision_type;
	int bar1_x = bar_x[0] + BAR_WIDTH_TOTAL/2;
	int bar1_y = bar_y[0] + BAR_HEIGHT/2.0;
	int ball_dist_x = new_ball_x - bar1_x;
	if (collided(new_ball_x, new_ball_y, bar1_x, bar1_y, BAR_WIDTH_TOTAL, BAR_HEIGHT, &bar_collision_type)) {
		switch(bar_collision_type) {
		case 1:
			ball_dir = reflect_about(90,ball_dir);
			if (ball_dist_x < -30) {
				ball_dir = (ball_dir-15<15) 	 ? 15 	: ball_dir-15;
			} else if (ball_dist_x > 30) {
				ball_dir = (ball_dir+15>165) 	 ? 165 	: ball_dir+15;
			} else if (ball_dist_x < -20) {
				ballspeed = (ballspeed-100<50) 	 ? 50 	: ballspeed-100;
			} else if (ball_dist_x > 20) {
				ballspeed = (ballspeed+100>1000) ? 1000 : ballspeed+100;
			}
			break;
		case 2:
		case 3:
		case 4:
		case 5:
		case 6:
			ball_dir += 180;
			break;
		}
	}

    // Deal with hitting zone edges last
    if (new_ball_y <= 79 && ball_dir <= 90 && ball_dir >= 270) {
    	ball_dir = reflect_about(90, ball_dir);
    }
    if (new_ball_x <= 79 || new_ball_x >= 501) {
    	ball_dir = reflect_about(0, ball_dir);
    }

    // Check for lose condition
	if (new_ball_y >= 415) {
	  game_status = -1;
	  pthread_exit(0);
	}

    // Update ballspeed_x/y according to speed/angle
	ballspeed_x = ballspeed * sin(ball_dir * PI / 180);
	ballspeed_y = ballspeed * - cos(ball_dir * PI / 180);

    // Update
    new_ball_x += ballspeed_x / FPS;
    new_ball_y += ballspeed_y / FPS;

    sleep(40);
  }
}

void inform_ball_thread(int collision_type) {
    struct collision_msg send_collision_msg;
    int msgid = msgget(0, IPC_CREAT);
    send_collision_msg.type = collision_type;
    msgsnd(msgid, &send_collision_msg, 4, 0);
}

void update_score(int col) {
  if((newgold_id == col) || (oldgold_id == col)) {
    total_score +=2;
  } else {
    total_score +=1;
  }
  check_tenpt();
}

void register_destroyed_brick(int row, int col) {
    pthread_mutex_lock(&mutex);
    destroyed_col[destroyed_index] = col;
    destroyed_row[destroyed_index] = row;
    destroyed_index++;
    pthread_mutex_unlock(&mutex);
}

void check_collisions_send_updates(int col, int *bricks_left) {
  int row, collision_type;

  for(row=0; row<8; row++) {
	  // only check collision if the specific brick is alive
	  // TODO: Check ball1 and 2
	  if(!brick_destroyed[col][row] && brick_collided(col, row, &collision_type)) {
		  xil_printf ("destroyed %d %d at %d\r\n", col, row, xget_clock_ticks());
		  brick_destroyed[col][row] = 1;
		  *bricks_left = *bricks_left - 1;
		  inform_ball_thread(collision_type);
		  update_score(col);
          register_destroyed_brick(row, col);
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

/**
*  Main - Inititialization for Semaphore, HW+SW Mutex, GPIOs, Mailbox and Threads
*/

int main_prog(void) {   // This thread is statically created (as configured in the kernel configuration) and has priority 0 (This is the highest possible)

  int ret;
  int Status;
  XMutex_Config *MutexConfigPtr;
  XMbox_Config *ConfigPtr;
  XMbox_Config *RxConfigPtr;

  // Initialize semaphore for resource competion
  if( sem_init(&sem_gold, 1, 2) < 0 ) {
    xil_printf("Error while initializing semaphore sem_gold.\r\n");
  }

  // Initialize semaphore for resource competion
  if( sem_init(&sem_bricks, 1, 1) < 0 ) {
    xil_printf("Error while initializing semaphore sem_bricks.\r\n");
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
	xil_printf ("B1-- ERROR  init HW mutex...\r\n");
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

  RxConfigPtr = XMbox_LookupConfig(MBOX_DEVICE_ID_1);
  if (RxConfigPtr == (XMbox_Config *)NULL) {
    print("-- Error configuring Mbox uB1 receiver--\r\n");
    return XST_FAILURE;
  }

  Status = XMbox_CfgInitialize(&receive_box, RxConfigPtr, RxConfigPtr->BaseAddress);
  if (Status != XST_SUCCESS) {
    print("-- Error initializing Mbox uB1 receiver--\r\n");
    return XST_FAILURE;
  }

  /** Initialize Threads
  * -----------------------------------------------------------------------------------
  * thread_mb_controller  (highest priority)  : Mailbox Controller to pipe data to MB1
  * thread_ball                               : Ball
  * thread_brick_col_1 ~ thread_brick_col_10  : Brick Columns
  * -----------------------------------------------------------------------------------
  */
  pthread_attr_init (&attr);

  // thread_mb_controller (Priority 1)
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

  // thread_ball (Priority 2)
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

  // thread_brick_col_1 (Brick cols share priority 3)
  sched_par.sched_priority = 3;
  pthread_attr_setschedparam(&attr,&sched_par);
  ret = pthread_create (&col1, &attr, (void*)thread_brick_col_1, NULL);

  // thread_brick_col_2
  ret |= pthread_create (&col2, &attr, (void*)thread_brick_col_2, NULL);

  // thread_brick_col_3
  ret |= pthread_create (&col3, &attr, (void*)thread_brick_col_3, NULL);

  // thread_brick_col_4
  ret |= pthread_create (&col4, &attr, (void*)thread_brick_col_4, NULL);

  // thread_brick_col_5
  ret |= pthread_create (&col5, &attr, (void*)thread_brick_col_5, NULL);

  // thread_brick_col_6
  ret |= pthread_create (&col6, &attr, (void*)thread_brick_col_6, NULL);

  // thread_brick_col_7
  ret |= pthread_create (&col7, &attr, (void*)thread_brick_col_7, NULL);

  // thread_brick_col_8
  ret |= pthread_create (&col8, &attr, (void*)thread_brick_col_8, NULL);

  // thread_brick_col_9
  ret |= pthread_create (&col9, &attr, (void*)thread_brick_col_9, NULL);

  // thread_brick_col_10
  ret |= pthread_create (&col10, &attr, (void*)thread_brick_col_10, NULL);

  if (ret) {
	  xil_printf ("-- ERROR (%d) launching thread_brick_col_7...\r\n", ret);
  } else {
	  xil_printf ("All brick threads launched.\r\n");
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

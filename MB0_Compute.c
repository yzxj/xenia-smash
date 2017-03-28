/* ================================================*
 * Author         : Lee-Hong Lau and Justin Yeo	   *	
 * Date           : 22/03/2017					   *
 * Version        : V1.0						   *	
 * License        : MIT 						   *
 * ================================================*
 */
/*#include "sys/init.h"
#include "xmk.h"
#include "xmbox.h"
#include "xmutex.h"
#include "xgpio.h"
#include "xuartlite.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <time.h>
#include <xparameters.h>
#include <pthread.h>
#include <errno.h>
#include <sys/msg.h>
#include <sys/ipc.h>
#include <sys/timer.h>*/

#define DISPLAY_COLUMNS       640
#define DISPLAY_ROWS          480
#define RECT_WIDTH 	          40
#define RECT_LENGTH           240
#define RECT_GAP 	          10
#define X1 		 	          50
#define X2 			          X1+RECT_WIDTH+RECT_GAP
#define X3 			          X2+RECT_WIDTH+RECT_GAP
#define X4 			          X3+RECT_WIDTH+RECT_GAP
#define Y1 			          50
#define INIT_BALL_X 		  288
#define INIT_BALL_Y 		  400
#define INIT_BALL_SPEED_X     10
#define INIT_BALL_SPEED_Y     10
#define MAX_BALL_SPEED        40
#define MIN_BALL_SPEED        2
#define BALL_DIR 	          180

#define MSG_COLUMN	          1
#define MSG_BALL	          2

/*	Mailbox Declaration	*/
#define MY_CPU_ID 			  XPAR_CPU_ID
#define MBOX_DEVICE_ID		  XPAR_MBOX_0_DEVICE_ID
static XMbox Mbox;			//Instance of the Mailbox driver

/*	MUTEX ID PARAMETER for HW Mutex	*/ 	
#define MUTEX_DEVICE_ID 	    XPAR_MUTEX_0_IF_1_DEVICE_ID
#define MUTEX_NUM 			      0

XMutex Mutex;
XGpio gpPB; 				//PB device instance.

struct msg {
  int id, old_gold_col, new_gold_col, ball_x_pos, ball_y_pos, bar_x_pos, total_score;
};

typedef struct {
  int dir,x,y;
  uint32_t col;
} ball_msg;

pthread_attr_t attr;
struct sched_param sched_par;
pthread_t mailbox_controller, ball, col1, col2, col3, col4, col5, col6, col7, col8, col9, col10, bar, scoreboard;
pthread_mutex_t mutex, uart_mutex;


// declare the semaphore
sem_t sem;

volatile int new_ball_x = INIT_BALL_X; 
volatile int new_ball_y = INIT_BALL_Y;
volatile int bar_x = DISPLAY_COLUMNS / 2;
volatile int bar_y = DISPLAY_ROWS / 2;
volatile int bar_region = 0;
volatile int button_pressed = 0;
volatile int total_score = 0;
volatile int ballspeed_x = INIT_BALL_SPEED_X;
volatile int ballspeed_y = INIT_BALL_SPEED_Y;

volatile int oldgold_id = 0;
volatile int newgold_id = 0;

/* ----------------------------------------------------
 * Function to send data struct over to MB1 via mailbox
 * ----------------------------------------------------
 */
void send(int id, int old_gold_col, int new_gold_col, int ball_x_pos, int ball_y_pos, int bar_x_pos, int total_score) {

  struct msg send_msg;
  int msgid;

  send_msg.id = id;
  send_msg.old_gold_col = old_gold_col;
  send_msg.new_gold_col = new_gold_col;
  send_msg.ball_x_pos = ball_x_pos;
  send_msg.ball_y_pos = ball_y_pos;
  send_msg.bar_x_pos = bar_x_pos;
  send_msg.tscore = tscore;

  msgid = msgget (id, IPC_CREAT ) ;
  if( msgid == -1 ) {
    xil_printf ("PRODCON: Producer -- ERROR while opening Message Queue. Errno: %d\r\n", errno) ;
    pthread_exit (&errno);
  }
  if( msgsnd (msgid, &send_msg, 32, 0) < 0 ) { 										// Blocking send
    xil_printf ("PRODCON: Producer -- msgsnd of message(%d) ran into ERROR. Errno: %d. Halting..\r\n", errno);
    pthread_exit(&errno);
  }

  XMutex_Lock(&Mutex, MUTEX_NUM);
  print("PRODCON: Producer done !\r\n");
  XMutex_Unlock(&Mutex, MUTEX_NUM);
}

/* ------------------------------------------------------------------
 * Function to compete for counting semaphore, and attain gold status
 * ------------------------------------------------------------------
 */
 void compete_gold(int max, int ID) {
  int randomizer = rand() % 3;

  if (randomizer == 1) {
    sem_wait(&sem);																	// Decrement the value of semaphore s by 1 (use up 1 sema resource)

    pthread_mutex_lock (&mutex);
    oldgold_id=newgold_id;
    newgold_id=ID;
    pthread_mutex_unlock (&mutex);

  } else {
    sleep(100);
  }
}

//---------------------------------------
static void Mailbox_Receive(XMbox *MboxInstancePtr, ball_msg *inbox_pointer) {		//TODO: Reorganize data struct coming into MB0
  XMbox_ReadBlocking(MboxInstancePtr, inbox_pointer, 16);
  if (inbox_pointer->display_updated)
	sem_post(&sem);																	// Increment the value of semaphore s by 1 (free up 1 semaphore count)
}

void shiftBar_xy(int shift_x, int shift_y) {
  bar_x = (bar_x + shift_x + DISPLAY_COLUMNS) % DISPLAY_COLUMNS;
  bar_y = (bar_y + shift_y + DISPLAY_ROWS) % DISPLAY_ROWS;
}

void* thread_mb_controller () {
  while(1) {
    send(1, oldgold_id, newgold_id, new_ball_x, new_ball_y, bar_x, total_score);		// Send mailbox to MB1
    Mailbox_Receive(&Mbox, &ball);														// Read from mailbox
  }
}

void* thread_ball () {
  while(1) {
    // move the ball upwards && upper ceiling boundary check
    if ((new_ball_y-ballspeed_y) >= 67) {
      new_ball_y -= ballspeed_y;
      // update every 1000ms
      sleep(1200);
    }
    // move the ball downwards && lower ceiling boundary check
    if ((new_ball_y+ballspeed_y) <= 398) {
      new_ball_y += ballspeed_y;
      // update every 1000ms
      sleep(1200);
    }
  }
}

void* thread_brick_col_1 () {
  while(1) {
    compete_gold(0x000f, 1);  
	}
}

void* thread_brick_col_2 () {
  while(1) {
    compete_gold(0x000f, 2);  
	}
}

void* thread_brick_col_3 () {
  while(1) {
    compete_gold(0x000f, 3);  
	}
}

void* thread_brick_col_4 () {
  while(1) {
    compete_gold(0x000f, 4);  
	}
}

void* thread_brick_col_5 () {
  while(1) {
    compete_gold(0x000f, 5);  
	}
}

void* thread_brick_col_6 () {
  while(1) {
    compete_gold(0x000f, 6);  
	}
}

void* thread_brick_col_7 () {
  while(1) {
    compete_gold(0x000f, 7);  
	}
}

void* thread_brick_col_8 () {
  while(1) {
    compete_gold(0x000f, 8);  
	}
}

void* thread_brick_col_9 () {
  while(1) {
    compete_gold(0x000f, 9);  
	}
}

void* thread_brick_col_10 () {
  while(1) {
    compete_gold(0x000f, 10);  
	}
}

void* thread_bar () {
  while(1) {
    switch (button_pressed) {
      case 4:
        shiftBar_xy(-4,0);
      break;
      case 8:
        shiftBar_xy(4,0);
      break;
  }
    // Update every 10ms;
    sleep(120);
  }
}

void* thread_scoreboard () {
  while(1) {
    // Increase ball speed by 25fps for every 10 points gained
    if ((ballspeed_x != MAX_BALL_SPEED) && (ballspeed_y != MAX_BALL_SPEED)) {
      ballspeed_x = INIT_BALL_SPEED_X + (total_score/10) + bar_region ;
      ballspeed_y = INIT_BALL_SPEED_Y + (total_score/10) + bar_region ;
    }
  }
}

static void gpPBIntHandler(void *arg) {
  unsigned char val;
  XGpio_InterruptClear(&gpPB,1);
  button_pressed = XGpio_DiscreteRead(&gpPB, 1);
  //  xil_printf("PB event, val = %d \r\n", val); // for testing.
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

  /** ----------------------------------------------------------
    *   Initialize and Configure GPIO interrupts for moving bar
    * ----------------------------------------------------------
    */

  xil_printf("Initializing PB\r\n");
    // Initialise the PB instance
  XGpio_Initialize(&gpPB, XPAR_GPIO_0_DEVICE_ID);
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


  /** Initialize Threads
    * -----------------------------------------------------------------------------------
    * thread_mb_controller  (highest priority)  : Mailbox Controller to pipe data to MB1  
    * thread_ball                               : Ball                                    
    * thread_brick_col_1 ~ thread_brick_col_10  : Brick Columns
    * thread_bar                                : Bar
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
  sched_par.sched_priority = 4;
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
  sched_par.sched_priority = 5;
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
  sched_par.sched_priority = 6;
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
  sched_par.sched_priority = 7;
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
  sched_par.sched_priority = 8;
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
  sched_par.sched_priority = 9;
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
  sched_par.sched_priority = 10;
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
  sched_par.sched_priority = 11;
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
  sched_par.sched_priority = 12;
  pthread_attr_setschedparam(&attr,&sched_par);
  //start thread_brick_col_10
  ret = pthread_create (&col10, &attr, (void*)thread_brick_col_10, NULL);
  if (ret != 0) {
    xil_printf ("-- ERROR (%d) launching thread_brick_col_10...\r\n", ret);
  }
  else {
    xil_printf ("Brick Column 10 launched with ID %d \r\n",col10);
  }

    // Priority 13 for thread_bar
  sched_par.sched_priority = 13;
  pthread_attr_setschedparam(&attr,&sched_par);
  //start thread_bar
  ret = pthread_create (&bar, &attr, (void*)thread_bar, NULL);
  if (ret != 0) {
    xil_printf ("-- ERROR (%d) launching thread_bar...\r\n", ret);
  }
  else {
    xil_printf ("Bar Thread launched with ID %d \r\n",bar);
  }

    // Priority 14 for thread_scoreboard
  sched_par.sched_priority = 14;
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

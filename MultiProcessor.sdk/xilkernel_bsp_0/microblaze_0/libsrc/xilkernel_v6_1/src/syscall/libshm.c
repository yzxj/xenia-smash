/******************************************************************************
*
* (c) Copyright 2010 Xilinx, Inc. All rights reserved.
*
* This file contains confidential and proprietary information of Xilinx, Inc.
* and is protected under U.S. and international copyright and other
* intellectual property laws.
*
* DISCLAIMER
* This disclaimer is not a license and does not grant any rights to the
* materials distributed herewith. Except as otherwise provided in a valid
* license issued to you by Xilinx, and to the maximum extent permitted by
* applicable law: (1) THESE MATERIALS ARE MADE AVAILABLE "AS IS" AND WITH ALL
* FAULTS, AND XILINX HEREBY DISCLAIMS ALL WARRANTIES AND CONDITIONS, EXPRESS,
* IMPLIED, OR STATUTORY, INCLUDING BUT NOT LIMITED TO WARRANTIES OF
* MERCHANTABILITY, NON-INFRINGEMENT, OR FITNESS FOR ANY PARTICULAR PURPOSE;
* and (2) Xilinx shall not be liable (whether in contract or tort, including
* negligence, or under any other theory of liability) for any loss or damage
* of any kind or nature related to, arising under or in connection with these
* materials, including for any direct, or any indirect, special, incidental,
* or consequential loss or damage (including loss of data, profits, goodwill,
* or any type of loss or damage suffered as a result of any action brought by
* a third party) even if such damage or loss was reasonably foreseeable or
* Xilinx had been advised of the possibility of the same.
*
* CRITICAL APPLICATIONS
* Xilinx products are not designed or intended to be fail-safe, or for use in
* any application requiring fail-safe performance, such as life-support or
* safety devices or systems, Class III medical devices, nuclear facilities,
* applications related to the deployment of airbags, or any other applications
* that could lead to death, personal injury, or severe property or
* environmental damage (individually and collectively, "Critical
* Applications"). Customer assumes the sole risk and liability of any use of
* Xilinx products in Critical Applications, subject only to applicable laws
* and regulations governing limitations on product liability.
*
* THIS COPYRIGHT NOTICE AND DISCLAIMER MUST BE RETAINED AS PART OF THIS FILE
* AT ALL TIMES.
*
******************************************************************************/

//----------------------------------------------------------------------------------------------------//
//! @file libshm.c
//! This contains system call wrapper functions for Shared Memory.
//----------------------------------------------------------------------------------------------------//

#include <os_config.h>
#include <sys/syscall.h>
#include <sys/shm.h>
#include <sys/kshm.h>

extern void* make_syscall (void *arg1, void *arg2, void *arg3, void *arg4, void *arg5, int syscall_num);

#ifdef CONFIG_SHM
int shmget (key_t key, size_t size, int shmflg ) 
{
    return (int) make_syscall ((void*)key, (void*)size, (void*)shmflg, NULL, NULL, SC_SHMGET);
}

int shmctl (int shmid, int cmd, struct shmid_ds *buf) 
{
    return (int) make_syscall ((void*)shmid, (void*)cmd, (void*)buf, NULL, NULL, SC_SHMCTL);
}

void* shmat (int shmid, const void *shmaddr, int shmflg ) 
{
    return (void*) make_syscall ((void*)shmid, (void*)shmaddr, (void*)shmflg, NULL, NULL, SC_SHMAT);
}

int shmdt( const void *shmaddr ) 
{
    return (int) make_syscall ((void*)shmaddr, NULL, NULL, NULL, NULL, SC_SHMDT);
}
#endif /* CONFIG_SHM */

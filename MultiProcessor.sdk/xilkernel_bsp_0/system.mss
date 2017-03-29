
 PARAMETER NAME = C:\Users\yeozx\Documents\Xilinx\MultiProcessor\MultiProcessor.sdk\xilkernel_bsp_0\system.mss

 PARAMETER VERSION = 2.2.0


BEGIN OS
 PARAMETER OS_NAME = xilkernel
 PARAMETER OS_VER = 6.1
 PARAMETER PROC_INSTANCE = microblaze_0
 PARAMETER config_msgq = true
 PARAMETER config_named_sema = true
 PARAMETER config_pthread_mutex = true
 PARAMETER config_sema = true
 PARAMETER config_time = true
 PARAMETER max_sem = 20
 PARAMETER sched_type = SCHED_PRIO
 PARAMETER stdin = ps7_uart_1
 PARAMETER stdout = ps7_uart_1
 PARAMETER sysintc_spec = microblaze_0_axi_intc
 PARAMETER systmr_dev = axi_timer_0
 PARAMETER use_malloc = true
END


BEGIN PROCESSOR
 PARAMETER DRIVER_NAME = cpu
 PARAMETER DRIVER_VER = 2.1
 PARAMETER HW_INSTANCE = microblaze_0
END


BEGIN DRIVER
 PARAMETER DRIVER_NAME = gpio
 PARAMETER DRIVER_VER = 4.0
 PARAMETER HW_INSTANCE = axi_gpio_0
END

BEGIN DRIVER
 PARAMETER DRIVER_NAME = gpio
 PARAMETER DRIVER_VER = 4.0
 PARAMETER HW_INSTANCE = axi_gpio_1
END

BEGIN DRIVER
 PARAMETER DRIVER_NAME = tft
 PARAMETER DRIVER_VER = 5.0
 PARAMETER HW_INSTANCE = axi_tft_0
END

BEGIN DRIVER
 PARAMETER DRIVER_NAME = tmrctr
 PARAMETER DRIVER_VER = 3.0
 PARAMETER HW_INSTANCE = axi_timer_0
END

BEGIN DRIVER
 PARAMETER DRIVER_NAME = mbox
 PARAMETER DRIVER_VER = 4.0
 PARAMETER HW_INSTANCE = mailbox_0
END

BEGIN DRIVER
 PARAMETER DRIVER_NAME = intc
 PARAMETER DRIVER_VER = 3.1
 PARAMETER HW_INSTANCE = microblaze_0_axi_intc
END

BEGIN DRIVER
 PARAMETER DRIVER_NAME = bram
 PARAMETER DRIVER_VER = 4.0
 PARAMETER HW_INSTANCE = microblaze_0_local_memory_dlmb_bram_if_cntlr
END

BEGIN DRIVER
 PARAMETER DRIVER_NAME = bram
 PARAMETER DRIVER_VER = 4.0
 PARAMETER HW_INSTANCE = microblaze_0_local_memory_ilmb_bram_if_cntlr
END

BEGIN DRIVER
 PARAMETER DRIVER_NAME = mutex
 PARAMETER DRIVER_VER = 4.0
 PARAMETER HW_INSTANCE = mutex_0
END

BEGIN DRIVER
 PARAMETER DRIVER_NAME = mutex
 PARAMETER DRIVER_VER = 4.0
 PARAMETER HW_INSTANCE = mutex_1
END

BEGIN DRIVER
 PARAMETER DRIVER_NAME = mutex
 PARAMETER DRIVER_VER = 4.0
 PARAMETER HW_INSTANCE = mutex_2
END

BEGIN DRIVER
 PARAMETER DRIVER_NAME = generic
 PARAMETER DRIVER_VER = 2.0
 PARAMETER HW_INSTANCE = ps7_ddr_0
END

BEGIN DRIVER
 PARAMETER DRIVER_NAME = sdps
 PARAMETER DRIVER_VER = 2.1
 PARAMETER HW_INSTANCE = ps7_sd_0
END

BEGIN DRIVER
 PARAMETER DRIVER_NAME = uartps
 PARAMETER DRIVER_VER = 2.1
 PARAMETER HW_INSTANCE = ps7_uart_1
END

BEGIN DRIVER
 PARAMETER DRIVER_NAME = usbps
 PARAMETER DRIVER_VER = 2.1
 PARAMETER HW_INSTANCE = ps7_usb_0
END



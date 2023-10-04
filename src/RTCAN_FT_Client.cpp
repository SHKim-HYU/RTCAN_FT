#ifndef RTCAN_FT_CLIENT_H_
#define RTCAN_FT_CLIENT_H_

#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <sys/mman.h>
#include <rtdk.h>
#include <native/task.h>
#include <native/timer.h>
#include <libpcanfd.h>

#define PCAN_DEVICE PCAN_PCIBUS2

RT_TASK can_task;

unsigned int cycle_ns = 1000000; // 1 ms

void can_comm_task(void *arg)
{
    pcan_handle_t *h;
    struct pcanfd_msg msg;
    int status;

    h = pcanfd_open(PCAN_DEVICE, 0);  // Remove O_RDWR
    if (!h) {
        perror("Failed to open PCAN device");
        return;
    }

    rt_task_set_periodic(NULL, TM_NOW, cycle_ns);
    while (1) {
        rt_task_wait_period(NULL); //wait for next cycle

        // Send CAN frame
        memset(&msg, 0, sizeof(msg));
        msg.type = PCANFD_TYPE_CAN20_MSG;
        msg.id = 0x064;
        msg.len = 8;  // Change from dlc to len
        msg.data[0] = 0x0B;
        // ... set other DATA bytes as needed
        status = pcanfd_send_msg(h, &msg);
        if (status < 0) {
            perror("Failed to send CAN frame");
        }

        // Read CAN frame
        status = pcanfd_recv_msg(h, &msg, NULL);
        if (status < 0) {
            perror("Failed to read CAN frame");
        } else {
            printf("Received: ID=0x%X, LEN=%d, data[0]=0x%X\n",  // Change DLC to LEN
                   msg.id, msg.len, msg.data[0]);
        }    
    }

    pcanfd_close(h);
}


void signal_handler(int signum)
{
    rt_task_delete(&can_task);
    printf("Servo drives Stopped!\n");
    exit(1);
}

int main(int argc, char *argv[])
{
    // Perform auto-init of rt_print buffers if the task doesn't do so
    rt_print_auto_init(1);

    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    /* Avoids memory swapping for this program */
    mlockall(MCL_CURRENT|MCL_FUTURE);

    rt_task_create(&can_task, "canTask", 0, 99, 0);
    rt_task_start(&can_task, &can_comm_task, NULL);

    // Must pause here
    pause();

    // Finalize
    signal_handler(0);

    return 0;
}
#endif

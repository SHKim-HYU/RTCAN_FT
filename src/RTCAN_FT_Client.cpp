#ifndef RTCAN_FT_CLIENT_H_
#define RTCAN_FT_CLIENT_H_

#include <stdio.h>
#include "iostream"
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <sys/mman.h>
// #include <rtdk.h>
#include <alchemy/task.h>
#include <alchemy/timer.h>
#include <alchemy/mutex.h>

#include <PCANDevice.h>

// PCI/E-FD
#define DEVICE "/dev/rtdm/pcan0"

RT_TASK read_task;

PCANDevice can;

unsigned int cycle_ns = 1000000; // 1 ms

unsigned char data_field[16];

short raw_data[6] = { 0 };
unsigned short temp;
unsigned DF=50, DT=2000;
double ft_array[6];

void read_thread(void *arg)
{
    int res1, res2;
    CANDevice::CAN_msg_t TxFrame;
    CANDevice::CAN_msg_t RxFrame1;
    CANDevice::CAN_msg_t RxFrame2;

    TxFrame.id = 0x64;
    TxFrame.length = 8;
    TxFrame.data[0] = 0x0A;
    TxFrame.data[1] = 0x00;
    TxFrame.data[2] = 0x00;
    TxFrame.data[3] = 0x00;
    TxFrame.data[4] = 0x00;
    TxFrame.data[5] = 0x00;
    TxFrame.data[6] = 0x00;
    TxFrame.data[7] = 0x00;

    RxFrame1.length = 8;
    RxFrame2.length = 8;

    can.Status();

    can.Send(TxFrame);

    rt_task_set_periodic(NULL, TM_NOW, cycle_ns);
    while (1) {
        rt_task_wait_period(NULL); //wait for next cycle
        
        res2 = can.Receive(RxFrame2);
        res1 = can.Receive(RxFrame1);

        can.Send(TxFrame);

        if (res1 == 1 && res2 == 1)
        {
            //CANbus data to Torque data
            for(int i = 0; i<8; i++)
            {
                data_field[i] = (unsigned char) RxFrame1.data[i];
                data_field[i+8] = (unsigned char) RxFrame2.data[i];
            }
            
            for(int idx = 0; idx<6; idx++)
            {
                temp = data_field[2*idx+1]*256;
                temp += data_field[2*idx+2];

                raw_data[idx] = (signed short) temp;
            }

            // Set Force/Torque Original
            for(int n = 0; n<3; n++)
            {
                ft_array[n] = ((float)raw_data[n]) / DF;
                ft_array[n+3] = ((float)raw_data[n+3]) / DT;
            }

            rt_printf("\nX direction Force: %f   \n",   ft_array[0]);
            rt_printf("Y direction Force: %f    \n",    ft_array[1]);
            rt_printf("Z direction Force: %f    \n\n",  ft_array[2]);
            rt_printf("X direction Torque: %f   \n",    ft_array[3]);
            rt_printf("Y direction Torque: %f   \n",    ft_array[4]);
            rt_printf("Z direction Torque: %f   \n\n",  ft_array[5]);
        }
    }
    can.Close();
}

void signal_handler(int signum)
{
    rt_task_delete(&read_task);
    printf("Servo drives Stopped!\n");
    exit(1);
}

int main(int argc, char *argv[])
{
    // CAN Setup
    CANDevice::Config_t config;
    config.mode_fd = 0; // 0: CAN2.0 Mode, 1: CAN-FD Mode
    config.bitrate = 1e6; //1mbps
    config.d_bitrate = 2e6; //2mbps
    config.sample_point = .875; //87.5% 
    config.d_sample_point = 0.6; //60%
    config.clock_freq = 80e6; // 80mhz // Read from driver?  
    

    if(!can.Open(DEVICE, config, false))
    {
        std::cout << "Unable to open CAN Device" << std::endl;
        // exit(-2);
        return 0;
    }

    // Setup Filters
    can.ClearFilters(); // Clear Existing/Reset.  Filters are saved on the device hardware.  Must make sure to clear
    can.AddFilter(1, 2); // Only Listen to messages on id 0x01, 0x02.  

    // Perform auto-init of rt_print buffers if the task doesn't do so
    rt_print_init(0, NULL);

    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    /* Avoids memory swapping for this program */
    mlockall(MCL_CURRENT|MCL_FUTURE);

    rt_task_create(&read_task, "read_task", 0, 90, 0);
    rt_task_start(&read_task, &read_thread, NULL);

    // Must pause here
    pause();

    // Finalize
    signal_handler(0);

    return 0;
}
#endif

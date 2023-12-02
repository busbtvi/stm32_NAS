Init_lwIP(){ lwIP.c
    tcpip_init(){ tcpip.c
        lwip_init(){ init.c
            sys_init
            mem_init: mem.c
                ram_heap = [LWIP_MEM_ALIGN_SIZE((8*1024)) + (2*LWIP_MEM_ALIGN_SIZE(sizeof(struct mem))) + 4]
                    = [8913 + ?? + 4]
                - LWIP_MEM_ALIGN_SIZE(size) = (size + 3) & ~(3)
                - MEM_ALIGNMENT 4
        }
        tcpip_init_done: setting
            - tcpip_thread
            - TcpioInitDone: sys_sem_signal

        mbox = sys_mbox_new()

        sys_thread_new(){ sys_arch.c
        }
    }
}
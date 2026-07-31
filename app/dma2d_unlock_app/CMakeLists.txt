# Contest 2026 team 137 DMA2D RISAF-unlock experiment
if(CONFIG_LVX_USE_DEMO_CONTEST2026_137_DMA2D_UNLOCK_APP)
  nuttx_add_application(
    NAME dma2d_unlock
    SRCS dma2d_unlock_main.c
    STACKSIZE 2048)
endif()

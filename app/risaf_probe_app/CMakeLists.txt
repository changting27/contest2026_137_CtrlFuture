# Contest 2026 team 137 RISAF firewall write-access probe
if(CONFIG_LVX_USE_DEMO_CONTEST2026_137_RISAF_PROBE_APP)
  nuttx_add_application(
    NAME risaf_probe
    SRCS risaf_probe_main.c
    STACKSIZE 2048)
endif()

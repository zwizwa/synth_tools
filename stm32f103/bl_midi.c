#define PARTITION_CONFIG_OVERLAP_PARTITION_INIT(addr, size) \
  { .max_size = size, .page_logsize = 10, .config = (void*)(addr) }
#define PARTITION_CONFIG_INIT {                                         \
        PARTITION_CONFIG_OVERLAP_PARTITION_INIT(0x08004000, 0x1C000),   \
        PARTITION_CONFIG_OVERLAP_PARTITION_INIT(0x08012000, 0x0E000),   \
}
#define GDBSTUB_MEMORY_MAP GDBSTUB_MEMORY_MAP_STM32F103CB
#define GDBSTUB_BOOT1_START(x) ((x)==0)
#define GDBSTUB_RSP_ENABLED 0
#define MONITOR_ENABLED 1
#define TRAMPOLINE_IN_BOOTLOADER
#define MANUFACTURER "Zwizwa"
#define PRODUCT "MIDI Synth"
#include "mod_trampoline.c"
#define CONFIG_DEFAULT 1
const char config_version[] = "bl_midi.c";
#include "mod_bl_midi.c"



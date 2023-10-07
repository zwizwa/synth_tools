# -*- gdb-script -*-
set confirm off
set pagination off

# "The program being debugged was signaled while in a function called from GDB."
set unwindonsignal on


# See OpenOCD manual
# Note that RAM needs to be initialized for this to work.
# E.g. auto-start via USB, or "p _config.start()" at (gdb) prompt
define rtt_init
  mon rtt server stop 9090
  mon rtt stop
  # Note that we start looking in the application RAM segment.
  # I can't explain why, but I've found he magic marker in the
  # bootloader RAM segment.
  mon rtt setup 0x20002000 0x5000 "SEGGER RTT"
  mon rtt start
  mon rtt channels
  mon rtt server start 9090 0
end

# Reset and run target but keep gdb console active for data inspection.
define reset
  monitor reset run
end

define file_check
  echo file=
  echo $arg0
  echo \n
  file $arg0
end

# All images rely on the MIDI SYSEX bootloader.
# Tis assumes cwd is set to synth_tools/stm32f103
define bl
  file_check bl_midi_bp.core.f103.elf
end
define pixi
  file_check pixi.x8.f103.elf
end

define startup
    bl
    connect
end

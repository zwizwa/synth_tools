# -*- gdb-script -*-
set confirm off
set pagination off

# "The program being debugged was signaled while in a function called from GDB."
set unwindonsignal on


define file_check
  echo file=
  echo $arg0
  echo \n
  file $arg0
end

# All images rely on the MIDI SYSEX bootloader.
# Tis assumes cwd is set to synth_tools/stm32f103
define bl
  file_check bl_midi.core.f103.elf
end
define pixi
  file_check pixi.x8.f103.elf
end



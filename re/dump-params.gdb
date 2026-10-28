set pagination off
set confirm off
set environment LD_LIBRARY_PATH /home/rick/sysroot-i386/usr/lib/i386-linux-gnu:/home/rick/sysroot-i386/lib/i386-linux-gnu
set environment PPD /home/rick/src/phaser6125/re/build/Phaser_6125N.ppd
set environment CUPS_FONTPATH /usr/share/fonts
break *0x08049184
break *0x08049243
run 1 rick test 1 "PageSize=A4 FXOutputMode=600x600 FXColorMode=Color InstalledMemory=64Meg Option1=1Tray-S FXInputSlot=AutoSelect" < /home/rick/src/phaser6125/re/build/s3-pr.fxr > /dev/null
printf "BP1: mode=%d fxdlut=%d cstbuf=%p\n", *(int*)($ebp-0x58), *(int*)($ebp-0x34), *(int*)($ebp-0xd8)
dump binary memory /home/rick/src/phaser6125/re/build/cst-param.bin *(int*)($ebp-0xd8) (*(int*)($ebp-0xd8)+1684)
continue
printf "BP2: dlutbuf=%p\n", *(int*)($ebp-0xdc)
dump binary memory /home/rick/src/phaser6125/re/build/dlut-param.bin *(int*)($ebp-0xdc) (*(int*)($ebp-0xdc)+20420)
kill
quit

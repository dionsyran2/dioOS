set OSNAME=CustomOS
set BUILDDIR=%0/../bin
set OVMFDIR=%0/../../OVMFbin

cd "C:\Program Files\qemu"
qemu-system-x86_64 ^
-machine q35 ^
-m 8G ^
-smp cores=2 ^
-cpu max ^
-drive file=%BUILDDIR%/%OSNAME%.img ^
-drive if=pflash,format=raw,unit=0,file="%OVMFDIR%/OVMF_CODE-pure-efi.fd",readonly=on ^
-drive if=pflash,format=raw,unit=1,file="%OVMFDIR%/OVMF_VARS-pure-efi.fd" ^
-drive file="%BUILDDIR%/Audio CD.bin",format=raw,media=cdrom ^
-device intel-hda,debug=0 ^
-device hda-output,audiodev=snd0 ^
-audiodev dsound,id=snd0 ^
-drive if=none,id=usbd,file=%BUILDDIR%/blank.img ^
-net none ^
-monitor stdio ^
-d guest_errors ^
-no-reboot -no-shutdown
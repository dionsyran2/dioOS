# dioOS
**What is dioOS** I hear you ask. dioOS is an operating system in early developement. This will never be something big and I am not aiming for that, this is just a 'small' project I started because I wanted to learn about low level developement and how the kernel interfaces with devices and applications. I am aiming for full compatibility with binaries compiled for linux... well I hope it will be full at some point, its much much harder than I originally anticipated.

#### What works
Currently it can run applications (ELF) linked against musl. This includes sh, busybox, ls and some other stuff ~~*that I totally didn't steal from an alpine installation*~~. Applications compiled with glibc may work if they dont use fork, glibc uses the clone syscall under the hood which,
well lets just say it does not work as expected.
##### Audio
There is basic ALSA support, currently locked to 16 bits per sample. It isnt anything special, `aplay` and `speaker-test` both work.

#### What does not work
Well, bash has an interesting behaviour, the tty emulator does have its issues, The vfs should be rewritten, I had no idea what I was doing when I wrote it. Links are also not supported (e.g. ls -> busybox) so if you want to use vim for example, you have to do **busybox vi filename.txt**. Any non position independend applications will fail, as the kernel is currently mapped at 1MB.

#### The kernel
The dioOS kernel is a monolithic x86_64 kernel written in c++, and a bit of our beloved intel assembly.

#### Screenshots
well, I would add screenshots here, but its just sh... it isn't anything special


## How to build
### Requirements
You will need GCC... I guess any compiler will do but you will have to edit the makefile
You may need other stuff, like wget. On a fresh install of ubuntu I had to install **bison** and **flex** (to compile grub).

### Build it
Well, if you really wanna build this...

Firstly you need a copy of the source code:
`git clone https://github.com/dionsyran2/dioOS.git`

Then you gotta set it up by running `make setup`. <br>
This will create a bunch of directories, download and compile grub.

And now the hardest part of it all, run `make` to compile the kernel / applications and create a disk image. <br>
After you run make, you will see 2 images in the folder, a dioOS.img which you can use with qemu and a dioOS.vmdk which you can use with vmware. <br>

I would recommend you to give it at least 4GB of ram... I have had issues before with it crashing.

#### I almost forgot, here is a list of the users set up. You will need this to log in :)

| Username  | Password |
| ----------|:--------:|
| root      | 1234     |
| user      |          |

The passwd and shadow files can be found in disk/etc/
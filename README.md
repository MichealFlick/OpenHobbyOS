## OpenHobbyOS (OHOS)
First lets start with whats OpenHobbyOS is. OpenHobbyOS is an compatibility focused operating system written in C. it ports some cool things like X.org server . NewLib . fastfetch. it started in 2024 then i ditched it. now i refocused on it again and after some more work i managed to make it suitable for the public !.

this is a learning project !. code quality itslef might not be the best and its probably full of glue code, so dont have high stakes for it !. if you found any bugs or issues you can open a pr to fix it or submit an issue !.

you can git clone this repo and run in qemu. pretty standard. it requires GCC. xorriso. and the standard tools to make a GRUB bootable ISO .

this project is made to be compiled on linux so if you want to try it on widnows. yeah it will be a pain.

if you are seeing this as of april 2026 this OS is still unstable and very hard to reproduce. still not meant to be reproduced by other people yet for now. this will change soon.

## Common questions (aka FAQ)
- **What is the goal of OHOS ?**
OHOS is firstly a learning projecct. secondly and edecutional resource . thirdly an expierment with modern development workflows that i didn't get to interact with in the past
- **Why does OHOS port newlib instead of posix compliant standard lib like mlibc ?** thats a great question. the reason is that openhobbyos is trying to stay small so it can have a high probality to run on embedded systems. which newlib is greater at then mlibc . even if it meant less posix compliance then mlibc and having to hand write more things. its still more mature and standard enough to give more then what it takes.
- **How does OHOS achieve stabality under pressure and high peformance when running software inside it ?** another great question. OHOS is simply small enough to achieve speed. its size reduces os overhead and allows more features to be implemented before the OS reaches the thereshold where peformance even starts degrading. at that point the OS would already have a stable base and a mature userland/userspace that makes it even more usable. i make sure OHOS stays small while porting as many features as possible without bloating it or putting pressure on it.

- **Why did OHOS choosed DWM to run with X11 infostructure inside of it ?** simply because dwm was the easiest yet most rewarding and fastest option in OHOS context. much easier then porting OpenBox and spending 10x more time for features that we are not demanding yet.

- **Why does OHOS port X11 instead of reimplementing a "subset" of its features manually ?** because implementing even a "subset" of x11 features takes more time and is less credible then just porting few more libs with x11 and making it work . even if we pretended im just porting a subset. its still more work then just porting xserver and x.org components. less stable and reliable then upstream x11 implementation. and much less credible . and is genuinly not my learning target.

Under BSD license
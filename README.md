# Introduction

这是一个简单的vfs实现，如果你懒得在你自己的OS上实现vfs，你可以将其移至你的OS中。

在[Plant-OS/fs/fat.c](https://github.com/plos-clan/Plant-OS/blob/main/src/fs/fat.c)中有关于[fatfs](https://github.com/abbrev/fatfs)的实现，可以通过该vfs操作fatfs的文件（夹）

# The Projects Used
1. [copi143](https://github.com/copi143)'s libc
2. [copi143](https://github.com/copi143)'s data structure

# Extensions

可以参考[CoolPotOS/src/x86_64/fs/vfs.c](https://github.com/plos-clan/CoolPotOS/blob/main/src/x86_64/fs/vfs.c)来支持`poll`、`dup`、`ioctl`、`map`等扩展接口。

# Additional
**Have fun!**

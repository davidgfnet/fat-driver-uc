fat-driver-uc
=============

Small FAT16/32 driver for uP

This little source allows writing and reading files stored
in a FAT partition. The library has little memory footprint
and is ideal for micro controllers. I've used it in PIC projects
for SD card accessing.

It features FAT table caching, FAT16 and FAT32 accessing and 
supports partition lookup.

The library needs a read and write function to be able to access
the underlying data block. Here you should implement the routines
necessary for disk access (in Linux would be read/write), SD card
or any other device.


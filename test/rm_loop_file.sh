#!/bin/bash
sudo losetup -d `losetup -l | grep regular_file | awk '{print $1}'`
sudo umount fat32_mnt
rmdir fat32_mnt/
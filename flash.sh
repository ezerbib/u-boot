arm-uclinuxeabi-gdb ./u-boot -ex 'target remote 192.168.68.1:2331' -ex 'monitor halt' -ex 'load' -ex 'monitor reset' -ex 'mon flash breakpoints=1'

qemu-system-i386 --kernel O.RTEMS-pc386/rtemsTestHarness -netdev user,id=mynet0 -device ne2k_isa,netdev=mynet0 -redir tcp:5075::5075 -redir udp:5076::5076 -m 1024 --no-reboot -curses

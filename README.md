# timer-in-C

Timer implamentaiton in C. Periodically call TimerFcn with respective Period.

## To compile in terminal (desktop version):
gcc -O3 -pthread main.c -o main.out

## To compile for raspberry pi zero:
* see wiki for cross compiler installation -> https://github.com/abhiTronix/raspberry-pi-cross-compilers/wiki
* arm-linux-gnueabihf-gcc -O3 -pthread main.c -o main.out

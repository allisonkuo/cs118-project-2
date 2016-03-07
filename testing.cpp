#include <iostream>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <pthread.h>
#include <time.h>
#include <unistd.h>

int main()
{
    time_t *timestamps;
    timestamps = (time_t *) malloc(2 * sizeof(time_t));
    timestamps[0] = 0;
    timestamps[1] = 0;
    printf("time0:%d\n",(int) timestamps[0]);
    printf("time1:%d\n",(int) timestamps[1]);
    timestamps[0] = time(NULL);
    printf("time0:%d\n",(int) timestamps[0]);
    printf("time1:%d\n",(int) timestamps[1]);
    timestamps[1] = time(NULL);
    printf("time0:%d\n",(int) timestamps[0]);
    printf("time1:%d\n",(int) timestamps[1]);
}

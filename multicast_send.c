/*
 * This file is part of multicast_test.
 *
 * Copyright 2015 Ricardo Garcia <r@rg3.name>
 *
 * To the extent possible under law, the author(s) have dedicated all copyright
 * and related and neighboring rights to this software to the public domain
 * worldwide. This software is distributed without any warranty. 
 *
 * You should have received a copy of the CC0 Public Domain Dedication along
 * with this software. If not, see
 * <http://creativecommons.org/publicdomain/zero/1.0/>.
 */
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/mman.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <limits.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#define MIN_MSG_SIZE (1)
#define MAX_MSG_SIZE (32768)

void usage(char *progname)
{
    fprintf(stderr, "Usage: %s MULTICAST_IP PORT INTERFACE_IP PACKET_SIZE PACKET_NUMBER PACKET_INTERVAL_IN_Us\n",
            progname);
}

int main(int argc, char *argv[])
{
    FILE *stream = NULL;
    char *msg = NULL;
    int retval = 0;
    int fd = -1;

    /*
     * Argument processing part.
     */

    if (argc < 7)
    {
        usage(argv[0]);
        retval = 1;
        goto out;
    }

    // Parse multicast IP.
    struct in_addr mip;
    if (! inet_pton(AF_INET, argv[1], &mip))
    {
        fprintf(stderr, "Error: invalid multicast IP: %s\n", argv[1]);
        retval = 2;
        goto out;
    }

    // Parse port.
    in_port_t port;
    char *endptr;
    long lport = strtol(argv[2], &endptr, 10);
    if (endptr != argv[2] + strlen(argv[2]) || lport > USHRT_MAX)
    {
        fprintf(stderr, "Error: invalid port: %s\n", argv[2]);
        retval = 3;
        goto out;
    }
    port = (in_port_t)lport;

    // Parse interface IP.
    struct in_addr iip;
    if (! inet_pton(AF_INET, argv[3], &iip))
    {
        fprintf(stderr, "Error: invalid interface IP: %s\n", argv[3]);
        retval = 4;
        goto out;
    }

    // Read message file.
    off_t size = atoi(argv[4]);
    if (size < 64) {
        size = 64;
    }
    msg = malloc(size);
    if (! msg)
    {
        fprintf(stderr, "Error: unable to allocate memory\n");
        retval = 8;
        goto out;
    }
    memset(msg, 0, size);

    /*stream = fopen(argv[4], "rb");
    if (! stream)
    {
        fprintf(stderr, "Error: unable to open message file\n");
        retval = 5;
        goto out;
    }

    if (fseeko(stream, 0, SEEK_END) != 0)
    {
        fprintf(stderr, "Error: unable to get message size\n");
        retval = 6;
        goto out;
    }

    off_t size = ftello(stream);
    if (size < MIN_MSG_SIZE || size > MAX_MSG_SIZE)
    {
        fprintf(stderr, "Error: file size not in valid range (%d-%d bytes)\n",
                MIN_MSG_SIZE, MAX_MSG_SIZE);
        retval = 7;
        goto out;
    }

    if (fseeko(stream, 0, SEEK_SET) != 0)
    {
        fprintf(stderr, "Error: unable to seek inside file\n");
        retval = 9;
        goto out;
    }

    if (fread(msg, (size_t)size, 1, stream) != 1)
    {
        fprintf(stderr, "Error: unable to read message file\n");
        retval = 9;
        goto out;
    } */

    // send number
    int num = atoi(argv[5]);
    int interval = atoi(argv[6]);
    
    int idx = 0;
    if (argc >= 8) {
        idx = atoi(argv[7]);
    }
    /*
     * Network part.
     */

    fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd == -1)
    {
        perror("Error: unable to create socket");
        retval = 10;
        goto out;
    }
   
   struct sockaddr_in baddr = {
       .sin_family = AF_INET,
       .sin_port = 0,
       .sin_addr = {
           .s_addr = INADDR_ANY
       }
   };
   if (bind(fd, (const struct sockaddr *)(&baddr), sizeof(baddr)) == -1)
   {
       perror("Error: unable to bind socket");
       retval = 11;
       goto out;
   }

   if (setsockopt(fd, IPPROTO_IP, IP_MULTICAST_IF,
                  (const void *)(&iip), sizeof(iip)) == -1)
   {
       perror("Error: unable to set multicast interface");
       retval = 12;
       goto out;
   }

   struct sockaddr_in daddr = {
       .sin_family = AF_INET,
       .sin_port = htons(port),
       .sin_addr = {
           .s_addr = mip.s_addr
       }
   };
   struct timespec deadline;
   struct timespec starttime;
   clock_gettime(CLOCK_MONOTONIC, &deadline);
   clock_gettime(CLOCK_MONOTONIC, &starttime);
   deadline.tv_nsec += interval;
   if(deadline.tv_nsec >= 1000000000) {
       deadline.tv_nsec -= 1000000000;
       deadline.tv_sec++;
   }

   /* for multiple send session, it will have a random delay */
   if (idx > 0) {
       int rand, i;
       struct timespec delay;
       clock_gettime(CLOCK_MONOTONIC, &delay);
       srandom(delay.tv_nsec);
       for (i = 0; i < idx; i++){
           rand = random();
       }

       delay.tv_nsec += (rand % interval);
       if(delay.tv_nsec >= 1000000000) {
           delay.tv_nsec -= 1000000000;
           delay.tv_sec++;
       }
       clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &deadline, NULL);
   }

   int i = 0;
   for (i = 0; i < num; i++) {
      *((int *)msg) = i;

      if (sendto(fd, msg, size, 0,
              (const struct sockaddr *)(&daddr), sizeof(daddr)) == -1) {
        perror("Error: unable to send message");
        retval = 13;
        goto out;
      }

      clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &deadline, NULL);
 
       deadline.tv_nsec += interval;
       if(deadline.tv_nsec >= 1000000000) {
           deadline.tv_nsec -= 1000000000;
           deadline.tv_sec++;
      }
   }
   
   struct timespec endtime;
   clock_gettime(CLOCK_MONOTONIC, &endtime);
   int second = endtime.tv_sec - starttime.tv_sec;
   long nsec = 0;
   if (endtime.tv_nsec > starttime.tv_nsec) {
       nsec = endtime.tv_nsec - starttime.tv_nsec;
   } else {
       nsec = starttime.tv_nsec - endtime.tv_nsec;
       second -= 1;
   }

   printf("it takes %d seconds %d nanosecond to send %d packets\n", second, nsec, num);

out:
    if (fd != -1)
        close(fd);
//    if (stream)
//        fclose(stream);
    free(msg);
    return retval;
}

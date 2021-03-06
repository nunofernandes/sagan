/*
** Copyright (C) 2009-2015 Quadrant Information Security <quadrantsec.com>
** Copyright (C) 2009-2015 Champ Clark III <cclark@quadrantsec.com>
**
** This program is free software; you can redistribute it and/or modify
** it under the terms of the GNU General Public License Version 2 as
** published by the Free Software Foundation.  You may not use, modify or
** distribute this program under any other version of the GNU General
** Public License.
**
** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
** GNU General Public License for more details.
**
** You should have received a copy of the GNU General Public License
** along with this program; if not, write to the Free Software
** Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
*/

/* sagan-peek.c
 *
 * This small utility "peeks" into Sagan memory to display thresholds,
 * afters, flowbis, etc.  The term "peek" goes back to old BASIC "peek"
 * and "poke" of memory.
 *
 */


#include <stdio.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/stat.h>
#include <stdint.h>
#include <inttypes.h>
#include <arpa/inet.h>
#include <string.h>
#include <time.h>

#include "../src/sagan.h"
#include "../src/sagan-defs.h"
#include "../src/sagan-flowbit.h"

/****************************************************************************
 * object_check - Verifies a memory object exists before doing an shm_open.
 * This way,  we don't mistakingly "create" the object!
 ****************************************************************************/

void object_check( char *object )
{

    char tmp_object_check[255];
    struct stat object_check;

    snprintf(tmp_object_check, sizeof(tmp_object_check) - 1, "%s%s", SHM_LOCATION, COUNTERS_IPC_FILE);

    if ( ( stat(tmp_object_check, &object_check) == -1 ))
        {
            fprintf(stderr, "Error.  The Sagan IPC object file '%s' was not found!\n", tmp_object_check);
            exit(1);
        }
}

/****************************************************************************
 * u32_time_to_human - Convert epoch time to human readable
 ****************************************************************************/

char *u32_time_to_human( uint64_t utime )
{

    struct tm tm;
    static char time_buf[80];
    char tmp[80];

    char *return_time = NULL;

    memset(&tm, 0, sizeof(struct tm));
    snprintf(tmp, sizeof(tmp) - 1, "%lu", utime);

    strptime(tmp, "%s", &tm);
    strftime(time_buf, sizeof(time_buf), "%b %d %H:%M %Y", &tm);

    return_time = (char*)&time_buf;

    return(return_time);

}

/****************************************************************************
 * main - Pull data from shared memory and display it!
 ****************************************************************************/

int main(int argc, char **argv)
{

    struct _Sagan_IPC_Counters *counters_ipc;

    struct _Sagan_IPC_Flowbit *flowbit_ipc;

    struct thresh_by_src_ipc *threshbysrc_ipc;
    struct thresh_by_dst_ipc *threshbydst_ipc;
    struct thresh_by_username_ipc *threshbyusername_ipc;

    struct after_by_src_ipc *afterbysrc_ipc;
    struct after_by_dst_ipc *afterbydst_ipc;
    struct after_by_username_ipc *afterbyusername_ipc;

    /* For convert 32 bit IP to octet */

    struct in_addr ip_addr_src;
    struct in_addr ip_addr_dst;

    /* Shared memory descriptors */

    int shm_counters;
    int shm;

    int i;

    char tmp_object_check[255];
    char tmp[64];

    /* Load the "counters" first.  The "counters" keep track of the number of elements on the
     * other arrays */

    snprintf(tmp_object_check, sizeof(tmp_object_check) - 1, "%s%s", SHM_LOCATION, COUNTERS_IPC_FILE);
    object_check(tmp_object_check);

    if (( shm_counters = shm_open(COUNTERS_IPC_FILE, (O_CREAT | O_RDWR), (S_IREAD | S_IWRITE))) == -1 )
        {
            fprintf(stderr, "[%s, line %d] Cannot shm_open() for counters (%s)\n", __FILE__, __LINE__, strerror(errno));
            exit(1);
        }

    ftruncate(shm_counters, sizeof(_Sagan_IPC_Counters));

    if (( counters_ipc = mmap(0, sizeof(_Sagan_IPC_Counters) , (PROT_READ | PROT_WRITE), MAP_SHARED, shm_counters, 0)) == MAP_FAILED )
        {
            fprintf(stderr, "[%s, line %d] Error allocating memory for counters object! [%s]\n", __FILE__, __LINE__, strerror(errno));
            exit(1);
        }

    /*** Get "threshold by source" data ****/

    snprintf(tmp_object_check, sizeof(tmp_object_check) - 1, "%s%s", SHM_LOCATION, THRESH_BY_SRC_IPC_FILE);
    object_check(tmp_object_check);

    if ((shm = shm_open(THRESH_BY_SRC_IPC_FILE, (O_CREAT | O_RDWR), (S_IREAD | S_IWRITE))) < 0 )
        {
            fprintf(stderr, "[%s, line %d] Cannot shm_open() for thresh_by_src (%s)\n", __FILE__, __LINE__, strerror(errno));
            exit(1);
        }

    ftruncate(shm, sizeof(thresh_by_src_ipc));

    if (( threshbysrc_ipc = mmap(0, sizeof(thresh_by_src_ipc) + (sizeof(thresh_by_src_ipc) * counters_ipc->thresh_count_by_src), (PROT_READ | PROT_WRITE), MAP_SHARED, shm, 0)) == MAP_FAILED )
        {
            fprintf(stderr, "[%s, line %d] Error allocating memory for thresh_by_src object! [%s]\n", __FILE__, __LINE__, strerror(errno));
            exit(1);
        }

    close(shm);

    if ( counters_ipc->thresh_count_by_src >= 1 )
        {


            printf("\n                      ***  Threshold by source ***\n");
            printf("-----------------------------------------------------------------------------\n");
            printf("SRC IP\t\t| Count\t\t| Date added/modified\t| Signature ID\n");
            printf("-----------------------------------------------------------------------------\n");


            for ( i = 0; i < counters_ipc->thresh_count_by_src; i++)
                {

                    ip_addr_src.s_addr = htonl(threshbysrc_ipc[i].ipsrc);
                    printf("%s\t| %d\t\t| %s\t| %s\n", inet_ntoa(ip_addr_src), threshbysrc_ipc[i].count, u32_time_to_human(threshbysrc_ipc[i].utime), threshbysrc_ipc[i].sid);

                }
        }

    /*** Get "threshold by destination" data ***/

    snprintf(tmp_object_check, sizeof(tmp_object_check) - 1, "%s%s", SHM_LOCATION, THRESH_BY_DST_IPC_FILE);
    object_check(tmp_object_check);

    if ((shm = shm_open(THRESH_BY_DST_IPC_FILE, (O_CREAT | O_RDWR), (S_IREAD | S_IWRITE))) < 0 )
        {
            fprintf(stderr, "[%s, line %d] Cannot shm_open() (%s)\n", __FILE__, __LINE__, strerror(errno));
            exit(1);
        }

    ftruncate(shm, sizeof(thresh_by_dst_ipc));

    if (( threshbydst_ipc = mmap(0, sizeof(thresh_by_dst_ipc) + (sizeof(thresh_by_dst_ipc)*counters_ipc->thresh_count_by_dst) , (PROT_READ | PROT_WRITE), MAP_SHARED, shm, 0)) == MAP_FAILED )
        {
            fprintf(stderr, "[%s, line %d] Error allocating memory object! [%s]\n", __FILE__, __LINE__, strerror(errno));
            exit(1);
        }

    close(shm);


    if ( counters_ipc->thresh_count_by_dst >= 1 )
        {


            printf("\n                    ***  Threshold by destination ***\n");
            printf("-----------------------------------------------------------------------------\n");
            printf("SRC IP\t\t| Count\t\t| Date added/modified\t| Signature ID\n");
            printf("-----------------------------------------------------------------------------\n");

            for ( i = 0; i < counters_ipc->thresh_count_by_dst; i++)
                {
                    ip_addr_src.s_addr = htonl(threshbydst_ipc[i].ipdst);
                    printf("%s\t| %d\t\t| %s\t| %s\n", inet_ntoa(ip_addr_src), threshbydst_ipc[i].count, u32_time_to_human(threshbydst_ipc[i].utime), threshbydst_ipc[i].sid);
                }

        }

    /*** Get "threshold by username" data ***/

    snprintf(tmp_object_check, sizeof(tmp_object_check) - 1, "%s%s", SHM_LOCATION, THRESH_BY_USERNAME_IPC_FILE);
    object_check(tmp_object_check);

    if ((shm = shm_open(THRESH_BY_USERNAME_IPC_FILE, (O_CREAT | O_RDWR), (S_IREAD | S_IWRITE))) < 0 )
        {
            fprintf(stderr, "[%s, line %d] Cannot shm_open() (%s)\n", __FILE__, __LINE__, strerror(errno));
            exit(1);
        }

    ftruncate(shm, sizeof(thresh_by_username_ipc));

    if (( threshbyusername_ipc = mmap(0, sizeof(thresh_by_username_ipc) + (sizeof(thresh_by_username_ipc) *  counters_ipc->thresh_count_by_username ), (PROT_READ | PROT_WRITE), MAP_SHARED, shm, 0)) == MAP_FAILED )
        {
            fprintf(stderr, "[%s, line %d] Error allocating memory object! [%s]\n", __FILE__, __LINE__, strerror(errno));
            exit(1);
        }

    close(shm);


    if ( counters_ipc->thresh_count_by_username >= 1 )
        {


            printf("\n                      ***  Threshold by username ***\n");
            printf("-----------------------------------------------------------------------------\n");
            printf("Username\t\t| Count\t\t| Date added/modified\t| Signature ID\n");
            printf("-----------------------------------------------------------------------------\n");


            for ( i = 0; i < counters_ipc->thresh_count_by_username; i++)
                {
                    printf("%s\t| %d\t\t| %s\t| %s\n", threshbyusername_ipc[i].username, threshbyusername_ipc[i].count, u32_time_to_human(threshbyusername_ipc[i].utime), threshbyusername_ipc[i].sid);
                }
        }

    /*** Get "after by source" data ***/

    snprintf(tmp_object_check, sizeof(tmp_object_check) - 1, "%s%s", SHM_LOCATION, AFTER_BY_SRC_IPC_FILE);
    object_check(tmp_object_check);

    if ((shm = shm_open(AFTER_BY_SRC_IPC_FILE, (O_CREAT | O_RDWR), (S_IREAD | S_IWRITE))) < 0 )
        {
            fprintf(stderr, "[%s, line %d] Cannot shm_open() (%s)\n", __FILE__, __LINE__, strerror(errno));
            exit(1);
        }

    ftruncate(shm, sizeof(after_by_src_ipc));

    if (( afterbysrc_ipc = mmap(0, sizeof(after_by_src_ipc) + (sizeof(after_by_src_ipc) +  counters_ipc->after_count_by_src ) , (PROT_READ | PROT_WRITE), MAP_SHARED, shm, 0)) == MAP_FAILED )
        {
            fprintf(stderr, "[%s, line %d] Error allocating memory object! [%s]\n", __FILE__, __LINE__, strerror(errno));
            exit(1);
        }

    close(shm);

    if ( counters_ipc->after_count_by_src >= 1 )
        {

            printf("\n                        ***  After by source ***\n");
            printf("-----------------------------------------------------------------------------\n");
            printf("SRC IP\t\t| Count\t\t| Date added/modified\t| Signature ID\n");
            printf("-----------------------------------------------------------------------------\n");

            for ( i = 0; i < counters_ipc->after_count_by_src; i++)
                {
                    ip_addr_src.s_addr = htonl(afterbysrc_ipc[i].ipsrc);
                    printf("%s\t| %d\t\t| %s\t| %s\n", inet_ntoa(ip_addr_src), afterbysrc_ipc[i].count, u32_time_to_human(afterbysrc_ipc[i].utime), afterbysrc_ipc[i].sid);
                }
        }


    /*** Get "After by destination" data ***/

    snprintf(tmp_object_check, sizeof(tmp_object_check) - 1, "%s%s", SHM_LOCATION, AFTER_BY_DST_IPC_FILE);
    object_check(tmp_object_check);

    if ((shm = shm_open(AFTER_BY_DST_IPC_FILE, (O_CREAT | O_RDWR), (S_IREAD | S_IWRITE))) < 0 )
        {
            fprintf(stderr, "[%s, line %d] Cannot shm_open() (%s)\n", __FILE__, __LINE__, strerror(errno));
            exit(1);
        }

    ftruncate(shm, sizeof(after_by_dst_ipc));

    if (( afterbydst_ipc = mmap(0, sizeof(after_by_dst_ipc) + (sizeof(after_by_dst_ipc) *counters_ipc->after_count_by_dst ) , (PROT_READ | PROT_WRITE), MAP_SHARED, shm, 0)) == MAP_FAILED )
        {
            fprintf(stderr, "[%s, line %d] Error allocating memory object! [%s]\n", __FILE__, __LINE__, strerror(errno));
            exit(1);
        }

    close(shm);

    if ( counters_ipc->after_count_by_dst >= 1 )
        {

            printf("\n                     ***  After by destination ***\n");
            printf("-----------------------------------------------------------------------------\n");
            printf("DST IP\t\t| Count\t\t| Date added/modified\t| Signature ID\n");
            printf("-----------------------------------------------------------------------------\n");

            for ( i = 0; i < counters_ipc->after_count_by_dst; i++)
                {
                    ip_addr_src.s_addr = htonl(afterbydst_ipc[i].ipdst);
                    printf("%s\t| %d\t\t| %s\t| %s\n", inet_ntoa(ip_addr_src), afterbydst_ipc[i].count, u32_time_to_human(afterbydst_ipc[i].utime), afterbydst_ipc[i].sid);
                }
        }

    /*** Get "after by username" data ***/

    snprintf(tmp_object_check, sizeof(tmp_object_check) - 1, "%s%s", SHM_LOCATION, AFTER_BY_USERNAME_IPC_FILE);
    object_check(tmp_object_check);

    if ((shm = shm_open(AFTER_BY_USERNAME_IPC_FILE, (O_CREAT | O_RDWR), (S_IREAD | S_IWRITE))) < 0 )
        {
            fprintf(stderr, "[%s, line %d] Cannot shm_open() (%s)\n", __FILE__, __LINE__, strerror(errno));
            exit(1);
        }

    ftruncate(shm, sizeof(after_by_username_ipc));

    if (( afterbyusername_ipc = mmap(0, sizeof(after_by_username_ipc) + (sizeof(after_by_username_ipc) * counters_ipc->after_count_by_username ) , (PROT_READ | PROT_WRITE), MAP_SHARED, shm, 0)) == MAP_FAILED )
        {
            fprintf(stderr, "[%s, line %d] Error allocating memory object! [%s]\n", __FILE__, __LINE__, strerror(errno));
            exit(1);
        }

    close(shm);


    if ( counters_ipc->after_count_by_username >= 1 )
        {

            printf("\n                        ***  After by username ***\n");
            printf("-----------------------------------------------------------------------------\n");
            printf("Username\t\t| Count\t\t| Date added/modified\t| Signature ID\n");
            printf("-----------------------------------------------------------------------------\n");


            for ( i = 0; i < counters_ipc->after_count_by_username; i++)
                {
                    printf("%s\t| %d\t\t| %s\t| %s\n", afterbyusername_ipc[i].username, afterbyusername_ipc[i].count, u32_time_to_human(afterbyusername_ipc[i].utime), afterbyusername_ipc[i].sid);
                }
        }

    /*** Get "flowbit" data ***/

    snprintf(tmp_object_check, sizeof(tmp_object_check) - 1, "%s%s", SHM_LOCATION, FLOWBIT_IPC_FILE);
    object_check(tmp_object_check);

    if ((shm = shm_open(FLOWBIT_IPC_FILE, (O_CREAT | O_RDWR), (S_IREAD | S_IWRITE))) < 0 )
        {
            fprintf(stderr, "[%s, line %d] Cannot shm_open() (%s)\n", __FILE__, __LINE__, strerror(errno));
            exit(1);
        }

    ftruncate(shm, sizeof(_Sagan_IPC_Flowbit));

    if (( flowbit_ipc = mmap(0, sizeof(_Sagan_IPC_Flowbit) + (sizeof(_Sagan_IPC_Flowbit) * counters_ipc->flowbit_count ) , (PROT_READ | PROT_WRITE), MAP_SHARED, shm, 0)) == MAP_FAILED )
        {
            fprintf(stderr, "[%s, line %d] Error allocating memory object! [%s]\n", __FILE__, __LINE__, strerror(errno));
            exit(1);
        }

    close(shm);


    if ( counters_ipc->flowbit_count >= 1 )
        {

            printf("\nFlowbits.\n");
            printf("-----------------------------------------------------------------------------\n");


            for ( i = 0; i < counters_ipc->flowbit_count; i++)
                {

                    ip_addr_src.s_addr = htonl(flowbit_ipc[i].ip_src);
                    ip_addr_dst.s_addr = htonl(flowbit_ipc[i].ip_dst);

                    if ( flowbit_ipc[i].flowbit_state == 1 )
                        {
                            printf("%s [%d] | Src: %s | Dst: %s | %s\n", flowbit_ipc[i].flowbit_name, flowbit_ipc[i].flowbit_state, inet_ntoa(ip_addr_src), inet_ntoa(ip_addr_dst), u32_time_to_human(flowbit_ipc[i].flowbit_expire));
                        }


                }
        }

    return(0);		/* Clean exit */

}



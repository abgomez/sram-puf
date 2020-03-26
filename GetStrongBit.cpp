/*
* Created by Abel Gomez 03/15/2020
* Adapted from Ade Setyawan code, available at: 
* https://github.com/Tribler/software-based-PUF
*/
#include <unistd.h>
#include <string.h>
#include "PUF/sram.h"

#define GOAL 2331
#define MAX_PAGE 1024

uint8_t buff[32];
long ones_count = 0;
long zeros_count = 0;
long step_delay = 50000;
long initial_delay = 330000;
uint8_t bits[32 * MAX_PAGE];
int strong_ones[MAX_PAGE*32*8];
int strong_zeros[MAX_PAGE*32*8];
int strongest_ones[MAX_PAGE*32*8];
int strongest_zeros[MAX_PAGE*32*8];

SRAM sram;

void write_pages(bool write_ones) {
    for (int page = 0; page < MAX_PAGE; page++) {
        sram.write_page(page, write_ones);
    }
}

void read_pages() {
    for (int page = 0; page < MAX_PAGE; page++) {
        sram.read_page(page, buff);
        memcpy(&bits[page*32], buff, sizeof(buff));
    }
}

//data remanence algorithm to identify strong cells in the SRAM
int data_remanence(bool write_ones, long delay) {
    int strong_bit = 0;
    int bit_position = 0;

    sram.turn_on();
    write_pages(write_ones);
    sram.turn_off();

    usleep(delay);

    sram.turn_on();
    read_pages();
    sram.turn_off();

    for (int i = 0; i < sizeof(bits); i++) {
        for (int j = 7; j >= 0; j--) {
            if ( write_ones == true ) {
                if ( ( (bits[i] >> j) & 1 ) == 0 ) {
                    strong_zeros[strong_bit] = bit_position;
                    strong_bit++;
                }
            }
            else {
                if ( ( (bits[i] >> j) & 1 ) == 1 ) {
                    strong_ones[strong_bit] = bit_position;
                    strong_bit++;
                }
            }
            bit_position++;
        }
    }
    return strong_bit;
}

int get_strong_bits_by_goals(bool write_ones) {
    int current_goal = 0;
    long current_delay = initial_delay;

    while (current_goal < GOAL) {
        current_goal = data_remanence(write_ones, current_delay);
        printf("DATA REMANENCE: %d delay: %ld\n", current_goal, current_delay);

        if (current_goal < GOAL) {
            current_delay += step_delay;
        }
    }
    return current_goal;
}

void get_strong_bits() {
    bool write_ones = true;
    zeros_count = get_strong_bits_by_goals(write_ones);
    write_ones = false;
    ones_count = get_strong_bits_by_goals(write_ones);
    // printf("Zeros: %d\n", zeros_count);
    // printf("Ones: %d\n", ones_count);
}

void get_data_remanence() {
    long current_delay = initial_delay;
    bool write_ones = true;
    FILE *fp;

    // fp = fopen("dataRemanenceZeros.txt", "w+");
    // while (current_delay < 1000000) {
    //     zeros_count = data_remanence(write_ones, current_delay);
    //     printf("DATA REMANENCE: %d delay: %ld\n", zeros_count, current_delay);
    //     fprintf(fp, "DATA REMANENCE: %d delay: %ld\n", zeros_count, current_delay);
    //     current_delay += 50000;
    // }

    write_ones = false;
    fp = fopen("dataRemanenceOnes.txt", "w+");
    while (current_delay < 1000000) {
        ones_count = data_remanence(write_ones, current_delay);
        printf("DATA REMANENCE: %d delay: %ld\n", ones_count, current_delay);
        fprintf(fp, "DATA REMANENCE: %d delay: %ld\n", ones_count, current_delay);
        current_delay += 30000;
    }
}

int readBit(long location) {
    uint8_t recv_data = 0x00;
    long loc = 0;
    loc = floor(location / 8);
    sram.turn_on();
    recv_data = sram.read_byte(loc);
    sram.turn_off();
    // printf("%d\n", (recv_data >> 7) & 1);
    // printf("%d\n", (recv_data >> (7 - (location % 8))) & 1);
    // printf("bits\n");
    // for (int i=7; i>=0; i--) {
    //     printf("%d ", (recv_data >> i) & 1); 
    // }
    return recv_data >> (7 - (location % 8)) & 1;
}

void get_strongest_zero() {
    FILE *fp;
    int count, strongest_count = 0;

    printf("Strong zeros: %d\n", zeros_count);
    for (int i=0; i < zeros_count; i++) {
        if ( readBit(strong_zeros[i]) == 1) {
            // printf("Error at location: %ld\n", strong_zeros[i]);
            count++;
            // exit(1);
        }
        else {
            strongest_zeros[strongest_count] = strong_zeros[i];
            strongest_count++;
        }
    }
    printf("Strong zeros error: %d\n", count);

    printf("Strongest zeros: %d\n", strongest_count);
    fp = fopen("strongestZeros.txt", "w+");
    count = 0;
    for (int i=0; i < strongest_count; i++) {
        if ( readBit(strongest_zeros[i]) == 1) {
            // printf("Error at location: %ld\n", strong_zeros[i]);
            count++;
            // exit(1);
        }
        else {
            fprintf(fp, "%d,", strongest_zeros[i]);
        }
    }
    printf("Strongest zeros error: %d\n", count);
    fclose(fp);
}

void get_strongest_one() {
    FILE *fp;
    int count, strongest_count = 0;

    printf("Strong ones: %d\n", ones_count);
    count, strongest_count = 0;
    for (int i=0; i < ones_count; i++) {
        if ( readBit(strong_ones[i]) == 0) {
            // printf("Error at location: %ld\n", strong_zeros[i]);
            count++;
            // exit(1);
        }
        else {
            strongest_ones[strongest_count] = strong_ones[i];
            strongest_count++;
        }
    }
    printf("Strong ones error: %d\n", count);

    fp = fopen("strongestOnes.txt", "w+");
    count = 0;
    for (int i=0; i < strongest_count; i++) {
        if ( readBit(strongest_ones[i]) == 0) {
            // printf("Error at location: %ld\n", strong_zeros[i]);
            count++;
            // exit(1);
        }
        else {
            fprintf(fp, "%d,", strongest_ones[i]);
        }
    }
    printf("Strongest ones error: %d\n", count);
    fclose(fp);
}

int main(int argc, char **argv){
    bool write_ones = true;
    memset(strong_ones, 0, sizeof(strong_ones));
    memset(strong_zeros, 0, sizeof(strong_zeros));
    memset(strongest_zeros, 0, sizeof(strongest_zeros));

    // get_data_remanence();

    // get strong bits
    // zeros_count = get_strong_bits_by_goals(write_ones);
    // write_ones = false;
    // ones_count = get_strong_bits_by_goals(write_ones);
    // printf("--\n");

    // // get strongest ones
    // get_strongest_zero();
    // printf("--\n");
    // get_strongest_one();

    printf("%d ", readBit(47142));
    printf("%d ", readBit(60389));
    printf("%d ", readBit(34854));
    printf("%d ", readBit(96409));
    printf("%d ", readBit(38));
    printf("%d \n", readBit(195639));

    printf("%d ", readBit(48287));
    printf("%d ", readBit(262114));
    printf("%d ", readBit(234649));
    printf("%d ", readBit(22715));
    printf("%d ", readBit(136327));
    printf("%d \n", readBit(135));


    ////////////////////////////////////////////////////////////////////////////
    // check if its working
    // int wrong_count = 0;
    // sram.turn_on();
    // for (int page = 0; page < MAX_PAGE; page++) {
    //     sram.write_page(page, true);

    //     sram.read_page(page, buff);
    //     memcpy(&bits[page*32], buff, sizeof(buff));
    // }

    // for (int i = 0; i < sizeof(bits); i++) {
    //     for (int j = 7; j >= 0; j--) {
    //         // printf("%d ", (bits[i] >> j) & 1); 
    //         if ( ( (bits[i] >> j) & 1 ) == 0 ) {
    //             wrong_count++;
    //         }
    //     }
    // }

    // printf("Wrong Count: %d\n", wrong_count);
    // sram.turn_off();
    ////////////////////////////////////////////////////////////////////////////

    return 0;
}
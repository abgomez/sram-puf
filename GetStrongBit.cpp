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

int strong_ones[MAX_PAGE*32*8];
int strong_zeros[MAX_PAGE*32*8];

// uint8_t strong_bits[GOAL];
long initial_delay = 330000;
// long initial_delay = 1;
// long initial_delay = 10980000;
long step_delay = 50000;
// long step_delay = 1;
uint8_t bits[32 * MAX_PAGE];
uint8_t buff[32];
long ones_count = 0;
long zeros_count = 0;


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

int data_remanence(bool write_ones, long delay) {
    // uint8_t send_data = 0xFF;
    // uint8_t recv_data = 0x00;
    int count = 0;
    int bit_pos = 0;
    int strong_bit_idx = 0;

    // sram.turn_on();
    // for (long address = 0; address < 1/*131072*/; address++) {
    //     sram.write_byte(address, send_data);
    // }
    // sram.turn_off();

    // usleep(delay);

    // sram.turn_on();
    // for (long address = 0; address < 1/*131072*/; address++) {
    //     recv_data = sram.read_byte(address);
    //     for (int i=7; i>=0; i--) {
    //         if ( ((recv_data >> i) & 1) == 0 ) {
    //             count++;
    //         }
    //         printf("%d ", (recv_data >> i) & 1); 
    //     }
    //     // printf("\n");
    // }
    // sram.turn_off();
    // return count;
    // printf("strong zero: %d\n", count);

    // int ones = 0;
    // int zeroes = 0;
    // int pos = 0;


    sram.turn_on();
    write_pages(write_ones);
    sram.turn_off();

    usleep(delay);

    sram.turn_on();
    read_pages();
    sram.turn_off();

    for (int i = 0; i < sizeof(bits); i++) {
        for (int j = 7; j >= 0; j--) {
            // printf("%d ", (bits[i] >> j) & 1); 
            // if (((bits[i] >> j) & 1) == 0) {
            //     count++;
            // }
            if ( write_ones == true ) {
                if ( ( (bits[i] >> j) & 1 ) == 0 ) {
                    count++;
                    strong_zeros[strong_bit_idx] = bit_pos;
                    strong_bit_idx++;
                }
            }
            else {
                if ( ( (bits[i] >> j) & 1 ) == 1 ) {
                    count++;
                    strong_ones[strong_bit_idx] = bit_pos;
                    strong_bit_idx++;
                }
            }
            bit_pos++;
        }
    }
    return count;
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
    // write_ones = false;
    // ones_count = get_strong_cells_by_goals(write_ones);
    // printf("Zeros: %d\n", zeros_count);
    // printf("Ones: %d\n", ones_count);
}

int readBit(long location) {
    uint8_t recv_data = 0x00;
    long loc = 0;
    loc = floor(location / 8);
    sram.turn_on();
    recv_data = sram.read_byte(loc);
    sram.turn_off();
    printf("%d\n", (recv_data >> 7) & 1);
    printf("%d\n", (recv_data >> (7 - (location % 8))) & 1);
    printf("bits\n");
    for (int i=7; i>=0; i--) {
        printf("%d ", (recv_data >> i) & 1); 
    }
    return recv_data >> (7 - (location % 8)) & 0x1 == 1;
}

void get_data_remanence() {
    long current_delay = 300000;
    bool write_ones = false;
    FILE *fp;

    // fp = fopen("dataRemanenceOnes.txt", "w+");

    while (current_delay < 1000000) {
        zeros_count = data_remanence(write_ones, current_delay);
        printf("DATA REMANENCE: %d delay: %ld\n", zeros_count, current_delay);
        // fprintf(fp, "DATA REMANENCE: %d delay: %ld\n", zeros_count, current_delay);
        current_delay += 50000;
    }
    

    // write_ones = false;
    // ones_count = data_remanence(write_ones);
}

int main(int argc, char **argv){
    memset(strong_ones, 0, sizeof(strong_ones));
    memset(strong_zeros, 0, sizeof(strong_zeros));

    get_data_remanence();

    // get_strong_bits();

    // long location = 0;
    // long location_floor = 0;
    // location = strong_zeros[234];
    // int bit_value = 0;
    // double loc = location / 8;
    // location_floor = floor(location / 8);
    // printf("lo %d\n", location);
    // printf("%.8lf\n", loc);
    // printf("%ld\n", location_floor);

    // if (readBit(location) == 1) {
    //     printf("\n 1 \n");
    // }
    // else {
    //     printf("\n 0 \n");
    // }

    // for (int i=7; i>=0; i--) {
    //     printf("%d ", (recv_data >> i) & 1); 
    // }
    // printf("\n");
    // printf("%d\n", strong_ones[553]);
    // printf("%d\n", strong_zeros[234]);
    // for (int i=0; i<ones_count; i++) {
    //     printf("%d ", strong_ones[i]);
    // }
    // return 1;

    // // long address = 4261413375;
    // uint8_t send_data = 0xFF;
    // // uint8_t recv_data = 0x00;
    // // int count = 0;
    // sram.turn_on();
    // for (long address = 0; address < 1024; address++) {
    //     sram.write_byte(address, send_data);
    // }
    // sram.turn_off();

    // // sleep(current_delay + step_delay);

    // sram.turn_on();
    // for (long address = 0; address < 1024; address++) {
    //     recv_data = sram.read_byte(address);
    //     for (int i=7; i>=0; i--) {
    //         if ( ((recv_data >> i) & 1) == 0 ) {
    //             count++;
    //         }
    //         printf("%d ", (recv_data >> i) & 1); 
    //     }
    //     printf("\n");
    // }
    // sram.turn_off();
    // printf("strong zero: %d\n", count);
    // printf("%u\n", recv_data);
    
    // printf("%02x\n", send_data);
    // for (int i=7; i>=0; i--) {
    //     printf("%d ", (recv_data >> i) & 1); 
    // }
    // printf("\n");
    // printf("\n%d ", (send_data >> 7) & 1);
    // printf("%d ", (send_data >> 6) & 1);
    // printf("%d ", (send_data >> 5) & 1);
    // printf("%d \n", (send_data >> 4) & 1);
    // printf("%d ", (send_data >> 3) & 1);
    // printf("%d ", (send_data >> 2) & 1);
    // printf("%d ", (send_data >> 1) & 1);
    // printf("%d \n", (send_data) & 1);

    // if ( ((send_data >> 7) & 1) == 0) {
    //     printf("zero");
    // }

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
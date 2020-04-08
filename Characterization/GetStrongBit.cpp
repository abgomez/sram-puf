/*
* Created by Abel Gomez 03/15/2020
* Adapted from Ade Setyawan code, available at: 
* https://github.com/Tribler/software-based-PUF
*/
#include <unistd.h>
#include <string.h>
#include <limits.h>
#include <time.h>
#include "../PUF/sram.h"

#define GOAL USHRT_MAX
#define MAX_PAGE 1024


long step_delay = 50000;
long initial_delay = 330000;

uint8_t buff[32];
uint8_t bits[32 * MAX_PAGE];

uint16_t ones_count = 0;
uint16_t zeros_count = 0;
uint16_t strong_ones[GOAL];
uint16_t strong_zeros[GOAL];
uint16_t strongest_ones[GOAL];
uint16_t strongest_zeros[GOAL];
uint16_t strongest_bit[GOAL];

SRAM sram;

/* Functions prototypes */
void get_zeros();
void read_pages();
void verify_sram();
void write_pages(bool);
void get_data_remanence();
void get_strongest_zero();

int  readBit(uint16_t);
int  data_remanence(bool, long);


void verify_sram() {
    int wrong_count = 0;
    sram.turn_on();
    for (int page = 0; page < MAX_PAGE; page++) {
        sram.write_page(page, true);

        sram.read_page(page, buff);
        memcpy(&bits[page*32], buff, sizeof(buff));
    }

    for (int i = 0; i < sizeof(bits); i++) {
        for (int j = 7; j >= 0; j--) {
            // printf("%d ", (bits[i] >> j) & 1); 
            if ( ( (bits[i] >> j) & 1 ) == 0 ) {
                wrong_count++;
            }
        }
    }

    printf("Wrong Count: %d\n", wrong_count);
    sram.turn_off();
}


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
        if (bit_position >= USHRT_MAX) {
            break;
        }
    }
    return strong_bit;
}

void get_data_remanence() {
    long current_delay = initial_delay;
    bool write_ones = true;
    int count = 0;
    FILE *fp;

    // fp = fopen("dataRemanenceZeros.txt", "w+");
    // while (current_delay < 1000000) {
    //     count = data_remanence(write_ones, current_delay);
    //     printf("DATA REMANENCE: %d delay: %ld\n", count, current_delay);
    //     fprintf(fp, "DATA REMANENCE: %d delay: %ld\n", count, current_delay);
    //     current_delay += 50000;
    // }

    write_ones = false;
    fp = fopen("dataRemanenceOnes.txt", "w+");
    while (current_delay < 1000000) {
        count = data_remanence(write_ones, current_delay);
        printf("DATA REMANENCE: %d delay: %ld\n", count, current_delay);
        fprintf(fp, "DATA REMANENCE: %d delay: %ld\n", count, current_delay);
        current_delay += 30000;
    }
}

int readBit(uint16_t location) {
    uint8_t recv_data = 0x00;
    long loc = 0;
    loc = floor(location / 8);
    sram.turn_on();
    recv_data = sram.read_byte(loc);
    sram.turn_off();
    return recv_data >> (7 - (location % 8)) & 1;
}

void get_zeros() {
    bool write_ones = true;
    long delay = 350000;
    uint16_t current_goal = 0;

    while (true) {
        memset(strong_zeros, 0, sizeof(strong_zeros));
        memset(strongest_zeros, 0, sizeof(strongest_zeros));

        // get strong bits
        printf("Get Strongest Zeros\n");
        zeros_count = data_remanence(write_ones, delay);
        printf("DATA REMANENCE: %d delay: %ld\n", zeros_count, delay);
        int one = 0;
        int zero = 0;
        for (int i = 0; i < zeros_count; i++) {
            // printf("%d ", readBit(strong_zeros[i]));
            if (readBit(strong_zeros[i]) == 1) {
                one++;
            }
            else {
                strongest_zeros[zero] = strong_zeros[i];
                zero++;
            }
        }
        printf("  ones: %d, zeros: %d, total: %d", one, zero, one+zero);
        zeros_count = zero;
        printf("\n");

        int error = 0;
        for (int i = 0; i < 1000; i++) {
            if (readBit(strongest_zeros[i]) == 1) {
                error++;
            }
        }
        if (error == 0) {
            printf("Strongest Zeros Error: %d", error);
            printf("\n\n");
            break;
        }
    }
}

void get_ones() {
    bool write_ones = false;
    long delay = 350000;
    uint16_t current_goal = 0;

    while (true) {
        memset(strong_ones, 0, sizeof(strong_ones));
        memset(strongest_ones, 0, sizeof(strongest_zeros));

        // get strong bits
        printf("Get Strongest Ones\n");
        ones_count = data_remanence(write_ones, delay);
        printf("DATA REMANENCE: %d delay: %ld\n", ones_count, delay);
        int one = 0;
        int zero = 0;
        for (int i = 0; i < ones_count; i++) {
            // printf("%d ", readBit(strong_ones[i]));
            if (readBit(strong_ones[i]) == 0) {
                zero++;
            }
            else {
                strongest_ones[one] = strong_ones[i];
                one++;
            }
        }
        printf("  ones: %d, zeros: %d, total: %d", one, zero, one+zero);
        ones_count = one;
        printf("\n");

        int error = 0;
        for (int i = 0; i < 1000; i++) {
            if (readBit(strongest_ones[i]) == 0) {
                error++;
            }
        }
        if (error == 0) {
            printf("Strongest Ones Error: %d", error);
            printf("\n\n");
            break;
        }
        else {
            printf("Strongest Ones Error: %d\n", error);
        }
    }
}

void swap(uint16_t *a, uint16_t *b) {
    uint16_t temp = *a;
    *a = *b;
    *b = temp;
}

void write_strong_bit() {
    FILE *fp;
    uint16_t strongest_bit_count = 0;
    memset(strongest_bit , 0, sizeof(strongest_bit));

    strongest_bit_count = zeros_count + ones_count;
    printf("strongest bits: %d\n", strongest_bit_count);
    memcpy(strongest_bit, strongest_zeros, zeros_count*sizeof(uint16_t));
    memcpy(&strongest_bit[zeros_count], strongest_ones, ones_count*sizeof(uint16_t));

    fp = fopen("strongestBit", "w+");
    // srand(time(NULL));
    for (int i = 0; i < strongest_bit_count; i++) {
        printf("%d ", strongest_bit[i]);
        // int j = rand() % strongest_bit_count;
        // swap(&strongest_bit[i], &strongest_bit[j]);
        fprintf(fp, "%d\n", strongest_bit[i]);
    }
    printf("\n\n");
    fclose(fp);

}

int main(int argc, char **argv){
    // check sram
    // verify_sram();
    // get_data_remanence();

    get_zeros();
    get_ones();
    write_strong_bit();

    return 0;
}
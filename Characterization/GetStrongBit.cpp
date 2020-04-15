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
uint16_t strongest_bit[GOAL];
uint16_t strongest_ones[GOAL];
uint16_t strongest_zeros[GOAL];
// uint16_t strong_ones_count[GOAL];
// uint16_t strong_zeros_count[GOAL];

SRAM sram;

/* Functions prototypes */
void get_one();
void get_zero();
void read_pages();
void verify_sram();
void identify_ones();
void identify_zeros();
void write_pages(bool);
void write_strong_bit();
void get_data_remanence();
void identify_strong_ones();
void identify_strong_zeros();

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
            // printf("%d", (bits[i] >> j) & 1 );
            if ( write_ones == true ) {
                if ( ( (bits[i] >> j) & 1 ) == 0 ) {
                    strong_zeros[strong_bit] = bit_position;
                    // strong_zeros_count[strong_bit] = 0;
                    strong_bit++;
                }
            }
            else {
                if ( ( (bits[i] >> j) & 1 ) == 1 ) {
                    strong_ones[strong_bit] = bit_position;
                    // strong_ones_count[strong_bit] = 0;
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

void identify_zeros() {
    bool write_ones = true;
    long delay = 330000;
    int count = 0;
    FILE *fp;
    
    memset(strong_zeros, 0, sizeof(strong_zeros));
    
    printf("Get Strongest Zeros\n");
    zeros_count = data_remanence(write_ones, delay);
    printf("DATA REMANENCE: %d delay: %ld\n", zeros_count, delay);
    fp = fopen("strongZero", "w+");
    // for (int j = 0; j < 1000; j ++) {
        for (int i = 0; i < zeros_count; i++) {
            // printf("%d ", readBit(strong_zeros[i]));
            if (readBit(strong_zeros[i]) == 0) {
                strong_zeros_count[i]++;
                // printf("%d ", readBit(strong_zeros[i]));
                fprintf(fp, "%d\n", strong_zeros[i]);
            }
        }
    // }
    printf("\n\n");
    fclose(fp);

    // for (int i = 0; i < zeros_count; i++) {
    //     if (strong_zeros_count[i] == 1000) {
    //         // printf("%d ", strong_zeros_count[i]);
    //         strongest_zeros[count] = strong_zeros[i];
    //         count++;
    //     }
    // }
    // printf("count: %d\n", count);
    // zeros_count = count;

    // fp = fopen("strongestZero", "w+");
    // for (int i = 0; i < zeros_count; i++) {
    //     printf("%d ", readBit(strongest_zeros[i]));
    //     fprintf(fp, "%d\n", strongest_zeros[i]);
    // }
    // printf("\n\n");
    // fclose(fp);
}

void get_zero() {
    FILE *fp;
    long loc;
    int index = 0;
    char location[10];

    memset(strong_zeros, 0, sizeof(strong_zeros));

    if ((fp = fopen("strongZero", "r")) == NULL) {
        printf("Error Opening File");
        exit(1);
    }

    while (fgets(location, sizeof(location), fp) != NULL) {
        strong_zeros[index] = atoi(location);
        index++;
    }
    zeros_count = index;
    fclose(fp);
}

void identify_strong_zeros() {
    FILE *fp;
    int flag = 0;

    for (int run = 0; run < 10000; run++) {
        get_zero();
        fp = fopen("strongZero", "w+");
        for (int i = 0; i < zeros_count; i++) {
            if (readBit(strong_zeros[i]) == 0) {
                // printf("%d ", readBit(strong_zeros[i]));
                fprintf(fp, "%d\n", strong_zeros[i]);
            }
        }
        // printf("\n");
        fclose(fp);
        usleep(2000000);
    }
    printf("zeros_count: %d\n", zeros_count);
    for (int i = 0; i < zeros_count; i++) {
        printf("%d ", strong_zeros[i]);
    }
    printf("\n\n");
}

void identify_ones() {
    bool write_ones = false;
    long delay = 340000;
    FILE *fp;
    
    memset(strong_ones, 0, sizeof(strong_ones));
    
    printf("Get Strongest Ones\n");
    ones_count = data_remanence(write_ones, delay);
    printf("DATA REMANENCE: %d delay: %ld\n", ones_count, delay);
    fp = fopen("strongOne", "w+");
    // for (int j = 0; j < 1000; j ++) {
        for (int i = 0; i < ones_count; i++) {
            // printf("%d ", readBit(strong_ones[i]));
            if (readBit(strong_ones[i]) == 1) {
                fprintf(fp, "%d\n", strong_ones[i]);
            }
        }
        printf("\n\n");
        fclose(fp);
    // }
    // int count = 0;
    // for (int i = 0; i < ones_count; i++) {
    //     if (strong_ones_count[i] == 1000) {
    //         // printf("%d ", strong_zeros[i]);
    //         strongest_ones[count] = strong_ones[i];
    //         count++;
    //     }
    // }
    // printf("count: %d\n", count);
    // ones_count = count;

    // fp = fopen("strongestOnes", "w+");
    // for (int i = 0; i < ones_count; i++) {
    //     printf("%d ", readBit(strongest_ones[i]));
    //     fprintf(fp, "%d\n", strongest_ones[i]);
    // }
    // printf("\n\n");
    // fclose(fp);
}

void get_one() {
    FILE *fp;
    long loc;
    int index = 0;
    char location[10];

    memset(strong_ones, 0, sizeof(strong_ones));

    if ((fp = fopen("strongOne", "r")) == NULL) {
        printf("Error Opening File");
        exit(1);
    }

    while (fgets(location, sizeof(location), fp) != NULL) {
        strong_ones[index] = atoi(location);
        index++;
    }
    ones_count = index;
    fclose(fp);
}

void identify_strong_ones() {
    FILE *fp;
    int flag = 0;

    for (int run = 0; run < 10000; run++) {
        get_one();
        fp = fopen("strongOne", "w+");
        for (int i = 0; i < ones_count; i++) {
            if (readBit(strong_ones[i]) == 1) {
                // printf("%d ", readBit(strong_zeros[i]));
                fprintf(fp, "%d\n", strong_ones[i]);
            }
        }
        // printf("\n");
        fclose(fp);
        usleep(2000000);
    }
    printf("ones_count: %d\n", ones_count);
    for (int i = 0; i < ones_count; i++) {
        printf("%d ", strong_ones[i]);
    }
    printf("\n\n");
}

void write_strong_bit() {
    FILE *fp;
    uint16_t strongest_bit_count = 0;
    memset(strongest_bit , 0, sizeof(strongest_bit));

    get_zero();
    get_one();

    strongest_bit_count = zeros_count + ones_count;
    printf("strongest bits: %d\n", strongest_bit_count);
    memcpy(strongest_bit, strong_zeros, zeros_count*sizeof(uint16_t));
    memcpy(&strongest_bit[zeros_count], strong_ones, ones_count*sizeof(uint16_t));

    fp = fopen("strongestBit", "w+");
    // srand(time(NULL));
    for (int i = 0; i < strongest_bit_count; i++) {
        printf("%d ", readBit(strongest_bit[i]));
        // int j = rand() % strongest_bit_count;
        // swap(&strongest_bit[i], &strongest_bit[j]);
        fprintf(fp, "%d\n", strongest_bit[i]);
    }
    printf("\n\n");
    fclose(fp);

}

int main(int argc, char **argv){
    sram = SRAM();
    // check sram
    // verify_sram();
    // get_data_remanence();

    // identify_zeros();
    // identify_strong_zeros();
    // identify_ones();
    // identify_strong_ones();
    write_strong_bit();

    return 0;
}
/*
* Created by Abel Gomez 03/15/2020
* Adapted from Ade Setyawan code, available at: 
* https://github.com/Tribler/software-based-PUF
*/
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <limits.h>
#include <stdio.h>
#include <errno.h>
#include <time.h>
#include <modbus.h>
#include "../PUF/sram.h"

#define GOAL 2331
#define MAX_PAGE 1024
#define CHALLENGE_SIZE MODBUS_MAX_WRITE_REGISTERS
#define MASTER_IP "127.0.0.1"
#define MODBUS_PORT 1502


SRAM sram;
modbus_t *context;
modbus_mapping_t *modbus_mapping;

int socket;
int strongest_one_count;
int strongest_zero_count;

long ones_count = 0;
long zeros_count = 0;
long step_delay = 50000;
long initial_delay = 330000;

// uint16_t *challenge;
// uint16_t *strong_ones;

uint16_t challenge[USHRT_MAX*2];
uint16_t strong_ones[USHRT_MAX];
uint16_t strong_zeros[USHRT_MAX];
uint16_t strongest_ones[USHRT_MAX];
uint16_t strongest_zeros[USHRT_MAX];
uint16_t strongest_ones_tmp[USHRT_MAX];
uint16_t strongest_zeros_tmp[USHRT_MAX];

uint8_t buff[32];
uint8_t bits[32 * MAX_PAGE];

int initialize() {
    /* Allocate and initialize the memory to store the modbus mapping */
    modbus_mapping = modbus_mapping_new(MODBUS_MAX_READ_BITS, 0, MODBUS_MAX_READ_REGISTERS, 0);
    if (modbus_mapping == NULL) {
        fprintf(stderr, "Failed to allocate the mapping: %s\n", modbus_strerror(errno));
        modbus_free(context);
        return -1;
    }

    // /* Allocate and initialize the memory to store the challenge */
    // challenge = (uint16_t *) malloc((MAX_PAGE*32*8) * sizeof(uint16_t));
    // if (challenge == NULL) {
    //     fprintf(stderr, "Failed to allocated memory");
    //     return -1;
    // }
    // memset(challenge, 0, CHALLENGE_SIZE * sizeof(uint16_t));

    //  /* Allocate and initialize the memory to store the challenge */
    // strong_ones = (uint16_t *) malloc(50000 * sizeof(uint16_t));
    // if (strong_ones == NULL) {
    //     fprintf(stderr, "Failed to allocated memory");
    //     return -1;
    // }
    // memset(strong_ones, 0, 50000 * sizeof(uint16_t));

    memset(challenge, 0, sizeof(challenge));
    memset(strong_ones, 0, sizeof(strong_ones));
    memset(strong_zeros, 0, sizeof(strong_zeros));
    memset(strongest_ones, 0, sizeof(strongest_ones));
    memset(strongest_zeros, 0, sizeof(strongest_zeros));
    memset(strongest_ones_tmp, 0, sizeof(strongest_ones_tmp));
    memset(strongest_zeros_tmp, 0, sizeof(strongest_zeros_tmp));

    return 0;
}

uint8_t getMode() {
    uint8_t mode = 0;
    if (modbus_mapping->tab_bits[0] == (uint8_t) 1) {
        mode++;
    }
    if (modbus_mapping->tab_bits[1] == (uint8_t) 1) {
        mode += 2;
    }
    return mode;
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

int get_strong_bit_by_goal(bool write_ones) {
    int current_goal = 0;
    long current_delay = initial_delay;

    while (current_goal < CHALLENGE_SIZE) {
        current_goal = data_remanence(write_ones, current_delay);
        printf("DATA REMANENCE: %d delay: %ld\n", current_goal, current_delay);

        if (current_goal < GOAL) {
            current_delay += step_delay;
        }
    }
    return current_goal;
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

void get_strongest_one() {
    FILE *fp;
    int count;

    printf("Strong ones: %d\n", ones_count);
    count, strongest_one_count = 0;
    for (int i=0; i < ones_count; i++) {
        if ( readBit(strong_ones[i]) == 0) {
            count++;
        }
        else {
            strongest_ones_tmp[strongest_one_count] = strong_ones[i];
            strongest_one_count++;
        }
    }
    printf("Strong ones error: %d\n", count);

    printf("Strongest ones: %d\n", strongest_one_count);
    count = 0;
    for (int i=0; i < strongest_one_count; i++) {
        if ( readBit(strongest_ones_tmp[i]) == 0) {
            count++;
        }
        else {
            strongest_ones[i] = strongest_ones_tmp[i];
        }
    }
    printf("Strongest ones error: %d\n", count);
}

void get_strongest_zero() {
    int count;
    strongest_zero_count = 0;

    printf("Strong zeros: %d\n", zeros_count);
    for (int i=0; i < zeros_count; i++) {
        if ( readBit(strong_zeros[i]) == 1) {
            count++;
        }
        else {
            strongest_zeros_tmp[strongest_zero_count] = strong_zeros[i];
            strongest_zero_count++;
        }
    }
    printf("Strong zeros error: %d\n", count);

    printf("Strongest zeros: %d\n", strongest_zero_count);
    count = 0;
    for (int i=0; i < strongest_zero_count; i++) {
        if ( readBit(strongest_zeros[i]) == 1) {
            count++;
        }
        else {
            strongest_zeros[i] = strongest_zeros_tmp[i];
        }
    }
    printf("Strongest zeros error: %d\n", count);
}

void swap(uint16_t *a, uint16_t *b) {
    uint16_t temp = *a;
    *a = *b;
    *b = temp;
}

void randomize(int n) {
    srand(time(NULL));
    for (int i = 0; i < n; i++) {
        int j = rand() % n;
        swap(&challenge[i], &challenge[j]);
    }
}

void createChallenge() {
    int challenge_count = strongest_one_count + strongest_zero_count;

    printf("Challenge bits: %d\n", challenge_count);

    memcpy(challenge, strongest_ones, strongest_one_count*sizeof(uint16_t));
    memcpy(&challenge[strongest_one_count], strongest_zeros, strongest_zero_count*sizeof(uint16_t));

    // for (int i = 0; i < CHALLENGE_SIZE; i++) {
    //     printf("%d ", challenge[i]);
    // }

    randomize(challenge_count);
    // printf("\n\n");
    // for (int i = 0; i < CHALLENGE_SIZE; i++) {
    //     printf("%d ", challenge[i]);
    // }

    // printf("\n");
}


void getChallenge() {
    bool write_ones;

    // get strong bits
    write_ones = false;
    ones_count = get_strong_bit_by_goal(write_ones);
    write_ones = true;
    zeros_count = get_strong_bit_by_goal(write_ones);

    // get strongest bits
    get_strongest_one();
    get_strongest_zero();

    createChallenge();

    printf("\n");
    for (int i = 0; i < CHALLENGE_SIZE; i++) {
        modbus_mapping->tab_registers[i] = challenge[i];
        printf("%d ", challenge[i]);
    }
    printf("\n\n");

    // for (int i = 0; i < CHALLENGE_SIZE; i++) {
    //     printf("%ld ", strongest_zeros[i]);
    // }
    // printf("\n");
}

int main(int argc, char **argv){
    int rc;

    // Initialize connection
    printf("Waiting for connection...\n");
    context = modbus_new_tcp(MASTER_IP, MODBUS_PORT);
    socket = modbus_tcp_listen(context, 1);
    modbus_tcp_accept(context, &socket);
	printf("Connection started!\n");

    if (initialize() != 0) {
        printf("Failed to allocated memory");
        exit(EXIT_FAILURE);
    }

    for(;;) {
        uint8_t query[MODBUS_TCP_MAX_ADU_LENGTH];
        int function_code;
        uint8_t mode;

        rc = modbus_receive(context, query);
        function_code = query[modbus_get_header_length(context)];
        if (rc >= 0) {
            printf("Replying to request: 0x%02X\n", function_code);
            switch (function_code) {
                case 0x10:
                    printf("Write\n");
                    break;
                case 0x3: //Read Registers
                    mode = getMode();
                    printf("Working on mode: %d\n", mode);
                    if (mode == 0) { //get challenge
                        getChallenge();
                    }
                    break;
            }
            modbus_reply(context, query, rc, modbus_mapping);
        } else {
            /* Connection closed by the client or server */
            modbus_close(context); // close
            // immediately start waiting for another request again
            printf("\n\nWaiting for connection...\n");
            modbus_tcp_accept(context, &socket);
        }
    }

    modbus_mapping_free(modbus_mapping);
    close(socket);
    modbus_free(context);

    // get_data_remanence();



    // bool write_ones = true;
    // memset(strong_ones, 0, sizeof(strong_ones));
    // memset(strong_zeros, 0, sizeof(strong_zeros));
    // memset(strongest_zeros, 0, sizeof(strongest_zeros));

    

    // get strong bits
    // zeros_count = get_strong_bits_by_goals(write_ones);
    // write_ones = false;
    // ones_count = get_strong_bits_by_goals(write_ones);
    // printf("--\n");

    // // get strongest ones
    // get_strongest_zero();
    // printf("--\n");
    // get_strongest_one();

    // printf("%d ", readBit(47142));
    // printf("%d ", readBit(60389));
    // printf("%d ", readBit(34854));
    // printf("%d ", readBit(96409));
    // printf("%d ", readBit(38));
    // printf("%d \n", readBit(195639));

    // printf("%d ", readBit(48287));
    // printf("%d ", readBit(262114));
    // printf("%d ", readBit(234649));
    // printf("%d ", readBit(22715));
    // printf("%d ", readBit(136327));
    // printf("%d \n", readBit(135));
    return 0;
}





// #include <stdio.h>
// #include <unistd.h>
// #include <string.h>
// #include <stdlib.h>


// int main(int argc, char *argv[])
// {
    // int socket;
    // modbus_t *ctx;
    // modbus_mapping_t *mb_mapping;
    // int rc;
    // int use_backend;

    

    // mb_mapping = modbus_mapping_new(MODBUS_MAX_READ_BITS, 0, MODBUS_MAX_READ_REGISTERS, 0);
    // if (mb_mapping == NULL) {
    //     fprintf(stderr, "Failed to allocate the mapping: %s\n",
    //             modbus_strerror(errno));
    //     modbus_free(ctx);
    //     return -1;
    // }

    

    // for(;;) {
    //     uint8_t query[MODBUS_TCP_MAX_ADU_LENGTH];
    //     int offset;
    //     int function;

    //     rc = modbus_receive(ctx, query);
    //     // offset = ctx->backend->header_lenght;
    //     function = query[modbus_get_header_length(ctx)];
    //     // printf("0x%X\n\n", function);
    //     // for (int i=0; i <MODBUS_TCP_MAX_ADU_LENGTH; i++){
    //     //     printf("0x%X ", query[i]);
    //     // }
    //     // printf("\n");
    //     if (rc >= 0) {
    //         printf("Replying to request.\n");
    //         switch (function) {
    //             case 0x10:
    //                 printf("Write\n");
    //                 break;
    //             case 0x3:
    //                 printf("Read\n");
    //                 break;
    //         }
            // mb_mapping->tab_registers[0] = 0x2;
            // mb_mapping->tab_registers[1] = 0x41;

            // mb_mapping->tab_bits[0] = (uint8_t) 1;
            // mb_mapping->tab_bits[1] = (uint8_t) 0;
            // for (int i=0; i < 4; i++) {
            //     printf("reg[%d]=%d (%c)\n", i, mb_mapping->tab_registers[i], mb_mapping->tab_registers[i]);
    //         // }
    //         modbus_reply(ctx, query, rc, mb_mapping);
    //     } else {
    //         /* Connection closed by the client or server */
    //         modbus_close(ctx); // close
    //         // immediately start waiting for another request again
    //         printf("Waiting for TCP connection...\n");
    //         modbus_tcp_accept(ctx, &socket);
    //     }
    // }

    // printf("Quit the loop: %s\n", modbus_strerror(errno));

//     modbus_mapping_free(mb_mapping);
//     close(socket);
//     modbus_free(ctx);

//     return 0;
// }
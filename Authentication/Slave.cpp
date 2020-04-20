/*
* Created by Abel Gomez 03/15/2020
* Adapted from Ade Setyawan code, available at: 
* https://github.com/Tribler/software-based-PUF
*/
#include <openssl/engine.h>
#include <openssl/hmac.h>
#include <openssl/evp.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <limits.h>
#include <stdio.h>
#include <errno.h>
#include <time.h>
#include <modbus.h>
#include "../PUF/sram.h"

/*
* Slave - Mode of Operation, 4 bits
*/
#define MODE 4
#define WRITE_H 0b0000
#define NEW_CHALLENGE 0b0001
#define WRITE_CHALLENGE 0b0010
#define FINISH_CHALLENGE 0b0011

#define MAX_PAGE 1024
#define IP "127.0.0.1"
#define MODBUS_PORT 1502
#define CHALLENGE_SIZE 256
#define HASH_LEN 32


SRAM sram;
modbus_t *context;
modbus_mapping_t *modbus_mapping;

int section_no = 0;
int challenge_idx = 0;
int response_idx = 0;
// uint16_t *challenge;
// uint16_t *response;

int socket;
// int strongest_one_count;
// int strongest_zero_count;

long ones_count = 0;
long zeros_count = 0;
long step_delay = 50000;
long initial_delay = 330000;
long strongest_one_count = 0;
long strongest_zero_count = 0;

uint8_t H[HASH_LEN];
uint8_t c2_hash[HASH_LEN];
uint8_t buff[32];
uint8_t bits[32 * MAX_PAGE];

uint16_t c1[CHALLENGE_SIZE];
uint16_t c2[CHALLENGE_SIZE];

uint16_t response[USHRT_MAX];
uint16_t challenge[USHRT_MAX];
uint16_t strong_ones[USHRT_MAX];
uint16_t strong_zeros[USHRT_MAX];
uint16_t strongest_ones[USHRT_MAX];
uint16_t strongest_zeros[USHRT_MAX];
uint16_t strongest_ones_tmp[USHRT_MAX];
uint16_t strongest_zeros_tmp[USHRT_MAX];

uint32_t timestamp = 0;

/*
* Functions Prototype 
*/
uint8_t get_mode();
uint32_t get_sha256(uint8_t *, uint8_t *);
uint32_t get_sha256(uint16_t *, uint8_t *); 
void write_timestamp();


uint32_t get_sha256(uint8_t *message, uint8_t *message_digest) {
    uint32_t hash_len = 0;
    EVP_MD_CTX *mdctx;

    OpenSSL_add_all_digests();
    mdctx = EVP_MD_CTX_create();
    EVP_DigestInit_ex(mdctx, EVP_sha256(), NULL);
    EVP_DigestUpdate(mdctx, message, sizeof(&message));
    EVP_DigestFinal_ex(mdctx, message_digest, &hash_len);
    EVP_MD_CTX_destroy(mdctx);

    return hash_len;
}

uint32_t get_sha256(uint16_t *message, uint8_t *message_digest) {
    uint32_t hash_len = 0;
    EVP_MD_CTX *mdctx;

    OpenSSL_add_all_digests();
    mdctx = EVP_MD_CTX_create();
    EVP_DigestInit_ex(mdctx, EVP_sha256(), NULL);
    EVP_DigestUpdate(mdctx, message, sizeof(&message));
    EVP_DigestFinal_ex(mdctx, message_digest, &hash_len);
    EVP_MD_CTX_destroy(mdctx);

    return hash_len;
}


int initialize() {
    /* Allocate and initialize the memory to store the modbus mapping */
    modbus_mapping = modbus_mapping_new(MODBUS_MAX_READ_BITS, 0, MODBUS_MAX_READ_REGISTERS, 0);
    if (modbus_mapping == NULL) {
        fprintf(stderr, "Failed to allocate the mapping: %s\n", modbus_strerror(errno));
        modbus_free(context);
        return -1;
    }

    // /* Allocate and initialize the memory to store the challenge */
    // challenge = (uint16_t *) malloc(USHRT_MAX * sizeof(uint16_t));
    // if (challenge == NULL) {
    //     fprintf(stderr, "Failed to allocated memory");
    //     return -1;
    // }
    // memset(challenge, 0, USHRT_MAX * sizeof(uint16_t));

    // /* Allocate and initialize the memory to store the challenge */
    // response = (uint16_t *) malloc(USHRT_MAX * sizeof(uint16_t));
    // if (response == NULL) {
    //     fprintf(stderr, "Failed to allocated memory");
    //     return -1;
    // }
    // memset(response, 0, USHRT_MAX * sizeof(uint16_t));

    // /* Allocate and initialize the memory to store strong ones*/
    // strong_ones = (uint16_t *) malloc(challenge_size * sizeof(uint16_t));
    // if (challenge == NULL) {
    //     fprintf(stderr, "Failed to allocated memory");
    //     return -1;
    // }
    // memset(strong_ones, 0, challenge_size * sizeof(uint16_t));

    //  /* Allocate and initialize the memory to store the challenge */
    // strong_ones = (uint16_t *) malloc(50000 * sizeof(uint16_t));
    // if (strong_ones == NULL) {
    //     fprintf(stderr, "Failed to allocated memory");
    //     return -1;
    // }
    // memset(strong_ones, 0, 50000 * sizeof(uint16_t));

    memset(c1, 0, sizeof(c1));
    // memset(strong_ones, 0, sizeof(strong_ones));
    // memset(strong_zeros, 0, sizeof(strong_zeros));
    // memset(strongest_ones, 0, sizeof(strongest_ones));
    // memset(strongest_zeros, 0, sizeof(strongest_zeros));
    // memset(strongest_ones_tmp, 0, sizeof(strongest_ones_tmp));
    // memset(strongest_zeros_tmp, 0, sizeof(strongest_zeros_tmp));

    return 0;
}

uint8_t get_mode() {
    uint8_t mode = 0;

    if (modbus_mapping->tab_bits[0] == (uint8_t) 1) {
        mode +=8;
    }
    if (modbus_mapping->tab_bits[1] == (uint8_t) 1) {
        mode +=4;
    }
    if (modbus_mapping->tab_bits[2] == (uint8_t) 1) {
        mode +=2;
    }
    if (modbus_mapping->tab_bits[3] == (uint8_t) 1) {
        mode ++;
    }

    return mode;
}

// void write_pages(bool write_ones) {
//     for (int page = 0; page < MAX_PAGE; page++) {
//         sram.write_page(page, write_ones);
//     }
// }

// void read_pages() {
//     for (int page = 0; page < MAX_PAGE; page++) {
//         sram.read_page(page, buff);
//         memcpy(&bits[page*32], buff, sizeof(buff));
//     }
// }

//data remanence algorithm to identify strong cells in the SRAM
// int data_remanence(bool write_ones, long delay) {
//     int strong_bit = 0;
//     int bit_position = 0;

//     sram.turn_on();
//     write_pages(write_ones);
//     sram.turn_off();
    
//     usleep(delay);

//     sram.turn_on();
//     read_pages();
//     sram.turn_off();

//     for (int i = 0; i < sizeof(bits); i++) {
//         for (int j = 7; j >= 0; j--) {
//             if ( write_ones == true ) {
//                 if ( ( (bits[i] >> j) & 1 ) == 0 ) {
//                     strong_zeros[strong_bit] = bit_position;
//                     strong_bit++;
//                 }
//             }
//             else {
//                 if ( ( (bits[i] >> j) & 1 ) == 1 ) {
//                     strong_ones[strong_bit] = bit_position;
//                     strong_bit++;
//                 }
//             }
//             bit_position++;
//         }
//         if (bit_position >= USHRT_MAX) {
//             break;
//         }
//     }
//     return strong_bit;
// }

// int get_strong_bit_by_goal(bool write_ones) {
//     int current_goal = 0;
//     long current_delay = initial_delay;

//     while (current_goal < CHALLENGE_SIZE) {
//         current_goal = data_remanence(write_ones, current_delay);
//         printf("DATA REMANENCE: %d delay: %ld\n", current_goal, current_delay);

//         if (current_goal < CHALLENGE_SIZE) {
//             current_delay += step_delay;
//         }
//     }
//     return current_goal;
// }

// int readBit(uint16_t location) {
//     uint8_t recv_data = 0x00;
//     long loc = 0;
    
//     loc = floor(location / 8);
//     sram.turn_on();
//     recv_data = sram.read_byte(loc);
//     sram.turn_off();

//     return recv_data >> (7 - (location % 8)) & 1;
// }

int readBit(long location) {
    uint8_t recv_data = 0x00;
    // printf("%d ", location);
    // printf("%d ", floor(location / 8));

    sram.turn_on();
    recv_data = sram.read_byte(floor(location / 8));
    sram.turn_off();
    printf("%d ", recv_data >> (7 - (location % 8)) & 1);
    
    return recv_data >> (7 - (location % 8)) & 1;
}

// void get_strongest_one() {
//     FILE *fp;
//     int count;

//     printf("Strong ones: %d\n", ones_count);
//     count, strongest_one_count = 0;
//     for (int i=0; i < ones_count; i++) {
//         if ( readBit(strong_ones[i]) == 0) {
//             count++;
//         }
//         else {
//             strongest_ones_tmp[strongest_one_count] = strong_ones[i];
//             strongest_one_count++;
//         }
//     }
//     printf("Strong ones error: %d\n", count);

//     printf("Strongest ones: %d\n", strongest_one_count);
//     count = 0;
//     for (int i=0; i < strongest_one_count; i++) {
//         if ( readBit(strongest_ones_tmp[i]) == 0) {
//             count++;
//         }
//         else {
//             strongest_ones[i] = strongest_ones_tmp[i];
//         }
//     }
//     printf("Strongest ones error: %d\n", count);
// }

// void get_strongest_zero() {
//     int count;
//     strongest_zero_count = 0;

//     printf("Strong zeros: %d\n", zeros_count);
//     for (int i=0; i < zeros_count; i++) {
//         if ( readBit(strong_zeros[i]) == 1) {
//             count++;
//         }
//         else {
//             strongest_zeros_tmp[strongest_zero_count] = strong_zeros[i];
//             strongest_zero_count++;
//         }
//     }
//     printf("Strong zeros error: %d\n", count);

//     printf("Strongest zeros: %d\n", strongest_zero_count);
//     count = 0;
//     for (int i=0; i < strongest_zero_count; i++) {
//         if ( readBit(strongest_zeros[i]) == 1) {
//             count++;
//         }
//         else {
//             strongest_zeros[i] = strongest_zeros_tmp[i];
//         }
//     }
//     printf("Strongest zeros error: %d\n", count);
// }

// void swap(uint16_t *a, uint16_t *b) {
//     uint16_t temp = *a;
//     *a = *b;
//     *b = temp;
// }

// void randomize(int n) {
//     srand(time(NULL));
//     for (int i = 0; i < n; i++) {
//         int j = rand() % n;
//         swap(&challenge[i], &challenge[j]);
//     }
// }

// void createChallenge() {
//     int challenge_count = strongest_one_count + strongest_zero_count;
    
//     memset(challenge, 0, sizeof(challenge));

//     printf("Challenge bits: %d\n", challenge_count);

//     memcpy(challenge, strongest_ones, strongest_one_count*sizeof(uint16_t));
//     memcpy(&challenge[strongest_one_count], strongest_zeros, strongest_zero_count*sizeof(uint16_t));

//     // for (int i = 0; i < strongest_one_count; i++) {
//     //     printf("%d ", strongest_ones[i]);
//     // }
//     // printf("\n\n");

//     // for (int i = 0; i < challenge_count; i++) {
//     //     printf("%d ", challenge[i]);
//     // }
//     // printf("\n\n");

//     randomize(challenge_count);
//     // printf("\n\n");
//     // for (int i = 0; i < CHALLENGE_SIZE; i++) {
//     //     printf("%d ", challenge[i]);
//     // }

//     // printf("\n");
// }


// void getChallenge() {
//     bool write_ones;

//     memset(strong_ones, 0, sizeof(strong_ones));
//     memset(strong_zeros, 0, sizeof(strong_zeros));
//     memset(strongest_ones, 0, sizeof(strongest_ones));
//     memset(strongest_zeros, 0, sizeof(strongest_zeros));
//     memset(strongest_ones_tmp, 0, sizeof(strongest_ones_tmp));
//     memset(strongest_zeros_tmp, 0, sizeof(strongest_zeros_tmp));

//     // get strong bits
//     write_ones = false;
//     ones_count = get_strong_bit_by_goal(write_ones);
//     write_ones = true;
//     zeros_count = get_strong_bit_by_goal(write_ones);

//     // get strongest bits
//     get_strongest_one();
//     get_strongest_zero();

//     createChallenge();

//     for (int i = 0; i < MODBUS_MAX_READ_REGISTERS; i++) {
//         modbus_mapping->tab_registers[i] = challenge[i];
//         printf("%ld ", challenge[i]);
//     }
//     printf("\n\n");

//     // for (int i = 0; i < CHALLENGE_SIZE; i++) {
//     //     printf("%ld ", strongest_zeros[i]);
//     // }
//     // printf("\n");
// }

// void getResponse() {
//     memset(response, 0, sizeof(response));
//     for (int i=0; i < MODBUS_MAX_WRITE_REGISTERS; i++) {
//         // challenge[i] = modbus_mapping->tab_registers[i];
//         response[i] = readBit(modbus_mapping->tab_registers[i]);
//         modbus_mapping->tab_registers[i] = response[i];
//         printf("%d ", response[i]);
//     }
//     printf("\n\n");
// }

void get_response() {
    for (int i = 0; i < MODBUS_MAX_READ_REGISTERS; i++) {
        int j = response_idx * MODBUS_MAX_READ_REGISTERS + i;
        if (j > CHALLENGE_SIZE) {
            response[j] = 0;
        }
        else {
            response[j] = readBit(challenge[j]);
        }
        modbus_mapping->tab_registers[i] = response[j];
    }
    
    // for (int i = 0 ; i < MODBUS_MAX_WRITE_REGISTERS; i++) {
    //     // printf("%d ", readBit(challenge[i]));
    //     modbus_mapping->tab_registers[i] = readBit(challenge[i]);
    //     usleep(330000);
    // }
    for (int i = 0; i < CHALLENGE_SIZE; i++) {
        printf("%d ", modbus_mapping->tab_registers[i]);
    }
    printf("\n\n");

    response_idx++;

}

void get_H() {
    memcpy(H, modbus_mapping->tab_registers, HASH_LEN * sizeof(uint8_t));
    for (int i = 0; i < HASH_LEN; i++) {
        printf("%02x", H[i]);
    }
    printf("\n");
}

void write_challenge(uint16_t *challenge) {
    int remainder_bit = 0;

    section_no = (int) ceil(CHALLENGE_SIZE/MODBUS_MAX_WRITE_REGISTERS);

    if (challenge_idx == section_no) {
        remainder_bit = CHALLENGE_SIZE - MODBUS_MAX_WRITE_REGISTERS * (int) ceil(CHALLENGE_SIZE/MODBUS_MAX_WRITE_REGISTERS);
        if (remainder_bit > 0) {
            memcpy(&challenge[challenge_idx*MODBUS_MAX_WRITE_REGISTERS], modbus_mapping->tab_registers, remainder_bit*sizeof(uint16_t));
        }
    }
    else {
        memcpy(&challenge[challenge_idx*MODBUS_MAX_WRITE_REGISTERS], modbus_mapping->tab_registers, MODBUS_MAX_WRITE_REGISTERS*sizeof(uint16_t));
    }

    for (int i = 0; i < CHALLENGE_SIZE; i++) {
        printf("%d ", challenge[i]);
    }
    printf("\n\n");

    challenge_idx++;
}

void write_timestamp() {
    memcpy(&timestamp, modbus_mapping->tab_registers, 2 * sizeof(uint16_t));
    printf("%d\n", timestamp);
}


void write_register() {
    uint8_t mode = 0;
    mode = get_mode();
    if (mode == 0) {
        get_H();
    }
    else if (mode == 1) {
        printf("Writting C1...\n");
        write_challenge(c1);
    }
    else if (mode == 2) {
        printf("Writting C2..\n");
        write_challenge(c2);
        get_sha256(c2, c2_hash);
        // for (int i = 0; i < 32; i++) {
        //     printf("%02x", c2_hash[i]);
        // }
        // printf("\n");
    }
    else if (mode == 3) {
        printf("Writting timestamp..\n");
        write_timestamp();
    }

}

int main(int argc, char **argv){
    int rc;

    sram = SRAM();

    // Initialize connection
    /* TODO: change communication link and add multiple slaves*/
    printf("Waiting for connection...\n");
    context = modbus_new_tcp(IP, MODBUS_PORT);
    socket = modbus_tcp_listen(context, 1);
    modbus_tcp_accept(context, &socket);
	printf("Connection started!\n");

    // /* Allocate and initialize the memory to store the modbus mapping */
    // modbus_mapping = modbus_mapping_new(MODBUS_MAX_READ_BITS, 0, MODBUS_MAX_READ_REGISTERS, 0);
    // if (modbus_mapping == NULL) {
    //     fprintf(stderr, "Failed to allocate the mapping: %s\n", modbus_strerror(errno));
    //     modbus_free(context);
    //     return -1;
    // }

    if (initialize() != 0) {
        printf("Failed to allocated memory");
        exit(EXIT_FAILURE);
    }

    for(;;) {
        uint8_t query[MODBUS_TCP_MAX_ADU_LENGTH];
        uint8_t function_code;
        uint8_t mode;

        rc = modbus_receive(context, query);
        function_code = query[modbus_get_header_length(context)];
        if (rc >= 0) {
            printf("Replying to request: 0x%02X ", function_code);
            if (function_code == 0x03) {
                get_response();
            }
            // switch (function_code) {
            //     case 0x0F:
            //         printf("(Set Mode)\n");
            //         break;
            //     case 0x06:
            //         printf("(Set Challenge Size)\n");    
            //         break;
            //     case 0x10:
            //         printf("(Write Register)\n");
            //         break;
            //     case 0x3:
            //         printf("(Read Register)\n");
            //         mode = getMode();
            //         printf("Working on mode: %d\n", mode);
            //         if (mode == 2) { //write challenge
            //             getChallenge();
            //         }
            //         else if (mode == 4) { //write response
            //             getResponse();
            //         }
            //         break;
            // }
            modbus_reply(context, query, rc, modbus_mapping);
            if (function_code == 0x0F) {
                printf("(Set Mode)\n");
                challenge_idx = 0;
            }
            else if (function_code == 0x10) {
                // write_challenge();
                write_register();
            }
            // switch(function_code) {
            //     case 0x0F:
            //         printf("(Set Mode)\n");
            //         break;
            //     case 0x10:
            //         write_challenge();
            //         break;
            // }
            // if (function_code == 0x06) {
            //     challenge_size = modbus_mapping->tab_registers[0];
            //     printf("Challenge size: %ld\n", challenge_size);
            // }
        } else if (rc == -1) {
            /* Connection closed by the client or server */
            modbus_close(context); // close
            // // immediately start waiting for another request again
            printf("\n\nWaiting for connection...\n");
            // sram.turn_off();
            challenge_idx = response_idx = 0;
            memset(c1, 0, USHRT_MAX * sizeof(uint16_t));
            memset(response, 0, USHRT_MAX * sizeof(uint16_t));
            modbus_tcp_accept(context, &socket);
            // break;
        }
    }

    modbus_mapping_free(modbus_mapping);
    close(socket);
    modbus_free(context);

    return 0;
}

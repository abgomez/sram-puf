/*
* Created by Abel Gomez 04/15/2020
*/

#include <string.h>
#include <stdlib.h>
#include <limits.h>
#include <stdio.h>
#include <errno.h>
#include <math.h>
#include <time.h>

#include "../PUF/sram.h"

SRAM sram;

uint8_t *final_response;
uint8_t *raw_response;

uint16_t *strong_bits;
uint16_t *challenge;

int CHALLENGE_SIZE = 256;
int challenge_no = 0;
int strong_bit_count = 0;

/* 
* Function Prototypes 
*/
int readBit(uint16_t);
int initialize();

void get_response();
void get_challenge();
void get_strong_bit();
void format_response();
void swap(uint16_t *, uint16_t *);


int initialize() {
     /* Allocate and initialize the memory to store the final response */
    final_response = (uint8_t *) malloc((CHALLENGE_SIZE / 8) * sizeof(uint8_t));
    if (final_response == NULL) {
        fprintf(stderr, "Failed to allocated memory");
        return -1;
    }
    memset(final_response, 0, (CHALLENGE_SIZE / 8) * sizeof(uint8_t));

    /* Allocate and initialize the memory to store the raw response */
    raw_response = (uint8_t *) malloc((CHALLENGE_SIZE /8) * sizeof(uint8_t));
    if (raw_response == NULL) {
        fprintf(stderr, "Failed to allocated memory");
        return -1;
    }
    memset(raw_response, 0, (CHALLENGE_SIZE /8) * sizeof(uint8_t));

    /* Allocate and initialize the memory to store the challenge */
    challenge = (uint16_t *) malloc(CHALLENGE_SIZE * sizeof(uint16_t));
    if (challenge == NULL) {
        fprintf(stderr, "Failed to allocated memory");
        return -1;
    }
    memset(challenge, 0, CHALLENGE_SIZE * sizeof(uint16_t));

     /* Allocate and initialize the memory to store the strong bits */
    strong_bits = (uint16_t *) malloc(USHRT_MAX * sizeof(uint16_t));
    if (strong_bits == NULL) {
        fprintf(stderr, "Failed to allocated memory");
        return -1;
    }
    memset(strong_bits, 0, USHRT_MAX * sizeof(uint16_t));

    return 0;
}

void get_strong_bit() {
    char challenge_name[3];
    int strong_bit = 0;
    char location[10];
    FILE *fp;

    if ((fp = fopen("../Characterization/strongBit", "r")) == NULL) {
        printf("Error Opening File\n");
        exit(1);
    }

    while (fgets(location, sizeof(location), fp) != NULL) {
        strong_bits[strong_bit] = atoi(location);
        strong_bit++;
    }
    fclose(fp);
    strong_bit_count = strong_bit;
}

void swap(uint16_t *a, uint16_t *b) {
    uint16_t temp = *a;
    *a = *b;
    *b = temp;
}

void get_challenge() {
    FILE *fp;
    char challenge_name[3];

    get_strong_bit();

    srand(time(NULL));
    for (int i = 0; i < strong_bit_count; i++) {
        int j = rand() % strong_bit_count;
        swap(&strong_bits[i], &strong_bits[j]);
    }
    memcpy(challenge, strong_bits, CHALLENGE_SIZE * sizeof(uint16_t));

    srand(time(NULL));
    challenge_no = rand() % 99;
    sprintf(challenge_name, "c%d", challenge_no);
    printf("CRP - %d\n\n", challenge_no);
    fp = fopen(challenge_name, "w+");
    for (int i=0; i < CHALLENGE_SIZE; i++) {
        printf("%d ", challenge[i]);
        fprintf(fp, "%d\n", challenge[i]);
    }
    printf("\n\n");
    fclose(fp);
}

void format_response() {
    int a = 0;
    int b = 0;
    uint8_t tmp_key;

    for (int i = 0; i < CHALLENGE_SIZE/8; i++) {
        tmp_key = 0;
        for (int j = 0; j < 7; j++) {
            switch (j) {
                case 0:
                    if (raw_response[a+j] == 1) {
                        tmp_key += 128;
                    }
                    break;
                case 1:
                    if (raw_response[a+j] == 1) {
                        tmp_key += 64;
                    }
                    break;
                case 2:
                    if (raw_response[a+j] == 1) {
                        tmp_key += 32;
                    }
                    break;
                case 3:
                    if (raw_response[a+j] == 1) {
                        tmp_key += 16;
                    }
                    break;
                case 4:
                    if (raw_response[a+j] == 1) {
                        tmp_key += 8;
                    }
                    break;
                case 5:
                    if (raw_response[a+j] == 1) {
                        tmp_key += 4;
                    }
                    break;
                case 6:
                    if (raw_response[a+j] == 1) {
                        tmp_key += 2;
                    }
                    break;
                case 7:
                    if (raw_response[a+j] == 1) {
                        tmp_key++;
                    }
                    break;
            }
        }
        final_response[i] = tmp_key;
        a +=8;
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

void get_response() {
    FILE *fp;
    char challenge_name[3];

    for (int i = 0; i < CHALLENGE_SIZE; i++) {
        raw_response[i] = readBit(challenge[i]);
        printf("%d ", raw_response[i]);
    }
    printf("\n\n");

    format_response();

    sprintf(challenge_name, "r%d", challenge_no);
    fp = fopen(challenge_name, "w+");
    for (int i=0; i < CHALLENGE_SIZE/8; i++) {
        printf("%d ", final_response[i]);
        fprintf(fp, "%d\n", final_response[i]);
    }
    printf("\n\n");
    fclose(fp);
}

int main(int argc, char **argv){
    
    if (argc == 2) {
        CHALLENGE_SIZE = atoi(argv[1]);
    }
    else {
        printf("Default CHALLENGE_SIZE = 256\n");
    }

    /* Initialize variables */
    initialize();

    /* get challenge */
    get_challenge();
    
    /* get response */
    get_response();

    return 0;
}
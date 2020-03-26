#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include "PUF/sram.h"

#define CHALLENGE_SIZE 256

uint8_t response[CHALLENGE_SIZE];
uint8_t response_tmp[CHALLENGE_SIZE];
long challenge[CHALLENGE_SIZE];

SRAM sram;

int readBit(long location) {
    uint8_t recv_data = 0x00;

    sram.turn_on();
    recv_data = sram.read_byte(floor(location / 8));
    sram.turn_off();
    
    return recv_data >> (7 - (location % 8)) & 1;
}

void getLocation() {
    FILE *fp;
    
    long loc;
    int index = 0;
    char location[10];

    if ((fp = fopen("c-21", "r")) == NULL) {
        printf("Error Opening File");
        exit(1);
    }

    while (fgets(location, sizeof(location), fp) != NULL) {
        challenge[index] = atoi(location);
        index++;
    }
    fclose(fp);
}

void getResponse() {
    for (int i=0; i < CHALLENGE_SIZE; i++) {
        response[i] = readBit(challenge[i]);
        // printf("%d", response[i]);
    }
    // printf("\n");
}



int main(int argc, char **argv) {
    memset(challenge, 0, sizeof(CHALLENGE_SIZE));
    getLocation();
    getResponse();
    int count = 0;

    for (int i=0; i<1000; i++) {
        count = 0;
        for (int i=0; i < CHALLENGE_SIZE; i++) {
            response_tmp[i] = readBit(challenge[i]);
            // printf("%d", response_tmp[i]);
        }
        // printf("\n");
        for (int i=0; i < CHALLENGE_SIZE; i++) {
            if (response[i] != response_tmp[i]) {
                count++;
            }
        }
        printf("run: %d -- error: %d\n", i, count);
        if (count > 0) {
            break;
        }
    }
    return 0;
}
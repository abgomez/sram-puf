/*
* Created by Abel Gomez 03/31/2020
*/

#include <string.h>
#include <stdlib.h>
#include <modbus.h>
#include <stdio.h>
#include <errno.h>
#include <time.h>

/*
* Slave Mode of Operation, 4 bits
* 0000 stand by
* 0001 new challenge
* 0010 write challenge
* 0011 finish challenge
* 0100 get response
* 0101 finish challenge
*/

#define GET_RESPONSE 1   //0b01
#define GET_CHALLENGE 0  //0b00
#define MODBUS_PORT 1502
#define SLAVE_IP "127.0.0.1"
#define CHALLENGE_SIZE MODBUS_MAX_WRITE_REGISTERS

modbus_t *context;

uint8_t  *bit;
uint16_t  *response;
uint8_t  *slave_mode;

uint16_t *registers;
uint16_t *challenge;

int challenge_no = 0;

int initialize() {
    /* Set response timeout*/
    modbus_set_byte_timeout(context, 0, 0);
    modbus_set_response_timeout(context, 60, 0);

    /* Allocate and initialize the memory to store the status */
    bit = (uint8_t *) malloc(MODBUS_MAX_READ_BITS * sizeof(uint8_t));
    if (bit == NULL) {
        fprintf(stderr, "Failed to allocated memory");
        return -1;
    }
    memset(bit, 0, MODBUS_MAX_READ_BITS * sizeof(uint8_t));

    /* Allocate and initialize the memory to store the mode */
    slave_mode = (uint8_t *) malloc(2 * sizeof(uint8_t));
    if (slave_mode == NULL) {
        fprintf(stderr, "Failed to allocated memory");
        return -1;
    }
    memset(slave_mode, 0, 2 * sizeof(uint8_t));

    /* Allocate and initialize the memory to store the registers */
    registers = (uint16_t *) malloc(MODBUS_MAX_READ_REGISTERS * sizeof(uint16_t));
    if (registers == NULL) {
        fprintf(stderr, "Failed to allocated memory");
        return -1;
    }
    memset(registers, 0, MODBUS_MAX_READ_REGISTERS * sizeof(uint16_t));

    /* Allocate and initialize the memory to store the challenge */
    challenge = (uint16_t *) malloc(CHALLENGE_SIZE * sizeof(uint16_t));
    if (challenge == NULL) {
        fprintf(stderr, "Failed to allocated memory");
        return -1;
    }
    memset(challenge, 0, CHALLENGE_SIZE * sizeof(uint16_t));

    /* Allocate and initialize the memory to store the challenge */
    response = (uint16_t *) malloc(CHALLENGE_SIZE * sizeof(uint16_t));
    if (response == NULL) {
        fprintf(stderr, "Failed to allocated memory");
        return -1;
    }
    memset(response, 0, CHALLENGE_SIZE * sizeof(uint16_t));

    return 0;
}

int setMode(int mode) {
    int rc = 0;

    /* Set slave mode */
    switch (mode) {
        case GET_CHALLENGE:
            slave_mode[0] = 0;
            slave_mode[1] = 0;
            rc = modbus_write_bits(context, 0, 2, slave_mode);
            if (rc == -1) {
                fprintf(stderr, "%s\n", modbus_strerror(errno));
                return -1;
            }
            break;
        case GET_RESPONSE:
            slave_mode[0] = 0;
            slave_mode[1] = 1;
            rc = modbus_write_bits(context, 0, 2, slave_mode);
            if (rc == -1) {
                fprintf(stderr, "%s\n", modbus_strerror(errno));
                return -1;
            }
            break;
        default:
            printf("Incorrect Mode\n");
            return -1;
    }
    return 0;
}

int getMode() {
    int rc = 0;

    rc = modbus_read_bits(context, 0, 2, bit);
    if (rc == -1) {
        fprintf(stderr, "%s\n", modbus_strerror(errno));
        return -1;
    }

    for (int i=0; i<2; i++) {
        printf("%d ", bit[i]);
    }
    printf("\n\n");
    return 0;

}

int getChallenge() {
    FILE *fp;
    int rc = 0;
    char challenge_name[3];
    
    rc = modbus_read_registers(context, 0, CHALLENGE_SIZE, registers);
    if (rc == -1) {
        fprintf(stderr, "%s\n", modbus_strerror(errno));
        return -1;
    }

    srand(time(NULL));
    challenge_no = rand() % 99;
    sprintf(challenge_name, "c%d", challenge_no);
    fp = fopen(challenge_name, "w+");
    for (int i=0; i < CHALLENGE_SIZE; i++) {
        challenge[i] = registers[i];
        printf("%ld ", challenge[i]);
        fprintf(fp, "%d\n", challenge[i]);
    }
    printf("\n\n");
    fclose(fp);

    return 0;
}

int getResonse() {
    FILE *fp;
    int rc = 0;
    char challenge_name[3];

    for (int i=0; i < CHALLENGE_SIZE; i++) {
        registers[i] = challenge[i];
    }

    /* send challenge to slave */
    rc = modbus_write_registers(context, 0, CHALLENGE_SIZE, registers);
    if (rc == -1) {
        fprintf(stderr, "%s\n", modbus_strerror(errno));
        return -1;
    }

    /* read response */
    rc = modbus_read_registers(context, 0, CHALLENGE_SIZE, registers);
    if (rc == -1) {
        fprintf(stderr, "%s\n", modbus_strerror(errno));
        return -1;
    }

    sprintf(challenge_name, "r%d", challenge_no);
    fp = fopen(challenge_name, "w+");
    for (int i=0; i < CHALLENGE_SIZE; i++) {
        response[i] = registers[i];
        printf("%ld ", response[i]);
        fprintf(fp, "%d\n", response[i]);
    }

    printf("\n\n");
    fclose(fp);
    
    return 0;
}

int main(int arcg, char **argv){
    /* Connect to slave */
    context = modbus_new_tcp(SLAVE_IP, MODBUS_PORT);

    if (modbus_connect(context) == -1) {
        fprintf(stderr, "Connection failed: %s\n", modbus_strerror(errno));
        modbus_free(context);
        exit(EXIT_FAILURE);
    }

    /* Initialize variables */
    if (initialize() != 0) {
        printf("Failed to allocated memory\n");
        exit(EXIT_FAILURE);
    }

    /* Set slave mode */
    if (setMode(GET_CHALLENGE) != 0) {
        printf("Failed to set slave mode\n");
        exit(EXIT_FAILURE);
    }

    // if (getMode() != 0) {
    //     printf("Failed to get slave mode");
    //     return -1;
    // }

    /* Get challenge */
    if (getChallenge() !=0 ) {
        printf("Failed to get challenge\n");
        exit(EXIT_FAILURE);
    }

    /* Set slave mode */
    if (setMode(GET_RESPONSE) != 0) {
        printf("Failed to set slave mode\n");
        exit(EXIT_FAILURE);
    }

    /* Get response */
    if (getResonse() !=0 ) {
        printf("Failed to get response\n");
        exit(EXIT_FAILURE);
    }
    
     /* Free the memory */
    free(bit);
    free(registers);
    free(challenge);

    /* Close the connection */
    modbus_close(context);
    modbus_free(context);

    return 0;
}
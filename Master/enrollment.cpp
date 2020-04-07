/*
* Created by Abel Gomez 03/31/2020
*/

#include <string.h>
#include <stdlib.h>
#include <modbus.h>
#include <stdio.h>
#include <errno.h>
#include <math.h>
#include <time.h>

/*
* Slave - Mode of Operation, 4 bits
*/
#define MODE 4
#define DEFAULT 0b0000
#define NEW_CHALLENGE 0b0001
#define WRITE_CHALLENGE 0b0010
#define FINISH_CHALLENGE 0b0011
#define WRITE_RESPONSE 0b0100

#define MODBUS_PORT 1502
#define SLAVE_IP "127.0.0.1"
#define CHALLENGE_SIZE 256

modbus_t *context;

uint8_t  *bits;
uint8_t  *slave_mode;
uint8_t  *final_response;

uint16_t *registers;
uint16_t *challenge;
uint16_t *response;

int challenge_no = 0;

int initialize() {
    /* Set response timeout*/
    modbus_set_byte_timeout(context, 0, 0);
    modbus_set_response_timeout(context, 60, 0);

    /* Allocate and initialize the memory to store the status */
    bits = (uint8_t *) malloc(MODBUS_MAX_READ_BITS * sizeof(uint8_t));
    if (bits == NULL) {
        fprintf(stderr, "Failed to allocated memory");
        return -1;
    }
    memset(bits, 0, MODBUS_MAX_READ_BITS * sizeof(uint8_t));

    /* Allocate and initialize the memory to store the mode */
    slave_mode = (uint8_t *) malloc(MODE * sizeof(uint8_t));
    if (slave_mode == NULL) {
        fprintf(stderr, "Failed to allocated memory");
        return -1;
    }
    memset(slave_mode, 0, MODE * sizeof(uint8_t));

     /* Allocate and initialize the memory to store the final response */
    final_response = (uint8_t *) malloc((CHALLENGE_SIZE / 8) * sizeof(uint8_t));
    if (final_response == NULL) {
        fprintf(stderr, "Failed to allocated memory");
        return -1;
    }
    memset(final_response, 0, (CHALLENGE_SIZE / 8) * sizeof(uint8_t));

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

/* Function that sets the mode of the slave 
*  the slave con operate in 16 modes.
*/
int setMode(int mode) {
    int rc = 0;

    switch (mode) {
        case DEFAULT:
            slave_mode[0] = 0;
            slave_mode[1] = 0;
            slave_mode[2] = 0;
            slave_mode[3] = 0;
            rc = modbus_write_bits(context, 0, MODE, slave_mode);
            if (rc == -1) {
                fprintf(stderr, "%s\n", modbus_strerror(errno));
                return -1;
            }
            break;
        case NEW_CHALLENGE:
            slave_mode[0] = 0;
            slave_mode[1] = 0;
            slave_mode[2] = 0;
            slave_mode[3] = 1;
            rc = modbus_write_bits(context, 0, MODE, slave_mode);
            if (rc == -1) {
                fprintf(stderr, "%s\n", modbus_strerror(errno));
                return -1;
            }
            break;
        case WRITE_CHALLENGE:
            slave_mode[0] = 0;
            slave_mode[1] = 0;
            slave_mode[2] = 1;
            slave_mode[3] = 0;
            rc = modbus_write_bits(context, 0, MODE, slave_mode);
            if (rc == -1) {
                fprintf(stderr, "%s\n", modbus_strerror(errno));
                return -1;
            }
            break;
        case FINISH_CHALLENGE:
            slave_mode[0] = 0;
            slave_mode[1] = 0;
            slave_mode[2] = 1;
            slave_mode[3] = 1;
            rc = modbus_write_bits(context, 0, MODE, slave_mode);
            if (rc == -1) {
                fprintf(stderr, "%s\n", modbus_strerror(errno));
                return -1;
            }
            break;
        case WRITE_RESPONSE:
            slave_mode[0] = 0;
            slave_mode[1] = 1;
            slave_mode[2] = 0;
            slave_mode[3] = 0;
            rc = modbus_write_bits(context, 0, MODE, slave_mode);
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

    rc = modbus_read_bits(context, 0, MODE, bits);
    if (rc == -1) {
        fprintf(stderr, "%s\n", modbus_strerror(errno));
        return -1;
    }

    for (int i = 0; i < MODE; i++) {
        printf("%d ", bits[i]);
    }
    printf("\n\n");
    return 0;

}

int getChallenge() {
    FILE *fp;
    int rc = 0;
    int section_no = 0;
    int remainder_bit = 0;
    char challenge_name[3];

    section_no = (int) ceil(CHALLENGE_SIZE/MODBUS_MAX_READ_REGISTERS);

    for (int i = 0; i < section_no; i++) {
        rc = modbus_read_registers(context, 0, MODBUS_MAX_READ_REGISTERS, registers);
        if (rc == -1) {
            fprintf(stderr, "%s\n", modbus_strerror(errno));
            return -1;
        }
        memcpy(&challenge[i*MODBUS_MAX_READ_REGISTERS], registers, MODBUS_MAX_READ_REGISTERS*sizeof(uint16_t));
    }

    remainder_bit = CHALLENGE_SIZE - MODBUS_MAX_READ_REGISTERS * (int) ceil(CHALLENGE_SIZE/MODBUS_MAX_READ_REGISTERS);
    if (remainder_bit > 0) {
        rc = modbus_read_registers(context, 0, remainder_bit, registers);
        if (rc == -1) {
            fprintf(stderr, "%s\n", modbus_strerror(errno));
            return -1;
        }
        memcpy(&challenge[section_no*MODBUS_MAX_READ_REGISTERS], registers, remainder_bit*sizeof(uint16_t));
    }

    srand(time(NULL));
    challenge_no = rand() % 99;
    sprintf(challenge_name, "c%d", challenge_no);
    fp = fopen(challenge_name, "w+");
    for (int i=0; i < CHALLENGE_SIZE; i++) {
        printf("%d ", challenge[i]);
        fprintf(fp, "%d\n", challenge[i]);
    }
    printf("\n\n");
    fclose(fp);
    
    return 0;
}

void format_response() {
    int a = 0;
    int b = 0;
    // int sub_key[] = {0,0,0,0,0,0,0,0};
    uint8_t tmp_key;

    for (int i = 0; i < CHALLENGE_SIZE/8; i++) {
        tmp_key = 0;
        for (int j = 0; j < 7; j++) {
            // sub_key[j] = response[a+j];
            switch (j) {
                case 0:
                    if (response[a+j] == 1) {
                        tmp_key += 128;
                    }
                    break;
                case 1:
                    if (response[a+j] == 1) {
                        tmp_key += 64;
                    }
                    break;
                case 2:
                    if (response[a+j] == 1) {
                        tmp_key += 32;
                    }
                    break;
                case 3:
                    if (response[a+j] == 1) {
                        tmp_key += 16;
                    }
                    break;
                case 4:
                    if (response[a+j] == 1) {
                        tmp_key += 8;
                    }
                    break;
                case 5:
                    if (response[a+j] == 1) {
                        tmp_key += 4;
                    }
                    break;
                case 6:
                    if (response[a+j] == 1) {
                        tmp_key += 2;
                    }
                    break;
                case 7:
                    if (response[a+j] == 1) {
                        tmp_key++;
                    }
                    break;
            }
        }
        final_response[i] = tmp_key;
        a +=8;
    }
}

int getResonse() {
    FILE *fp;
    int rc = 0;
    int section_no = 0;
    int remainder_bit = 0;
    char response_name[3];

    section_no = (int) ceil(CHALLENGE_SIZE/MODBUS_MAX_WRITE_REGISTERS);

    for (int i = 0; i < section_no; i++) {
        /* send challenge to slave */
        memcpy(registers, &challenge[i*MODBUS_MAX_WRITE_REGISTERS], MODBUS_MAX_WRITE_REGISTERS*sizeof(uint16_t));
        
        rc = modbus_write_registers(context, 0, MODBUS_MAX_WRITE_REGISTERS, registers);
        if (rc == -1) {
            fprintf(stderr, "%s\n", modbus_strerror(errno));
            return -1;
        }

        /* read response */
        rc = modbus_read_registers(context, 0, MODBUS_MAX_WRITE_REGISTERS, registers);
        if (rc == -1) {
            fprintf(stderr, "%s\n", modbus_strerror(errno));
            return -1;
        }
        memcpy(&response[i*MODBUS_MAX_WRITE_REGISTERS], registers, MODBUS_MAX_WRITE_REGISTERS*sizeof(uint16_t));
    }

    remainder_bit = CHALLENGE_SIZE - MODBUS_MAX_WRITE_REGISTERS * (int) ceil(CHALLENGE_SIZE/MODBUS_MAX_WRITE_REGISTERS);
    if (remainder_bit > 0) {
        /* send challenge to slave */
        memcpy(registers, &challenge[section_no*MODBUS_MAX_WRITE_REGISTERS], MODBUS_MAX_WRITE_REGISTERS*sizeof(uint16_t));
        
        rc = modbus_write_registers(context, 0, remainder_bit, registers);
        if (rc == -1) {
            fprintf(stderr, "%s\n", modbus_strerror(errno));
            return -1;
        }

         /* read response */
        rc = modbus_read_registers(context, 0, remainder_bit, registers);
        if (rc == -1) {
            fprintf(stderr, "%s\n", modbus_strerror(errno));
            return -1;
        }
        memcpy(&response[section_no*MODBUS_MAX_WRITE_REGISTERS], registers, remainder_bit*sizeof(uint16_t));
    }

    for (int i=0; i < CHALLENGE_SIZE; i++) {
        printf("%d ", response[i]);
        // printf("%d ", MODBUS_GET_LOW_BYTE(response[i]));
        // fprintf(fp, "%d\n", response[i]);
    }
    printf("\n\n");
    // fclose(fp);

    format_response();

    printf("\n\n");
    sprintf(response_name, "r%d", challenge_no);
    fp = fopen(response_name, "w+");
    for (int i=0; i < CHALLENGE_SIZE/8; i++) {
        printf("%d ", final_response[i]);
        fprintf(fp, "%d\n", final_response[i]);
    }
    printf("\n\n");
    fclose(fp);
    
    return 0;
}

int main(int argc, char **argv){
    int rc = 0;
    
    // if (argc != 2) {
    //     printf("Incorrect number of arguments..\n");
    //     printf("./enrollment <challenge_size>\n");
    //     exit(EXIT_FAILURE);
    // }

    /* Connect to slave TODO: change communication link and add multiple slaves*/
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
    if (setMode(NEW_CHALLENGE) != 0) {
        printf("Failed to set slave mode\n");
        exit(EXIT_FAILURE);
    }
    /* Send challenge size to slave */
    // rc = modbus_write_register(context, 0, challenge_size);
    // if (rc == -1) {
    //     fprintf(stderr, "%s\n", modbus_strerror(errno));
    //     return -1;
    // }

    /* Set slave mode */
    if (setMode(WRITE_CHALLENGE) != 0) {
        printf("Failed to set slave mode\n");
        exit(EXIT_FAILURE);
    }

    /* Get challenge */
    if (getChallenge() !=0 ) {
        printf("Failed to get challenge\n");
        exit(EXIT_FAILURE);
    }

     /* Set slave mode */
    if (setMode(WRITE_RESPONSE) != 0) {
        printf("Failed to set slave mode\n");
        exit(EXIT_FAILURE);
    }

    // if (getMode() != 0) {
    //     printf("Failed to get slave mode");
    //     return -1;
    // }

    /* Get response */
    if (getResonse() !=0 ) {
        printf("Failed to get response\n");
        exit(EXIT_FAILURE);
    }
    
     /* Free the memory */
    free(bits);
    free(registers);
    free(challenge);

    /* Close the connection */
    modbus_close(context);
    modbus_free(context);

    return 0;
}
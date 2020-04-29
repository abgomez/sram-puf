/*
* Created by Abel Gomez 04/17/2020
* the program simulates a master device (RTU) in the modbus protocol
* the master pull information from a temperature sensor (or slave).
*/
#include <modbus.h>
#include "lib/tools.h"

#define HASH_LEN 32
#define MODBUS_PORT 1502
#define CHALLENGE_SIZE 256
#define SLAVE_IP "127.0.0.1"

/*
* Slave - Mode of Operation, 4 bits
*/
#define MODE 4
#define WRITE_C    0b0000
#define WRITE_TS   0b0001
#define WRITE_HMAC 0b0010
#define READ_DATA  0b0011
#define READ_HMAC  0b0100

modbus_t *context;

uint8_t *hmac;
uint8_t *bits;
uint8_t *response;
uint8_t *data_hmac;
uint8_t *slave_mode;
uint8_t *master_hmac;
uint8_t *response_hash;
uint8_t temperature = 0;

uint16_t *challenge;
uint16_t *registers;

/* 
* Function Prototypes 
*/
int read_hmac();
int setMode(int);
int initialize();
int read_sensor();
int write_ts(uint32_t);
int write_master_hmac();
int write_challenge(uint16_t *);
void get_master_hmac(uint32_t ts);

int initialize() {
    /* Set response timeout*/
    modbus_set_byte_timeout(context, 0, 0);
    modbus_set_response_timeout(context, 440, 0);

    /* Allocate and initialize the memory to store the status */
    bits = (uint8_t *) malloc(MODBUS_MAX_READ_BITS * sizeof(uint8_t));
    if (bits == NULL) {
        fprintf(stderr, "Failed to allocated memory");
        return -1;
    }
    memset(bits, 0, MODBUS_MAX_READ_BITS * sizeof(uint8_t));

    /* Allocate and initialize the memory to store the response */
    response = (uint8_t *) malloc((CHALLENGE_SIZE / 8) * sizeof(uint8_t));
    if (response == NULL) {
        fprintf(stderr, "Failed to allocated memory");
        return -1;
    }
    memset(response, 0, (CHALLENGE_SIZE / 8) * sizeof(uint8_t));
    
    /* Allocate and initialize the memory to store the response_hash */
    response_hash = (uint8_t *) malloc((CHALLENGE_SIZE / 8) * sizeof(uint8_t));
    if (response_hash == NULL) {
        fprintf(stderr, "Failed to allocated memory");
        return -1;
    }
    memset(response_hash, 0, (CHALLENGE_SIZE / 8) * sizeof(uint8_t));

    /* Allocate and initialize the memory to store master_hmac */
    master_hmac = (uint8_t *) malloc(EVP_MAX_MD_SIZE * sizeof(uint8_t));
    if (master_hmac == NULL) {
        fprintf(stderr, "Failed to allocated memory");
        return -1;
    }
    memset(master_hmac, 0, EVP_MAX_MD_SIZE * sizeof(uint8_t));

    /* Allocate and initialize the memory to store hmac */
    hmac = (uint8_t *) malloc(EVP_MAX_MD_SIZE * sizeof(uint8_t));
    if (hmac == NULL) {
        fprintf(stderr, "Failed to allocated memory");
        return -1;
    }
    memset(hmac, 0, EVP_MAX_MD_SIZE * sizeof(uint8_t));

    /* Allocate and initialize the memory to store data_hmac */
    data_hmac = (uint8_t *) malloc(EVP_MAX_MD_SIZE * sizeof(uint8_t));
    if (data_hmac == NULL) {
        fprintf(stderr, "Failed to allocated memory");
        return -1;
    }
    memset(data_hmac, 0, EVP_MAX_MD_SIZE * sizeof(uint8_t));


    /* Allocate and initialize the memory to store the registers */
    registers = (uint16_t *) malloc(MODBUS_MAX_READ_REGISTERS * sizeof(uint16_t));
    if (registers == NULL) {
        fprintf(stderr, "Failed to allocated memory");
        return -1;
    }
    memset(registers, 0, MODBUS_MAX_READ_REGISTERS * sizeof(uint16_t));

    /* Allocate and initialize the memory to store the c1 */
    challenge = (uint16_t *) malloc(CHALLENGE_SIZE * sizeof(uint16_t));
    if (challenge == NULL) {
        fprintf(stderr, "Failed to allocated memory");
        return -1;
    }
    memset(challenge, 0, CHALLENGE_SIZE * sizeof(uint16_t));

    /* Allocate and initialize the memory to store the mode */
    slave_mode = (uint8_t *) malloc(MODE * sizeof(uint8_t));
    if (slave_mode == NULL) {
        fprintf(stderr, "Failed to allocated memory");
        return -1;
    }
    memset(slave_mode, 0, MODE * sizeof(uint8_t));

    return 0;
}

/*
* get_master_hmac
* create a message authentication code to authenticate the master
*/
void get_master_hmac(uint32_t ts) {
    uint8_t ts_bytes[4];
    uint32_t tmp = 1588189708;
    uint8_t message[CHALLENGE_SIZE * 2 + 4];

    memset(message, 0, sizeof(message));

    /* concat challenge and timestamp */
    memcpy(message, challenge, CHALLENGE_SIZE * sizeof(uint16_t));
    memcpy(&message[CHALLENGE_SIZE*2], &tmp, 4);

    memcpy(ts_bytes, &tmp, 4);
    
    master_hmac = HMAC(EVP_sha256(), response, 4, ts_bytes, 4, NULL, NULL);
}

int setMode(int mode) {
    int rc = 0;

    /* Set slave mode */
    switch (mode) {
        case WRITE_C:
            slave_mode[0] = 0;
            slave_mode[1] = 0;
            slave_mode[2] = 0;
            slave_mode[3] = 0;
            rc = modbus_write_bits(context, 0, 4, slave_mode);
            if (rc == -1) {
                fprintf(stderr, "%s\n", modbus_strerror(errno));
                return -1;
            }
            break;
        case WRITE_TS:
            slave_mode[0] = 0;
            slave_mode[1] = 0;
            slave_mode[2] = 0;
            slave_mode[3] = 1;
            rc = modbus_write_bits(context, 0, 4, slave_mode);
            if (rc == -1) {
                fprintf(stderr, "%s\n", modbus_strerror(errno));
                return -1;
            }
            break;
        case WRITE_HMAC:
            slave_mode[0] = 0;
            slave_mode[1] = 0;
            slave_mode[2] = 1;
            slave_mode[3] = 0;
            rc = modbus_write_bits(context, 0, 4, slave_mode);
            if (rc == -1) {
                fprintf(stderr, "%s\n", modbus_strerror(errno));
                return -1;
            }
            break;
        case READ_DATA:
            slave_mode[0] = 0;
            slave_mode[1] = 0;
            slave_mode[2] = 1;
            slave_mode[3] = 1;
            rc = modbus_write_bits(context, 0, 4, slave_mode);
            if (rc == -1) {
                fprintf(stderr, "%s\n", modbus_strerror(errno));
                return -1;
            }
            break;
        case READ_HMAC:
            slave_mode[0] = 0;
            slave_mode[1] = 1;
            slave_mode[2] = 0;
            slave_mode[3] = 0;
            rc = modbus_write_bits(context, 0, 4, slave_mode);
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

int write_challenge(uint16_t * challenge) {
    int rc = 0;
    int section_no = 0;
    int remainder_bit = 0;

    section_no = (int) ceil(CHALLENGE_SIZE/MODBUS_MAX_WRITE_REGISTERS);

    for (int i = 0; i < section_no; i++) {
        /* send challenge to slave */
        memcpy(registers, &challenge[i*MODBUS_MAX_WRITE_REGISTERS], MODBUS_MAX_WRITE_REGISTERS*sizeof(uint16_t));
        
        rc = modbus_write_registers(context, 0, MODBUS_MAX_WRITE_REGISTERS, registers);
        if (rc == -1) {
            fprintf(stderr, "%s\n", modbus_strerror(errno));
            return -1;
        }
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
    }

    return 0;
}


int write_ts(uint32_t timestamp) {
    int rc = 0;
    uint16_t ts_array[2];
    
    memset(ts_array, 0, 2 * sizeof(uint16_t));
    memcpy(ts_array, &timestamp, 4);
    
    rc = modbus_write_registers(context, 0, 2, ts_array);
    if (rc == -1) {
        fprintf(stderr, "%s\n", modbus_strerror(errno));
        return -1;
    }
    
    return 0;
}

int write_master_hmac() {
    int rc = 0;

    memcpy(registers, master_hmac, HASH_LEN * sizeof(uint8_t));
    
    rc = modbus_write_registers(context, 0, HASH_LEN, registers);
    if (rc == -1) {
        fprintf(stderr, "%s\n", modbus_strerror(errno));
        return -1;
    }

    return 0;
}

int read_sensor() {
    int rc = 0;

    rc = modbus_read_registers(context, 0, 1, registers);
    if (rc == -1) {
        fprintf(stderr, "%s\n", modbus_strerror(errno));
        return -1;
    }

    memcpy(&temperature, registers, sizeof(uint8_t));

    printf("Temperature read from sensor: %d\n", temperature);
    return 0;
}

int read_hmac() {
    int rc = 0;

    rc = modbus_read_registers(context, 0, HASH_LEN, registers);
    if (rc == -1) {
        fprintf(stderr, "%s\n", modbus_strerror(errno));
        return -1;
    }

    memcpy(hmac, registers, HASH_LEN*sizeof(uint8_t));
    
    data_hmac = HMAC(EVP_sha256(), response, 4, &temperature, 1, NULL, NULL);

    if (memcmp(hmac, data_hmac, HASH_LEN) == 0 ) {
        printf("Data Integrity verified..\n" );
        return 0;
    }

    return -1;
}

int main(int argc, char **argv){
    int rc = 0;
    time_t ts = 0;
    uint32_t helper;
    uint32_t hash_len = 0;
    uint32_t timestamp = 0;

    /* Initialize variables */
    if (initialize() != 0) {
        printf("Failed to allocated memory\n");
        exit(EXIT_FAILURE);
    }

    /* Select one CRP */
    if (get_challenge(challenge) != 0) {
        printf("Failed to get challenge\n");
        exit(EXIT_FAILURE);
    }

    if (get_response(response) != 0) {
        printf("Failed to get response\n");
        exit(EXIT_FAILURE);
    }
    /*hash the response*/
    get_sha256(response, response_hash);
    printf("Hash of response: ");
    for (int i = 0; i < HASH_LEN; i++) {
        printf("%02x", response_hash[i]);
    }
    printf("\n");

    /* get timestamp */
    time(&ts);
    timestamp = (uint32_t) ts;
    printf("Timestamp: %d\n", timestamp);

    /* get hmac */
    get_master_hmac(timestamp);
    printf("Master hmac: ");
    for (int i = 0; i < HASH_LEN; i++) {
        printf("%02x", master_hmac[i]);
    }
    printf("\n");

    /* get timestamp prime */
    memcpy(&helper, response_hash, 4);
    timestamp ^= helper;

    /* Connect to slave TODO: change communication link and add multiple slaves*/
    context = modbus_new_tcp(SLAVE_IP, MODBUS_PORT);

    if (modbus_connect(context) == -1) {
        fprintf(stderr, "Connection failed: %s\n", modbus_strerror(errno));
        modbus_free(context);
        exit(EXIT_FAILURE);
    }

    /* Set slave mode */
    if (setMode(WRITE_C) != 0) {
        printf("Failed to set slave mode\n");
        exit(EXIT_FAILURE);
    }

    /* Write challenge */
    if (write_challenge(challenge) != 0) {
        printf("Failed to write H\n");
        exit(EXIT_FAILURE);
    }
    
    /* Set slave mode */
    if (setMode(WRITE_TS) != 0) {
        printf("Failed to set slave mode\n");
        exit(EXIT_FAILURE);
    }

    /* write TS */
    if (write_ts(timestamp) != 0) {
        printf("Failed to write challenge\n");
        exit(EXIT_FAILURE);
    }

    /* Set slave mode */
    if (setMode(WRITE_HMAC) != 0) {
        printf("Failed to set slave mode\n");
        exit(EXIT_FAILURE);
    }

    /* write master hmac */
    if (write_master_hmac() != 0) {
        printf("Failed to write challenge\n");
        exit(EXIT_FAILURE);
    }

    printf("Reading sensor...\n");

    /* Set slave mode */
    if (setMode(READ_DATA) != 0) {
        printf("Failed to set slave mode\n");
        exit(EXIT_FAILURE);
    }
    
    /* read sensor */
    if (read_sensor() != 0) {
        printf("Failed to read sensor\n");
        exit(EXIT_FAILURE);
    }
    
    /* Set slave mode */
    if (setMode(READ_HMAC) != 0) {
        printf("Failed to set slave mode\n");
        exit(EXIT_FAILURE);
    }

    /* read sensor data */
    if (read_hmac() != 0) {
        printf("Failed to verified data integrity\n");
        exit(EXIT_FAILURE);
    }
   
     /* Free the memory */
    free(bits);
    free(registers);

    /* Close the connection */
    modbus_close(context);
    modbus_free(context);

    return 0;
}
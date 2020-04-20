/*
* Created by Abel Gomez 04/17/2020
*/
#include <openssl/engine.h>
#include <openssl/hmac.h>
#include <openssl/evp.h>
#include <string.h>
#include <stdlib.h>
#include <modbus.h>
#include <stdio.h>
#include <errno.h>
#include <math.h>
#include <time.h>

#define MODBUS_PORT 1502
#define SLAVE_IP "127.0.0.1"
#define CHALLENGE_SIZE 256

/*
* Slave - Mode of Operation, 4 bits
*/
#define MODE 4
#define WRITE_H  0b0000
#define WRITE_C1 0b0001
#define WRITE_C2 0b0010
#define WRITE_TS 0b0011
#define FINISH_CHALLENGE 0b0011
#define WRITE_RESPONSE 0b0100


modbus_t *context;

uint8_t *H;
uint8_t *r1;
uint8_t *r2;
uint8_t *bits;
uint8_t *r1_hash;
uint8_t *c2_hash;
uint8_t *slave_mode;

uint16_t *c1;
uint16_t *c2;


uint16_t *raw_response;
uint16_t *strong_bits;
uint16_t *registers;

int challenge_no = 0;
int strong_bit_count = 0;

/* 
* Function Prototypes 
*/
int write_challenge(uint16_t *);
int initialize();
int write_H(uint32_t);

int get_response();
int get_challenge();

uint32_t get_H(time_t);
uint32_t get_sha256(uint8_t *, uint8_t *);
uint32_t get_sha256(uint16_t *, uint8_t *);

void get_C2(uint32_t);
void get_strong_bit();
void swap(uint16_t *, uint16_t *);




int initialize() {
    /* Set response timeout*/
    modbus_set_byte_timeout(context, 0, 0);
    modbus_set_response_timeout(context, 120, 0);

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

    /* Allocate and initialize the memory to store the r1 */
    r1 = (uint8_t *) malloc((CHALLENGE_SIZE / 8) * sizeof(uint8_t));
    if (r1 == NULL) {
        fprintf(stderr, "Failed to allocated memory");
        return -1;
    }
    memset(r1, 0, (CHALLENGE_SIZE / 8) * sizeof(uint8_t));

    /* Allocate and initialize the memory to store the r2 */
    r2 = (uint8_t *) malloc((CHALLENGE_SIZE / 8) * sizeof(uint8_t));
    if (r2 == NULL) {
        fprintf(stderr, "Failed to allocated memory");
        return -1;
    }
    memset(r2, 0, (CHALLENGE_SIZE / 8) * sizeof(uint8_t));

    /* Allocate and initialize the memory to store H */
    H = (uint8_t *) malloc(EVP_MAX_MD_SIZE * sizeof(uint8_t));
    if (H == NULL) {
        fprintf(stderr, "Failed to allocated memory");
        return -1;
    }
    memset(H, 0, EVP_MAX_MD_SIZE * sizeof(uint8_t));

    /* Allocate and initialize the memory to store H */
    r1_hash = (uint8_t *) malloc(EVP_MAX_MD_SIZE * sizeof(uint8_t));
    if (r1_hash == NULL) {
        fprintf(stderr, "Failed to allocated memory");
        return -1;
    }
    memset(r1_hash, 0, EVP_MAX_MD_SIZE * sizeof(uint8_t));

    /* Allocate and initialize the memory to store H */
    c2_hash = (uint8_t *) malloc(EVP_MAX_MD_SIZE * sizeof(uint8_t));
    if (c2_hash == NULL) {
        fprintf(stderr, "Failed to allocated memory");
        return -1;
    }
    memset(c2_hash, 0, EVP_MAX_MD_SIZE * sizeof(uint8_t));

    /* Allocate and initialize the memory to store the registers */
    registers = (uint16_t *) malloc(MODBUS_MAX_READ_REGISTERS * sizeof(uint16_t));
    if (registers == NULL) {
        fprintf(stderr, "Failed to allocated memory");
        return -1;
    }
    memset(registers, 0, MODBUS_MAX_READ_REGISTERS * sizeof(uint16_t));

    /* Allocate and initialize the memory to store the raw response */
    raw_response = (uint16_t *) malloc((CHALLENGE_SIZE) * sizeof(uint16_t));
    if (raw_response == NULL) {
        fprintf(stderr, "Failed to allocated memory");
        return -1;
    }
    memset(raw_response, 0, (CHALLENGE_SIZE) * sizeof(uint16_t));

    /* Allocate and initialize the memory to store the c1 */
    c1 = (uint16_t *) malloc(CHALLENGE_SIZE * sizeof(uint16_t));
    if (c1 == NULL) {
        fprintf(stderr, "Failed to allocated memory");
        return -1;
    }
    memset(c1, 0, CHALLENGE_SIZE * sizeof(uint16_t));

    /* Allocate and initialize the memory to store the c2 */
    c2 = (uint16_t *) malloc(CHALLENGE_SIZE * sizeof(uint16_t));
    if (c2 == NULL) {
        fprintf(stderr, "Failed to allocated memory");
        return -1;
    }
    memset(c2, 0, CHALLENGE_SIZE * sizeof(uint16_t));
    

     /* Allocate and initialize the memory to store the strong bits */
    strong_bits = (uint16_t *) malloc(USHRT_MAX * sizeof(uint16_t));
    if (strong_bits == NULL) {
        fprintf(stderr, "Failed to allocated memory");
        return -1;
    }
    memset(strong_bits, 0, USHRT_MAX * sizeof(uint16_t));

    return 0;
}

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

int setMode(int mode) {
    int rc = 0;

    /* Set slave mode */
    switch (mode) {
        case WRITE_H:
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
        case WRITE_C1:
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
        case WRITE_C2:
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
        case WRITE_TS:
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
        default:
            printf("Incorrect Mode\n");
            return -1;
    }
    return 0;
}


int get_challenge() {
    // char c1[3] = "c87";
    // char c2[3] = "c77";
    int strong_bit = 0;
    char location[10];
    FILE *fp;

    // TODO: select two random challenges
    if ((fp = fopen("../Enrollment/c87", "r")) == NULL) {
        printf("Error Opening File");
        return -1;
    }

    while (fgets(location, sizeof(location), fp) != NULL) {
        c1[strong_bit] = atoi(location);
        strong_bit++;
    }
    fclose(fp);

    strong_bit = 0;
    if ((fp = fopen("../Enrollment/c77", "r")) == NULL) {
        printf("Error Opening File");
        return -1;
    }

    while (fgets(location, sizeof(location), fp) != NULL) {
        c2[strong_bit] = atoi(location);
        strong_bit++;
    }
    fclose(fp);
    
    return 0;
}

int get_response() {
    int strong_bit = 0;
    char location[10];
    FILE *fp;

    // TODO: select two random challenges
    if ((fp = fopen("../Enrollment/r87", "r")) == NULL) {
        printf("Error Opening File");
        return -1;
    }

    while (fgets(location, sizeof(location), fp) != NULL) {
        r1[strong_bit] = atoi(location);
        strong_bit++;
    }
    fclose(fp);

    strong_bit = 0;
    if ((fp = fopen("../Enrollment/r77", "r")) == NULL) {
        printf("Error Opening File");
        return -1;
    }

    while (fgets(location, sizeof(location), fp) != NULL) {
        r2[strong_bit] = atoi(location);
        strong_bit++;
    }
    fclose(fp);
    
    return 0;
}

uint32_t get_H(time_t ts) {
    uint32_t hash_len = 0;
    uint32_t timestamp = (uint32_t) ts;
    uint8_t tmp_h[(CHALLENGE_SIZE/8)*2+4];

    // EVP_MD_CTX *mdctx;

    memset(tmp_h, 0, sizeof(tmp_h));

    // concat R1, R2, timestamp
    memcpy(tmp_h, r1, (CHALLENGE_SIZE / 8) * sizeof(uint8_t));
    memcpy(tmp_h + (CHALLENGE_SIZE / 8), r2, (CHALLENGE_SIZE / 8) * sizeof(uint8_t));
    memcpy(tmp_h + (CHALLENGE_SIZE / 8) * 2, &timestamp, 4);

    hash_len = get_sha256(tmp_h, H);

    printf("H: ");
    for (int i = 0; i < hash_len; i++) {
        printf("%02x", H[i]);
    }
    printf("\n");

    return hash_len;
}

int write_H(uint32_t hash_len) {
    int rc = 0;

    memcpy(registers, H, hash_len*sizeof(uint8_t));
    
    rc = modbus_write_registers(context, 0, hash_len, registers);
    if (rc == -1) {
        fprintf(stderr, "%s\n", modbus_strerror(errno));
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

void get_C2(uint32_t hash_len) {
    int hash_index = 0;
    uint16_t helper = 0;
    uint16_t location = 0;

    for (int i = 0; i < CHALLENGE_SIZE; i++) {
        location = c2[i];
        if (hash_index == hash_len) {
            hash_index = 0;
        }
        memcpy(&helper, &r1_hash[hash_index], 2);
        location ^= helper;
        c2[i] = location;
        hash_index += 2;
    }
}

int write_ts(time_t ts) {
    int rc = 0;
    uint16_t ts_array[2];
    uint32_t helper;
    uint32_t timestamp = (uint32_t) ts;

    memset(ts_array, 0, 2 * sizeof(uint16_t));
    memcpy(&helper, &r1_hash, 4);
    printf("%d\n", timestamp);
    timestamp ^= helper;
    memcpy(ts_array, &timestamp, 4);
    
    rc = modbus_write_registers(context, 0, 2, ts_array);
    if (rc == -1) {
        fprintf(stderr, "%s\n", modbus_strerror(errno));
        return -1;
    }
    
    return 0;
}


// int get_resonse() {
//     FILE *fp;
//     int rc = 0;
//     int section_no = 0;
//     int remainder_bit = 0;
//     char response_name[3];

//     section_no = (int) ceil(CHALLENGE_SIZE/MODBUS_MAX_READ_REGISTERS);

//     for (int i = 0; i < section_no; i++) {
//         rc = modbus_read_registers(context, 0, MODBUS_MAX_READ_REGISTERS, registers);
//         if (rc == -1) {
//             fprintf(stderr, "%s\n", modbus_strerror(errno));
//             return -1;
//         }
//         memcpy(&raw_response[i*MODBUS_MAX_READ_REGISTERS], registers, MODBUS_MAX_READ_REGISTERS*sizeof(uint16_t));
//     }

//     remainder_bit = CHALLENGE_SIZE - MODBUS_MAX_READ_REGISTERS * (int) ceil(CHALLENGE_SIZE/MODBUS_MAX_READ_REGISTERS);
//     if (remainder_bit > 0) {
//         rc = modbus_read_registers(context, 0, remainder_bit, registers);
//         if (rc == -1) {
//             fprintf(stderr, "%s\n", modbus_strerror(errno));
//             return -1;
//         }
//         memcpy(&raw_response[section_no*MODBUS_MAX_READ_REGISTERS], registers, remainder_bit*sizeof(uint16_t));
//     }

//     for (int i=0; i < CHALLENGE_SIZE; i++) {
//         printf("%d ", raw_response[i]);
//     }
//     printf("\n\n");

//     format_response();

//     for (int i=0; i < CHALLENGE_SIZE/8; i++) {
//         printf("%d ", final_response[i]);
//     }
//     printf("\n");

//     return 0;
// }

// void get_file_response() {
//     char challenge_name[3];
//     int strong_bit = 0;
//     char location[10];
//     FILE *fp;
//     uint8_t *file_response;

//     file_response = (uint8_t *) malloc((CHALLENGE_SIZE / 8) * sizeof(uint8_t));
//     if (file_response == NULL) {
//         fprintf(stderr, "Failed to allocated memory");
//         exit(1);
//     }
//     memset(file_response, 0, (CHALLENGE_SIZE / 8) * sizeof(uint8_t));

//     if ((fp = fopen("../Enrollment/r87", "r")) == NULL) {
//         printf("Error Opening File");
//         exit(1);
//     }

//     while (fgets(location, sizeof(location), fp) != NULL) {
//         file_response[strong_bit] = atoi(location);
//         strong_bit++;
//     }
//     fclose(fp);

//     for (int i = 0; i < CHALLENGE_SIZE/8; i++) {
//         printf("%d ", file_response[i]);
//     }
//     printf("\n\n");

// }

int main(int argc, char **argv){
    int rc = 0;
    time_t ts = 0;
    uint32_t hash_len = 0;

    /* Initialize variables */
    if (initialize() != 0) {
        printf("Failed to allocated memory\n");
        exit(EXIT_FAILURE);
    }

    /* Select two CRP to authenticate the device */
    if (get_challenge() != 0) {
        printf("Failed to get challenge\n");
        exit(EXIT_FAILURE);
    }
    if (get_response() != 0) {
        printf("Failed to get response\n");
        exit(EXIT_FAILURE);
    }

    time(&ts);
    /* get_H = H(R1, R2, TS) */
    hash_len = get_H(ts);

    /* Connect to slave TODO: change communication link and add multiple slaves*/
    context = modbus_new_tcp(SLAVE_IP, MODBUS_PORT);

    if (modbus_connect(context) == -1) {
        fprintf(stderr, "Connection failed: %s\n", modbus_strerror(errno));
        modbus_free(context);
        exit(EXIT_FAILURE);
    }

    /* Set slave mode */
    if (setMode(WRITE_H) != 0) {
        printf("Failed to set slave mode\n");
        exit(EXIT_FAILURE);
    }

    /* Write H */
    if (write_H(hash_len) != 0) {
        printf("Failed to write H\n");
        exit(EXIT_FAILURE);
    }

    /* Set slave mode */
    if (setMode(WRITE_C1) != 0) {
        printf("Failed to set slave mode\n");
        exit(EXIT_FAILURE);
    }

    /* write challenge */
    if (write_challenge(c1) != 0) {
        printf("Failed to write challenge\n");
        exit(EXIT_FAILURE);
    }

    hash_len = get_sha256(r1, r1_hash);
    get_C2(hash_len);
    get_sha256(c2, c2_hash);
    for (int i = 0; i < hash_len; i++) {
        printf("%02x", c2_hash[i]);
    }
    printf("\n");

    /* Set slave mode */
    if (setMode(WRITE_C2) != 0) {
        printf("Failed to set slave mode\n");
        exit(EXIT_FAILURE);
    }

    /* write challenge */
    if (write_challenge(c2) != 0) {
        printf("Failed to write challenge\n");
        exit(EXIT_FAILURE);
    }
    
    /* Set slave mode */
    if (setMode(WRITE_TS) != 0) {
        printf("Failed to set slave mode\n");
        exit(EXIT_FAILURE);
    }

    /* write TS */
    if (write_ts(ts) != 0) {
        printf("Failed to write challenge\n");
        exit(EXIT_FAILURE);
    }


    
    
    // get_file_response();

     /* Free the memory */
    free(bits);
    free(registers);
    // free(challenge);

    /* Close the connection */
    modbus_close(context);
    modbus_free(context);
    EVP_cleanup();

    return 0;
}
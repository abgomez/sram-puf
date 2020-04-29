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
#define MAXTIMINGS	85
#define DHTPIN		RPI_GPIO_P1_07


SRAM sram;
modbus_t *context;
modbus_mapping_t *modbus_mapping;


////
int challenge_idx = 0;
int response_idx = 0;
uint8_t *master_hmac;
uint8_t *slave_hmac;
uint8_t *raw_response;
uint8_t *final_response;
uint8_t *response_hash;
uint16_t *challenge;
uint32_t timestamp = 0;
////


int section_no = 0;

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


uint8_t c2_hash[HASH_LEN];
uint8_t buff[32];
uint8_t bits[32 * MAX_PAGE];
uint8_t temperature = 0;

uint8_t h[HASH_LEN];
// uint8_t hmac[HASH_LEN];
uint8_t hash_r1[HASH_LEN];
uint8_t hash_r2[HASH_LEN];
uint8_t raw_r1[CHALLENGE_SIZE];
uint8_t raw_r2[CHALLENGE_SIZE];
uint8_t final_r1[CHALLENGE_SIZE/8];
// uint8_t final_r2[CHALLENGE_SIZE/8];
uint8_t *hmac;
uint8_t *final_r2;

uint16_t c1[CHALLENGE_SIZE];
uint16_t c2[CHALLENGE_SIZE];

// uint16_t response[USHRT_MAX];
// uint16_t challenge[USHRT_MAX];
uint16_t strong_ones[USHRT_MAX];
uint16_t strong_zeros[USHRT_MAX];
uint16_t strongest_ones[USHRT_MAX];
uint16_t strongest_zeros[USHRT_MAX];
uint16_t strongest_ones_tmp[USHRT_MAX];
uint16_t strongest_zeros_tmp[USHRT_MAX];



/*
* Functions Prototype 
*/
int initialize();
void read_challenge(uint16_t *);
void authenticate_master();
int readBit(uint16_t);
void format_response(uint8_t *, uint8_t *);


/////

uint8_t get_mode();
uint32_t get_sha256(uint8_t *, uint8_t *);
uint32_t get_sha256(uint16_t *, uint8_t *); 

void write_H();


int initialize() {
    /* Allocate and initialize the memory to store the modbus mapping */
    modbus_mapping = modbus_mapping_new(MODBUS_MAX_READ_BITS, 0, MODBUS_MAX_READ_REGISTERS, 0);
    if (modbus_mapping == NULL) {
        fprintf(stderr, "Failed to allocate the mapping: %s\n", modbus_strerror(errno));
        modbus_free(context);
        return -1;
    }

    /* Allocate and initialize the memory to store master_hmac */
    master_hmac = (uint8_t *) malloc(HASH_LEN* sizeof(uint8_t));
    if (master_hmac == NULL) {
        fprintf(stderr, "Failed to allocated memory");
        return -1;
    }
    memset(master_hmac, 0, HASH_LEN * sizeof(uint8_t));

    /* Allocate and initialize the memory to store slave_hmac */
    slave_hmac = (uint8_t *) malloc(HASH_LEN* sizeof(uint8_t));
    if (slave_hmac == NULL) {
        fprintf(stderr, "Failed to allocated memory");
        return -1;
    }
    memset(slave_hmac, 0, HASH_LEN * sizeof(uint8_t));

    /* Allocate and initialize the memory to store response_hash */
    response_hash = (uint8_t *) malloc(HASH_LEN* sizeof(uint8_t));
    if (response_hash == NULL) {
        fprintf(stderr, "Failed to allocated memory");
        return -1;
    }
    memset(response_hash, 0, HASH_LEN * sizeof(uint8_t));


    /* Allocate and initialize the memory to store raw_response */
    raw_response = (uint8_t *) malloc((CHALLENGE_SIZE / 8) * sizeof(uint8_t));
    if (raw_response == NULL) {
        fprintf(stderr, "Failed to allocated memory");
        return -1;
    }
    memset(raw_response, 0, (CHALLENGE_SIZE / 8) * sizeof(uint8_t));

    /* Allocate and initialize the memory to store final_response */
    final_response = (uint8_t *) malloc((CHALLENGE_SIZE / 8) * sizeof(uint8_t));
    if (final_response == NULL) {
        fprintf(stderr, "Failed to allocated memory");
        return -1;
    }
    memset(final_response, 0, (CHALLENGE_SIZE / 8) * sizeof(uint8_t));

     /* Allocate and initialize the memory to store challenge */
    challenge = (uint16_t *) malloc(CHALLENGE_SIZE * sizeof(uint16_t));
    if (challenge == NULL) {
        fprintf(stderr, "Failed to allocated memory");
        return -1;
    }
    memset(challenge, 0, CHALLENGE_SIZE * sizeof(uint16_t));



    ////

    /* Allocate and initialize the memory to store final_r2 */
    final_r2 = (uint8_t *) malloc((CHALLENGE_SIZE / 8) * sizeof(uint8_t));
    if (final_r2 == NULL) {
        fprintf(stderr, "Failed to allocated memory");
        return -1;
    }
    memset(final_r2, 0, (CHALLENGE_SIZE / 8) * sizeof(uint8_t));

    /* Allocate and initialize the memory to store hmac */
    hmac = (uint8_t *) malloc(HASH_LEN* sizeof(uint8_t));
    if (hmac == NULL) {
        fprintf(stderr, "Failed to allocated memory");
        return -1;
    }
    memset(hmac, 0, HASH_LEN * sizeof(uint8_t));

    memset(c1, 0, sizeof(c1));
    memset(c2, 0, sizeof(c2));
    memset(raw_r1, 0, sizeof(raw_r1));
    memset(raw_r2, 0, sizeof(raw_r2));
    memset(hash_r1, 0, sizeof(hash_r1));
    memset(hash_r2, 0, sizeof(hash_r2));
    memset(final_r1, 0, sizeof(final_r1));
    // memset(final_r2, 0, sizeof(final_r2));
            // memset(response, 0, USHRT_MAX * sizeof(uint16_t));
    // memset(strong_ones, 0, sizeof(strong_ones));
    // memset(strong_zeros, 0, sizeof(strong_zeros));
    // memset(strongest_ones, 0, sizeof(strongest_ones));
    // memset(strongest_zeros, 0, sizeof(strongest_zeros));
    // memset(strongest_ones_tmp, 0, sizeof(strongest_ones_tmp));
    // memset(strongest_zeros_tmp, 0, sizeof(strongest_zeros_tmp));

    return 0;
}

void reset() {
    challenge_idx = response_idx = 0;
    memset(master_hmac, 0, HASH_LEN * sizeof(uint8_t));
    memset(raw_response, 0, (CHALLENGE_SIZE / 8) * sizeof(uint8_t));
    memset(final_response, 0, (CHALLENGE_SIZE / 8) * sizeof(uint8_t));
    memset(challenge, 0, CHALLENGE_SIZE * sizeof(uint16_t));
    memset(response_hash, 0, HASH_LEN * sizeof(uint8_t));
    memset(slave_hmac, 0, HASH_LEN * sizeof(uint8_t));

    //////


    memset(final_r2, 0, (CHALLENGE_SIZE / 8) * sizeof(uint8_t));

    memset(c1, 0, sizeof(c1));
    memset(c2, 0, sizeof(c2));
    memset(raw_r1, 0, sizeof(raw_r1));
    memset(raw_r2, 0, sizeof(raw_r2));
    memset(hash_r1, 0, sizeof(hash_r1));
    memset(hash_r2, 0, sizeof(hash_r2));
    memset(final_r1, 0, sizeof(final_r1));
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

// void get_response() {
//     for (int i = 0; i < MODBUS_MAX_READ_REGISTERS; i++) {
//         int j = response_idx * MODBUS_MAX_READ_REGISTERS + i;
//         if (j > CHALLENGE_SIZE) {
//             response[j] = 0;
//         }
//         else {
//             response[j] = readBit(challenge[j]);
//         }
//         modbus_mapping->tab_registers[i] = response[j];
//     }
    
//     // for (int i = 0 ; i < MODBUS_MAX_WRITE_REGISTERS; i++) {
//     //     // printf("%d ", readBit(challenge[i]));
//     //     modbus_mapping->tab_registers[i] = readBit(challenge[i]);
//     //     usleep(330000);
//     // }
//     for (int i = 0; i < CHALLENGE_SIZE; i++) {
//         printf("%d ", modbus_mapping->tab_registers[i]);
//     }
//     printf("\n\n");

//     response_idx++;

// }

// void write_H() {
//     memcpy(h, modbus_mapping->tab_registers, HASH_LEN * sizeof(uint8_t));
//     for (int i = 0; i < HASH_LEN; i++) {
//         printf("%02x", h[i]);
//     }
//     printf("\n");
// }

void read_challenge(uint16_t *challenge) {
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

    // for (int i = 0; i < CHALLENGE_SIZE; i++) {
    //     printf("%d ", challenge[i]);
    // }
    // printf("\n\n");

    challenge_idx++;
}

void authenticate_master() {
    uint32_t helper;
    uint8_t message[CHALLENGE_SIZE * 2 + 4];
    unsigned char* key = (unsigned char*)  "10001101101100111111000010100001";
    unsigned char* data = (unsigned char*) "22";
    uint32_t tmp = 1588189708;
    uint8_t ts_bytes[4];

    memset(message, 0, sizeof(message));

    /* get raw response from challenge */
    for (int i = 0; i < CHALLENGE_SIZE; i++) {
        raw_response[i] = readBit(challenge[i]);
    }
    
    /* get final response */
    format_response(raw_response, final_response);
    /* hash final response */
    get_sha256(final_response, response_hash);
    
    /* get original timestamp */
    memcpy(&helper, response_hash, 4);
    timestamp ^= helper;

    /* get hmac from response and timestamp */
    /* concat challenge and timestamp */
    memcpy(message, challenge, CHALLENGE_SIZE * sizeof(uint16_t));
    memcpy(&message[CHALLENGE_SIZE*2], &tmp, 4);

    memcpy(ts_bytes, &tmp, 4);
    
    slave_hmac = HMAC(EVP_sha256(), final_response, 4, ts_bytes, 4, NULL, NULL);

    if (memcmp(slave_hmac, master_hmac, 32) == 0 ) {
        printf("Request verified...\n" );
    }
    else {
        printf("Couldn't autheticate master... \n");
        printf("Restarting connection...\n");
        modbus_close(context);
        /* immediately start waiting for another request again */
        printf("\n\nWaiting for connection...\n");
        reset();
        modbus_tcp_accept(context, &socket);
    }
}

void read_register() {
    uint8_t mode = 0;
    mode = get_mode();
    if (mode == 0) {
        printf("Reading challenge...\n");
        read_challenge(challenge);
    }
    if (mode == 1) {
        printf("Reading timestamp...\n");
        memcpy(&timestamp, modbus_mapping->tab_registers, 2 * sizeof(uint16_t));
    }
    if (mode == 2 ) {
        printf("Reading hmac...\n");
        memcpy(master_hmac, modbus_mapping->tab_registers, HASH_LEN * sizeof(uint8_t));
        printf("Verifying master identity..\n");
        authenticate_master();
    }
}

void format_response(uint8_t *raw_response, uint8_t *final_response) {
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

/*
* Function to read the bit value of a cell 
*/

int readBit(uint16_t location) {
    uint8_t recv_data = 0x00;
    long loc = 0;
    loc = floor(location / 8);
    sram.turn_on();
    recv_data = sram.read_byte(loc);
    sram.turn_off();
    return recv_data >> (7 - (location % 8)) & 1;
}

// void get_response(uint8_t *raw_response, uint16_t *challenge) {
//     for (int i = 0; i < CHALLENGE_SIZE; i++) {
//         raw_response[i] = readBit(challenge[i]);
//         printf("%d ", raw_response[i]);
//     }
//     printf("\n");
// }

void get_c2() {
    int hash_index = 0;
    uint16_t helper = 0;
    uint16_t location = 0;

    for (int i = 0; i < CHALLENGE_SIZE; i++) {
        location = c2[i];
        if (hash_index == HASH_LEN) {
            hash_index = 0;
        }
        memcpy(&helper, &hash_r1[hash_index], 2);
        location ^= helper;
        c2[i] = location;
        hash_index += 2;
    }
}


void write_response() {
    uint8_t tmp_h[(CHALLENGE_SIZE/8)*2+4];

    memcpy(tmp_h, final_r1, (CHALLENGE_SIZE / 8) * sizeof(uint8_t));
    memcpy(tmp_h + (CHALLENGE_SIZE / 8), final_r2, (CHALLENGE_SIZE / 8) * sizeof(uint8_t));
    memcpy(tmp_h + (CHALLENGE_SIZE / 8) * 2, &timestamp, 4);

    get_sha256(tmp_h, h);

    memcpy(modbus_mapping->tab_registers, h, HASH_LEN*sizeof(uint8_t));
    memcpy(&modbus_mapping->tab_registers[HASH_LEN*sizeof(uint8_t)], hmac, HASH_LEN*sizeof(uint8_t));
}

// void read_sensor() {
//     uint32_t helper;

//     /* generate R1 from C1 */
//     get_response(raw_r1, c1);
//     format_response(raw_r1, final_r1);
//     printf("\n");
//     /* get sha256 of R1 */
//     get_sha256(final_r1, hash_r1);
//     /* get C2 from C2' and hash(R1) */
//     get_c2();
    
//     /* generate R2 from C2 */
//     get_response(raw_r2, c2);
//     format_response(raw_r2, final_r2);
//     /* get TS from TS' */
//     // for (int i = 0; i < 32; i++) {
//     //     printf("%02x", hash_r1[i]);
//     // }
//     // printf("\n");
//     memcpy(&helper, hash_r1, 4);
//     // printf("%d - %d\n", timestamp, helper);
//     timestamp ^= helper;
//     // printf("%d\n", timestamp);

//     /* read temperature from sensor */
//     // get_temperature();
//     /* send reading to the RTU */
//     write_response();
// }

// void send_h() {
//     uint32_t helper;
//     uint8_t tmp_h[(CHALLENGE_SIZE/8)*2+4];

//     /* generate R1 from C1 */
//     get_response(raw_r1, c1);
//     format_response(raw_r1, final_r1);
//     printf("\n");
//     /* get sha256 of R1 */
//     get_sha256(final_r1, hash_r1);
//     /* get C2 from C2' and hash(R1) */
//     get_c2();
//     // get_sha256(c2, c2_hash);
//     for (int i = 0; i < 256; i++) {
//         printf("%d ", c2[i]);
//     }
//     printf("\n");
//     /* generate R2 from C2 */
//     get_response(raw_r2, c2);
//     format_response(raw_r2, final_r2);
//     /* get TS from TS' */
//     memcpy(&helper, hash_r1, 4);
//     timestamp ^= helper;

//     memcpy(tmp_h, final_r1, (CHALLENGE_SIZE / 8) * sizeof(uint8_t));
//     memcpy(tmp_h + (CHALLENGE_SIZE / 8), final_r2, (CHALLENGE_SIZE / 8) * sizeof(uint8_t));
//     memcpy(tmp_h + (CHALLENGE_SIZE / 8) * 2, &timestamp, 4);

//     get_sha256(tmp_h, h);

//     memcpy(modbus_mapping->tab_registers, h, HASH_LEN*sizeof(uint8_t));
// }

uint8_t read_dht11() {
    int dht11_dat[5] = { 0, 0, 0, 0, 0 };
	uint8_t laststate	= HIGH;
	uint8_t counter		= 0;
	uint8_t j		= 0, i;
	float	f; 

    if (!bcm2835_init()) {
        printf("bcm2835_init failed. Are you running as root??\n");
        exit(1);
    }

	bcm2835_gpio_fsel(DHTPIN, BCM2835_GPIO_FSEL_OUTP);
	bcm2835_gpio_write(DHTPIN, LOW);
	delay( 18 );
	bcm2835_gpio_write(DHTPIN, HIGH);
	delayMicroseconds( 40 );
	bcm2835_gpio_fsel(DHTPIN, BCM2835_GPIO_FSEL_INPT);
 
	for ( i = 0; i < MAXTIMINGS; i++ ) {
		counter = 0;
		
		while ( bcm2835_gpio_lev(DHTPIN) == laststate ) {
			counter++;
			delayMicroseconds( 1 );
			if ( counter == 255 ) {
				break;
			}
		}
		laststate = bcm2835_gpio_lev( DHTPIN );
 
		if ( counter == 255 ) {
            break;
        }
 
		if ( (i >= 4) && (i % 2 == 0) ) {
			dht11_dat[j / 8] <<= 1;
			if ( counter > 50 ) {
                dht11_dat[j / 8] |= 1;
            }
			j++;
		}
	}
 
	if ( (j >= 40) && (dht11_dat[4] == ( (dht11_dat[0] + dht11_dat[1] + dht11_dat[2] + dht11_dat[3]) & 0xFF) ) ) {
        return (uint8_t) dht11_dat[2];
	} else  {
		return (uint8_t) 0;
	}
}

// void get_temperature() {
//     uint8_t temperature = 0;

//     while (temperature == 0) {
//         temperature = read_dht11();
//     }
//     hmac = HMAC(EVP_sha256(), final_r2, 32, &temperature, 1, NULL, NULL);

//     for (int i = 0; i < HASH_LEN; i++) {
//         printf("%02x", hmac[i]);
//     }
//     printf("\n");
// }


void send_sensor_read() {
    while (temperature == 0) {
        temperature = read_dht11();
    }

    printf("Temperature: %d\n", temperature);
    memcpy(modbus_mapping->tab_registers, &temperature, sizeof(uint8_t));
}

void write_register() {
    uint8_t mode = 0;

    mode = get_mode();

    if (mode == 3) {
        printf("Writting sensor data..\n");
        send_sensor_read();
    }
    else if (mode == 4) {
        printf("Writting HMAC..\n");
        hmac = HMAC(EVP_sha256(), final_response, 4, &temperature, 1, NULL, NULL);
        memcpy(modbus_mapping->tab_registers, hmac, HASH_LEN*sizeof(uint8_t));
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

    initialize();

    for(;;) {
        uint8_t query[MODBUS_TCP_MAX_ADU_LENGTH];
        uint8_t function_code;
        uint8_t mode;

        rc = modbus_receive(context, query);
        function_code = query[modbus_get_header_length(context)];
        if (rc >= 0) {
            printf("Replying to request: 0x%02X ", function_code);
            if (function_code == 0x03) {
                write_register();
            }
            modbus_reply(context, query, rc, modbus_mapping);
            if (function_code == 0x0F) {
                printf("(Set Mode)\n");
                challenge_idx = response_idx = 0;
            }
            else if (function_code == 0x10) {
                read_register();
            }
        } else if (rc == -1) {
            /* Connection closed by the client or server */
            modbus_close(context);
            /* immediately start waiting for another request again */
            printf("\n\nWaiting for connection...\n");
            reset();
            modbus_tcp_accept(context, &socket);
        }
    }

    modbus_mapping_free(modbus_mapping);
    close(socket);
    modbus_free(context);

    return 0;
}

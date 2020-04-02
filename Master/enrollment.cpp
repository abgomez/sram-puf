/*
* Created by Abel Gomez 03/31/2020
*/

#include <string.h>
#include <stdlib.h>
#include <modbus.h>
#include <stdio.h>
#include <errno.h>
#include <time.h>


#define CHALLENGE_SIZE MODBUS_MAX_WRITE_REGISTERS
#define SLAVE_IP "127.0.0.1"
#define MODBUS_PORT 1502
#define GET_CHALLENGE 0  //0b00
#define GET_RESPONSE 1   //0b01

modbus_t *context;
uint8_t  *bit;
uint8_t  *slave_mode;
uint8_t  *response;
uint16_t *registers;
uint16_t *challenge;


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
    response = (uint8_t *) malloc(CHALLENGE_SIZE * sizeof(uint8_t));
    if (response == NULL) {
        fprintf(stderr, "Failed to allocated memory");
        return -1;
    }
    memset(response, 0, CHALLENGE_SIZE * sizeof(uint8_t));

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
    printf("\n");
    return 0;

}

int getChallenge() {
    FILE *fp;
    int rc = 0;
    int challenge_no = 0;
    char challenge_name[3];
    
    rc = modbus_read_registers(context, 0, CHALLENGE_SIZE, registers);
    
    if (rc == -1) {
        fprintf(stderr, "%s\n", modbus_strerror(errno));
        return -1;
    }

    srand(time(NULL));
    challenge_no = rand() % 99;
    // challenge[0] = 'c';
    sprintf(challenge_name, "c%d", challenge_no);
    fp = fopen(challenge_name, "w+");
    for (int i=0; i < CHALLENGE_SIZE; i++) {
        challenge[i] = registers[i];
        printf("%ld ", challenge[i]);
        fprintf(fp, "%d\n", challenge[i]);
    }
    printf("\n");
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
    
     /* Free the memory */
    free(bit);
    free(registers);
    free(challenge);

    /* Close the connection */
    modbus_close(context);
    modbus_free(context);
    return 0;
}





// #include <string.h>
// #include <stdlib.h>
// #include <time.h>

// /* Tests based on PI-MBUS-300 documentation */
// int main(int argc, char *argv[])
// {
    
    
//     int i;
//     int nb_points;
//     double elapsed;
//     uint32_t start;
//     uint32_t end;
//     uint32_t bytes;
//     uint32_t rate;
//     int rc;
//     int n_loop;
//     int use_backend;
//     uint16_t send_data[5];

    // if (argc > 1) {
    //     if (strcmp(argv[1], "tcp") == 0) {
    //         use_backend = TCP;
    //         n_loop = 100000;
    //     } else if (strcmp(argv[1], "rtu") == 0) {
    //         use_backend = RTU;
    //         n_loop = 100;
    //     } else {
    //         printf("Usage:\n  %s [tcp|rtu] - Modbus client to measure data bandwith\n\n", argv[0]);
    //         exit(1);
    //     }
    // } else {
    //     /* By default */
    //     use_backend = TCP;
    //     n_loop = 100000;
    // }

    // if (use_backend == TCP) {
    //     ctx = modbus_new_tcp("127.0.0.1", 1502);
    // } else {
    //     ctx = modbus_new_rtu("/dev/ttyUSB1", 115200, 'N', 8, 1);
    //     modbus_set_slave(ctx, 1);
    // }
    // n_loop = 100000;
    
    
    // /* Allocate and initialize the memory to store the status */
    // tab_bit = (uint8_t *) malloc(MODBUS_MAX_READ_BITS * sizeof(uint8_t));
    // memset(tab_bit, 0, MODBUS_MAX_READ_BITS * sizeof(uint8_t));

    // /* Allocate and initialize the memory to store the registers */
    // tab_reg = (uint16_t *) malloc(MODBUS_MAX_READ_REGISTERS * sizeof(uint16_t));
    // memset(tab_reg, 0, MODBUS_MAX_READ_REGISTERS * sizeof(uint16_t));

    // memset(send_data, 0, 5 * sizeof(uint16_t));

    // printf("READ BITS\n");

    // rc = modbus_read_bits(ctx, 0, 2, tab_bit);
    // for (int i=0; i < rc; i++) {
    //     printf("reg[%d]=%d (0x%X)\n", i, tab_bit[i], tab_bit[i]);
    // }

    // nb_points = MODBUS_MAX_READ_BITS;
    // start = gettime_ms();
    // for (i=0; i<n_loop; i++) {
    //     rc = modbus_read_bits(ctx, 0, nb_points, tab_bit);
    //     if (rc == -1) {
    //         fprintf(stderr, "%s\n", modbus_strerror(errno));
    //         return -1;
    //     }
    // }
    // end = gettime_ms();
    // elapsed = end - start;

    // rate = (n_loop * nb_points) * G_MSEC_PER_SEC / (end - start);
    // printf("Transfert rate in points/seconds:\n");
    // printf("* %d points/s\n", rate);
    // printf("\n");

    // bytes = n_loop * (nb_points / 8) + ((nb_points % 8) ? 1 : 0);
    // rate = bytes / 1024 * G_MSEC_PER_SEC / (end - start);
    // printf("Values:\n");
    // printf("* %d x %d values\n", n_loop, nb_points);
    // printf("* %.3f ms for %d bytes\n", elapsed, bytes);
    // printf("* %d KiB/s\n", rate);
    // printf("\n");

    // /* TCP: Query and reponse header and values */
    // bytes = 12 + 9 + (nb_points / 8) + ((nb_points % 8) ? 1 : 0);
    // printf("Values and TCP Modbus overhead:\n");
    // printf("* %d x %d bytes\n", n_loop, bytes);
    // bytes = n_loop * bytes;
    // rate = bytes / 1024 * G_MSEC_PER_SEC / (end - start);
    // printf("* %.3f ms for %d bytes\n", elapsed, bytes);
    // printf("* %d KiB/s\n", rate);
    // printf("\n\n");

    // printf("WRITE REGISTERS\n");
    // send_data[0] = 0x41;
    // send_data[1] = 0x42;
    // send_data[2] = 0x45;
    // send_data[3] = 0x4C;
    // send_data[4] = 0x42;
    // rc = modbus_write_registers(ctx, 0, 4, send_data);
    // if (rc == -1) {
    //     fprintf(stderr, "%s\n", modbus_strerror(errno));
    //     return -1;
    // }
    // for (int i=0; i < rc; i++) {
    //     printf("reg[%d]=%d (%c)\n", i, tab_reg[i], tab_reg[i]);
    // }

    // printf("READ REGISTERS\n");

    // nb_points = MODBUS_MAX_READ_REGISTERS;
    // // start = gettime_ms();
    // // for (i=0; i<n_loop; i++) {
    // //     rc = modbus_read_registers(ctx, 0, nb_points, tab_reg);
    // //     if (rc == -1) {
    // //         fprintf(stderr, "%s\n", modbus_strerror(errno));
    // //         return -1;
    // //     }
    // // }

    // rc = modbus_read_registers(ctx, 0, 4, tab_reg);
    // for (int i=0; i < rc; i++) {
    //     printf("reg[%d]=%d (%c)\n", i, tab_reg[i], tab_reg[i]);
    // }
    // // end = gettime_ms();
    // elapsed = end - start;

    // rate = (n_loop * nb_points) * G_MSEC_PER_SEC / (end - start);
    // printf("Transfert rate in points/seconds:\n");
    // printf("* %d registers/s\n", rate);
    // printf("\n");

    // bytes = n_loop * nb_points * sizeof(uint16_t);
    // rate = bytes / 1024 * G_MSEC_PER_SEC / (end - start);
    // printf("Values:\n");
    // printf("* %d x %d values\n", n_loop, nb_points);
    // printf("* %.3f ms for %d bytes\n", elapsed, bytes);
    // printf("* %d KiB/s\n", rate);
    // printf("\n");

    // /* TCP:Query and reponse header and values */
    // bytes = 12 + 9 + (nb_points * sizeof(uint16_t));
    // printf("Values and TCP Modbus overhead:\n");
    // printf("* %d x %d bytes\n", n_loop, bytes);
    // bytes = n_loop * bytes;
    // rate = bytes / 1024 * G_MSEC_PER_SEC / (end - start);
    // printf("* %.3f ms for %d bytes\n", elapsed, bytes);
    // printf("* %d KiB/s\n", rate);
    // printf("\n\n");

    

//     /* Free the memory */
//     free(tab_bit);
//     free(tab_reg);

//     /* Close the connection */
//     modbus_close(ctx);
//     modbus_free(ctx);

//     return 0;
// }
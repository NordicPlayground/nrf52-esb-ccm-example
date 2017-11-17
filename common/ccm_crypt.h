#ifndef __CCM_CRYPT_H
#define __CCM_CRYPT_H

#include <stdint.h>
#include <stdbool.h>
#include "nrf.h"

enum {CCM_CRYPT_SUCCESS = 0, CCM_CRYPT_INVALID_MIC, CCM_CRYPT_RUNTIME_ERROR};

// Fixed parameters (should not be changed)
#define CCM_KEY_SIZE        16
#define CCM_IV_SIZE         8
#define CCM_COUNTER_SIZE    5
#define CCM_MIC_SIZE        4

#define CCM_IN_HEADER_INDEX 0
#define CCM_IN_LENGTH_INDEX 1
#define CCM_IN_RFU_INDEX 2
#define CCM_IN_PAYLOAD_INDEX 3

// Configurable parameters
#define DATA_LENGTH 28

// Derived parameters (should not be changed)
#if (DATA_LENGTH > (43 - 16))
#define SCRATCH_AREA_SIZE (DATA_LENGTH + 16)
#else
#define SCRATCH_AREA_SIZE 43
#endif

// Types
typedef uint8_t* ccm_passkey_t;

typedef struct
{
    uint8_t  key[CCM_KEY_SIZE];
    uint64_t counter;
    uint8_t  direction;
    uint8_t  iv[CCM_IV_SIZE];    
}ccm_data_t;

typedef struct 
{
    uint8_t header;
    uint8_t length;
    uint8_t rfu;
    uint8_t payload[DATA_LENGTH];    
}ccm_clear_text_t;

typedef struct 
{
    uint8_t header;
    uint8_t length;
    uint8_t rfu;
    uint8_t payload[DATA_LENGTH];
    uint8_t mic[CCM_MIC_SIZE];
}ccm_cipher_text_t;

typedef struct
{
    uint8_t key[CCM_KEY_SIZE];
    uint8_t iv[CCM_IV_SIZE];
}ccm_pair_request_packet_t;

// Functions

void ccm_rng_fill_buffer(uint8_t *buf, uint32_t bufsize);
    
void ccm_crypt_init(void);

uint32_t ccm_crypt_pair_request_prepare(ccm_passkey_t passkey, ccm_pair_request_packet_t *request_out);

uint32_t ccm_crypt_pair_request_accept(ccm_passkey_t passkey, ccm_pair_request_packet_t *request);

uint32_t ccm_crypt_packet_encrypt(uint8_t *packet, uint32_t packet_length, uint8_t *out_packet);

uint32_t ccm_crypt_packet_decrypt(uint8_t *packet, uint32_t packet_length, uint8_t *out_packet);

#endif

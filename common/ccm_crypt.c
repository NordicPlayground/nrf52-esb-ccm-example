#include "ccm_crypt.h"
#include <string.h>

static uint8_t ccm_scratch_area[SCRATCH_AREA_SIZE];

static ccm_data_t m_ccm_config;
static ccm_pair_request_packet_t m_pair_request_packet;
static uint8_t clear_buffer[1 + 1 + 1 + DATA_LENGTH];
static uint8_t cipher_buffer[1 + 1 + 1 + DATA_LENGTH + 4];


void ccm_rng_fill_buffer(uint8_t *buf, uint32_t bufsize)
{
    NRF_RNG->CONFIG = RNG_CONFIG_DERCEN_Enabled << RNG_CONFIG_DERCEN_Pos;
    while(bufsize--)
    {
        NRF_RNG->EVENTS_VALRDY = 0;
        NRF_RNG->TASKS_START = 1;
        while(NRF_RNG->EVENTS_VALRDY == 0);
        *buf++ = NRF_RNG->VALUE;
    }
}


void ccm_crypt_init(void)
{
    NRF_CCM->ENABLE = CCM_ENABLE_ENABLE_Enabled << CCM_ENABLE_ENABLE_Pos;
    NRF_CCM->SCRATCHPTR = (uint32_t)ccm_scratch_area;
}


uint32_t ccm_crypt_pair_request_prepare(ccm_passkey_t passkey, ccm_pair_request_packet_t *request_out)
{
    ccm_rng_fill_buffer(m_pair_request_packet.key, CCM_KEY_SIZE);
    ccm_rng_fill_buffer(m_pair_request_packet.iv,  CCM_IV_SIZE);
    memcpy(m_ccm_config.key, m_pair_request_packet.key, CCM_KEY_SIZE);
    memcpy(m_ccm_config.iv,  m_pair_request_packet.iv,  CCM_IV_SIZE);
    m_ccm_config.direction = 0;
    m_ccm_config.counter = 0;
    NRF_CCM->CNFPTR = (uint32_t)&m_ccm_config;
    memcpy(request_out, &m_pair_request_packet, sizeof(ccm_pair_request_packet_t));
    return CCM_CRYPT_SUCCESS;
}


uint32_t ccm_crypt_pair_request_accept(ccm_passkey_t passkey, ccm_pair_request_packet_t *request)
{
    memcpy(m_ccm_config.key, request->key, CCM_KEY_SIZE);
    memcpy(m_ccm_config.iv,  request->iv,  CCM_IV_SIZE);
    m_ccm_config.direction = 0;
    m_ccm_config.counter = 0;
    NRF_CCM->CNFPTR = (uint32_t)&m_ccm_config;
    return CCM_CRYPT_SUCCESS;
}


uint32_t ccm_crypt_packet_encrypt(uint8_t *packet, uint32_t packet_length, uint8_t *out_packet)
{
    clear_buffer[CCM_IN_HEADER_INDEX] = 0;
    clear_buffer[CCM_IN_LENGTH_INDEX] = (uint8_t)packet_length;
    clear_buffer[CCM_IN_RFU_INDEX] = 0;
    memcpy(&clear_buffer[CCM_IN_PAYLOAD_INDEX], packet, packet_length);
    NRF_CCM->INPTR = (uint32_t)clear_buffer;
    NRF_CCM->OUTPTR = (uint32_t)cipher_buffer;
    
    NRF_CCM->MODE = CCM_MODE_MODE_Encryption << CCM_MODE_MODE_Pos |
                    CCM_MODE_DATARATE_1Mbit << CCM_MODE_DATARATE_Pos |
                    CCM_MODE_LENGTH_Default << CCM_MODE_LENGTH_Pos;   
    
    // Generate a key
    NRF_CCM->EVENTS_ENDKSGEN = 0;
    NRF_CCM->EVENTS_ERROR = 0;
    NRF_CCM->TASKS_KSGEN = 1;
    while(NRF_CCM->EVENTS_ENDKSGEN == 0);
    if(NRF_CCM->EVENTS_ERROR)
    {
        return CCM_CRYPT_RUNTIME_ERROR;
    }

    // Encrypt the packet
    NRF_CCM->EVENTS_ENDCRYPT = 0;
    NRF_CCM->EVENTS_ERROR = 0;
    NRF_CCM->TASKS_CRYPT = 1;
    while(NRF_CCM->EVENTS_ENDCRYPT == 0);
    if(NRF_CCM->EVENTS_ERROR)
    {
        return CCM_CRYPT_RUNTIME_ERROR;
    }

    // Auto increment the counter
    m_ccm_config.counter++;
    
    memcpy(out_packet, &cipher_buffer[CCM_IN_PAYLOAD_INDEX], packet_length + 4);
    return CCM_CRYPT_SUCCESS;    
}


uint32_t ccm_crypt_packet_decrypt(uint8_t *packet, uint32_t packet_length, uint8_t *out_packet)
{
    cipher_buffer[CCM_IN_HEADER_INDEX] = 0;
    cipher_buffer[CCM_IN_LENGTH_INDEX] = (uint8_t)packet_length;
    cipher_buffer[CCM_IN_RFU_INDEX] = 0;
    memcpy(&cipher_buffer[CCM_IN_PAYLOAD_INDEX], packet, packet_length);
    NRF_CCM->INPTR = (uint32_t)cipher_buffer;
    NRF_CCM->OUTPTR = (uint32_t)clear_buffer;
    
    NRF_CCM->MODE = CCM_MODE_MODE_Decryption << CCM_MODE_MODE_Pos |
                    CCM_MODE_DATARATE_1Mbit << CCM_MODE_DATARATE_Pos |
                    CCM_MODE_LENGTH_Default << CCM_MODE_LENGTH_Pos; 

    // Generate a key
    NRF_CCM->EVENTS_ENDKSGEN = 0;
    NRF_CCM->EVENTS_ERROR = 0;
    NRF_CCM->TASKS_KSGEN = 1;
    while(NRF_CCM->EVENTS_ENDKSGEN == 0);
    if(NRF_CCM->EVENTS_ERROR)
    {
        return CCM_CRYPT_RUNTIME_ERROR;
    }
    
    // decrypt the packet    
    NRF_CCM->EVENTS_ENDCRYPT = 0;
    NRF_CCM->EVENTS_ERROR = 0;
    NRF_CCM->TASKS_CRYPT = 1;
    while(NRF_CCM->EVENTS_ENDCRYPT == 0);
    if(NRF_CCM->EVENTS_ERROR)
    {
        return CCM_CRYPT_RUNTIME_ERROR;
    }
    if(NRF_CCM->MICSTATUS == (CCM_MICSTATUS_MICSTATUS_CheckFailed << CCM_MICSTATUS_MICSTATUS_Pos))
    {
        return CCM_CRYPT_INVALID_MIC;
    }
    
    // Auto increment the counter
    m_ccm_config.counter++;

    memcpy(out_packet, &clear_buffer[CCM_IN_PAYLOAD_INDEX], packet_length - 4);    
    return CCM_CRYPT_SUCCESS;    
}

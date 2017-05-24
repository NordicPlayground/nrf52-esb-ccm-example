/**
 * Copyright (c) 2014 - 2017, Nordic Semiconductor ASA
 * 
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 * 
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 * 
 * 2. Redistributions in binary form, except as embedded into a Nordic
 *    Semiconductor ASA integrated circuit in a product or a software update for
 *    such product, must reproduce the above copyright notice, this list of
 *    conditions and the following disclaimer in the documentation and/or other
 *    materials provided with the distribution.
 * 
 * 3. Neither the name of Nordic Semiconductor ASA nor the names of its
 *    contributors may be used to endorse or promote products derived from this
 *    software without specific prior written permission.
 * 
 * 4. This software, with or without modification, must only be used with a
 *    Nordic Semiconductor ASA integrated circuit.
 * 
 * 5. Any software provided in binary form under this license must not be reverse
 *    engineered, decompiled, modified and/or disassembled.
 * 
 * THIS SOFTWARE IS PROVIDED BY NORDIC SEMICONDUCTOR ASA "AS IS" AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY, NONINFRINGEMENT, AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL NORDIC SEMICONDUCTOR ASA OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
 * GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 * 
 */
#include "nrf_esb.h"

#include <stdbool.h>
#include <stdint.h>
#include "sdk_common.h"
#include "nrf.h"
#include "nrf_esb_error_codes.h"
#include "nrf_delay.h"
#include "nrf_gpio.h"
#include "nrf_error.h"
#include "boards.h"
#include "app_defines.h"
#include "ccm_crypt.h"

#define NRF_LOG_MODULE_NAME "APP"
#include "nrf_log.h"
#include "nrf_log_ctrl.h"

uint8_t led_nr;

nrf_esb_payload_t rx_payload;

static ccm_pair_request_packet_t pair_request_packet;

static volatile bool rx_packet_received = false;

/*lint -save -esym(40, BUTTON_1) -esym(40, BUTTON_2) -esym(40, BUTTON_3) -esym(40, BUTTON_4) -esym(40, LED_1) -esym(40, LED_2) -esym(40, LED_3) -esym(40, LED_4) */

void nrf_esb_event_handler(nrf_esb_evt_t const * p_event)
{
    switch (p_event->evt_id)
    {
        case NRF_ESB_EVENT_TX_SUCCESS:
            NRF_LOG_DEBUG("TX SUCCESS EVENT\r\n");
            break;
        case NRF_ESB_EVENT_TX_FAILED:
            NRF_LOG_DEBUG("TX FAILED EVENT\r\n");
            break;
        case NRF_ESB_EVENT_RX_RECEIVED:
            NRF_LOG_DEBUG("RX RECEIVED EVENT\r\n");
            rx_packet_received = true;
            break;
    }
}


void clocks_start( void )
{
    NRF_CLOCK->EVENTS_HFCLKSTARTED = 0;
    NRF_CLOCK->TASKS_HFCLKSTART = 1;

    while (NRF_CLOCK->EVENTS_HFCLKSTARTED == 0);
}


void gpio_init( void )
{
    bsp_board_leds_init();
    bsp_board_buttons_init();
}


uint32_t esb_init( void )
{
    uint32_t err_code;
    uint8_t base_addr_0[4] = {0xE7, 0xE7, 0xE7, 0xE7};
    uint8_t base_addr_1[4] = {0xC2, 0xC2, 0xC2, 0xC2};
    uint8_t addr_prefix[8] = {0xE7, 0xC2, 0xC3, 0xC4, 0xC5, 0xC6, 0xC7, 0xC8 };
    nrf_esb_config_t nrf_esb_config         = NRF_ESB_DEFAULT_CONFIG;
    nrf_esb_config.payload_length           = 8;
    nrf_esb_config.protocol                 = NRF_ESB_PROTOCOL_ESB_DPL;
    nrf_esb_config.bitrate                  = NRF_ESB_BITRATE_2MBPS;
    nrf_esb_config.mode                     = NRF_ESB_MODE_PRX;
    nrf_esb_config.event_handler            = nrf_esb_event_handler;
    nrf_esb_config.selective_auto_ack       = false;

    err_code = nrf_esb_init(&nrf_esb_config);
    VERIFY_SUCCESS(err_code);

    err_code = nrf_esb_set_base_address_0(base_addr_0);
    VERIFY_SUCCESS(err_code);

    err_code = nrf_esb_set_base_address_1(base_addr_1);
    VERIFY_SUCCESS(err_code);
    
    err_code = nrf_esb_set_prefixes(addr_prefix, 8);
    VERIFY_SUCCESS(err_code);

    return err_code;
}


int main(void)
{
    uint32_t err_code;
    uint8_t decrypted_packet[32];
    bool encryption_enabled = false;

    gpio_init();

    err_code = NRF_LOG_INIT(NULL);
    APP_ERROR_CHECK(err_code);

    clocks_start();

    err_code = esb_init();
    APP_ERROR_CHECK(err_code);
    
    ccm_crypt_init();

    NRF_LOG_INFO("ESB CCM example started (PRX)\r\n");

    err_code = nrf_esb_start_rx();
    APP_ERROR_CHECK(err_code);

    while (true)
    {
        if(rx_packet_received)
        {
            rx_packet_received = false;
            while (nrf_esb_read_rx_payload(&rx_payload) == NRF_SUCCESS)
            {
                switch(rx_payload.data[0])
                {
                    case APP_CMD_PAYLOAD:
                        NRF_LOG_INFO(" Payload received\r\n");
                        NRF_LOG_HEXDUMP_INFO(&rx_payload.data[1], rx_payload.length-1);
                        break;
                    
                    case APP_CMD_PAIR_REQUEST:
                        NRF_LOG_INFO(" Pairing request received\r\n");
                        memcpy(&pair_request_packet, &rx_payload.data[1], sizeof(pair_request_packet));
                        ccm_crypt_pair_request_accept(0, &pair_request_packet);
                        encryption_enabled = true;
                        break;
                    
                    case APP_CMD_ENCRYPTED_PAYLOAD:
                        if(encryption_enabled)
                        {
                            NRF_LOG_INFO(" Encrypted payload received\r\n");
                            NRF_LOG_HEXDUMP_INFO(&rx_payload.data[1], rx_payload.length-1);                        
                            err_code = ccm_crypt_packet_decrypt(&rx_payload.data[1], rx_payload.length - 1, decrypted_packet);
                            if(err_code == CCM_CRYPT_SUCCESS)
                            {
                                NRF_LOG_HEXDUMP_INFO(decrypted_packet, rx_payload.length - 1 - 4);
                                decrypted_packet[0] ? bsp_board_led_on(0) : bsp_board_led_off(0);
                                decrypted_packet[1] ? bsp_board_led_on(1) : bsp_board_led_off(1);
                                decrypted_packet[2] ? bsp_board_led_on(2) : bsp_board_led_off(2);
                            }
                            else if(err_code == CCM_CRYPT_INVALID_MIC)
                            {
                                NRF_LOG_WARNING("Invalid MIC!\r\n");
                            }
                        }
                        else
                        {
                            NRF_LOG_WARNING("Encrypted packet received without having enabled encryption!\r\n");
                        }
                        break;                    
                }
            }
        }
        if (NRF_LOG_PROCESS() == false)
        {
            __WFE();
        }
    }
}
/*lint -restore */

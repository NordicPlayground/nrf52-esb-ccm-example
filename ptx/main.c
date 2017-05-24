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
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include "sdk_common.h"
#include "nrf.h"
#include "nrf_esb.h"
#include "nrf_error.h"
#include "nrf_esb_error_codes.h"
#include "nrf_delay.h"
#include "nrf_gpio.h"
#include "boards.h"
#include "nrf_delay.h"
#include "app_util.h"
#include "app_timer.h"
#include "app_defines.h"
#include "ccm_crypt.h"
#define NRF_LOG_MODULE_NAME "APP"
#include "nrf_log.h"
#include "nrf_log_ctrl.h"

APP_TIMER_DEF(m_update_timer_id); 

static nrf_esb_payload_t            tx_payload = NRF_ESB_CREATE_PAYLOAD(0, 0x01, 0x00, 0x00, 0x00, 0x11, 0x00, 0x00, 0x00);

static nrf_esb_payload_t            rx_payload;

static ccm_pair_request_packet_t    pair_request_packet;

static bool encryption_enabled = false;

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
            (void) nrf_esb_flush_tx();
            (void) nrf_esb_start_tx();
            break;
        case NRF_ESB_EVENT_RX_RECEIVED:
            NRF_LOG_DEBUG("RX RECEIVED EVENT\r\n");
            while (nrf_esb_read_rx_payload(&rx_payload) == NRF_SUCCESS)
            {
                if (rx_payload.length > 0)
                {
                    NRF_LOG_DEBUG("RX RECEIVED PAYLOAD\r\n");
                }
            }
            break;
    }
    NRF_GPIO->OUTCLR = 0xFUL << 12;
    NRF_GPIO->OUTSET = (p_event->tx_attempts & 0x0F) << 12;
}


void clocks_start( void )
{
    NRF_CLOCK->EVENTS_HFCLKSTARTED = 0;
    NRF_CLOCK->TASKS_HFCLKSTART = 1;

    while (NRF_CLOCK->EVENTS_HFCLKSTARTED == 0);
}


void gpio_init( void )
{
    bsp_board_buttons_init();
    bsp_board_leds_init();
}


uint32_t esb_init( void )
{
    uint32_t err_code;
    uint8_t base_addr_0[4] = {0xE7, 0xE7, 0xE7, 0xE7};
    uint8_t base_addr_1[4] = {0xC2, 0xC2, 0xC2, 0xC2};
    uint8_t addr_prefix[8] = {0xE7, 0xC2, 0xC3, 0xC4, 0xC5, 0xC6, 0xC7, 0xC8 };

    nrf_esb_config_t nrf_esb_config         = NRF_ESB_DEFAULT_CONFIG;
    nrf_esb_config.protocol                 = NRF_ESB_PROTOCOL_ESB_DPL;
    nrf_esb_config.retransmit_delay         = 600;
    nrf_esb_config.bitrate                  = NRF_ESB_BITRATE_2MBPS;
    nrf_esb_config.event_handler            = nrf_esb_event_handler;
    nrf_esb_config.mode                     = NRF_ESB_MODE_PTX;
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

static void send_payload(uint8_t *payload, uint32_t payload_length)
{
    NRF_LOG_DEBUG("Transmitting packet %02x\r\n", payload[0]);
    tx_payload.noack = false;
    tx_payload.data[0] = APP_CMD_PAYLOAD;
    memcpy(&tx_payload.data[1], payload, payload_length);
    tx_payload.length = payload_length + 1;
    if (nrf_esb_write_payload(&tx_payload) != NRF_SUCCESS)
    {
        NRF_LOG_WARNING("Sending packet failed\r\n");
    }
}

static void send_pairing_request()
{
    NRF_LOG_INFO("Transmitting pair request packet\r\n");
    
    ccm_crypt_pair_request_prepare(0, &pair_request_packet);
    
    tx_payload.noack = false;
    tx_payload.data[0] = APP_CMD_PAIR_REQUEST;
    memcpy(&tx_payload.data[1], &pair_request_packet, sizeof(ccm_pair_request_packet_t));
    tx_payload.length = 1 + sizeof(ccm_pair_request_packet_t);
    if (nrf_esb_write_payload(&tx_payload) != NRF_SUCCESS)
    {
        NRF_LOG_WARNING("Sending packet failed\r\n");
    }
}

static void send_encrypted_payload(uint8_t *payload, uint32_t payload_length)
{
    tx_payload.noack = false;
    tx_payload.data[0] = APP_CMD_ENCRYPTED_PAYLOAD;
    ccm_crypt_packet_encrypt(payload, payload_length, &tx_payload.data[1]);
    tx_payload.length = 1 + payload_length + 4;
    if (nrf_esb_write_payload(&tx_payload) != NRF_SUCCESS)
    {
        NRF_LOG_WARNING("Sending packet failed\r\n");
    }
}

static void update_timer(void *p_context)
{
    static uint8_t payload[8] = {0,0,0,0,0,0,0,0};
    payload[0] = bsp_board_button_state_get(0);
    payload[1] = bsp_board_button_state_get(1);
    payload[2] = bsp_board_button_state_get(2);
    
    if(!encryption_enabled || bsp_board_button_state_get(3))
    {
        encryption_enabled = true;
        send_pairing_request();
    }
    else
    {
        send_encrypted_payload(payload, 8);
    }
}


int main(void)
{
    ret_code_t err_code;

    gpio_init();

    err_code = NRF_LOG_INIT(NULL);
    APP_ERROR_CHECK(err_code);

    // Start the 32k clock.
    NRF_CLOCK->TASKS_LFCLKSTART = 1;
    
    // Initialize timer module.
    err_code = app_timer_init();
    APP_ERROR_CHECK(err_code);    

    // Create send packet timer.
    err_code = app_timer_create(&m_update_timer_id, APP_TIMER_MODE_REPEATED, update_timer);
    APP_ERROR_CHECK(err_code);
    
    // Start send packet timer.
    err_code = app_timer_start(m_update_timer_id, APP_TIMER_TICKS(500), 0);
    APP_ERROR_CHECK(err_code);
    
    clocks_start();

    err_code = esb_init();
    APP_ERROR_CHECK(err_code);

    ccm_crypt_init();

    bsp_board_leds_init();

    NRF_LOG_INFO("ESB CCM example started (PTX)\r\n");

    while (true)
    {
        if (NRF_LOG_PROCESS() == false)
        {
            
        }
    }
}
/*lint -restore */

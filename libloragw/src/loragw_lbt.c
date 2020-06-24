/*
 / _____)             _              | |
( (____  _____ ____ _| |_ _____  ____| |__
 \____ \| ___ |    (_   _) ___ |/ ___)  _ \
 _____) ) ____| | | || |_| ____( (___| | | |
(______/|_____)_|_|_| \__)_____)\____)_| |_|
  (C)2020 Semtech

Description:
    LoRa concentrator Listen-Before-Talk functions

License: Revised BSD License, see LICENSE.TXT file include in the project
*/


/* -------------------------------------------------------------------------- */
/* --- DEPENDANCIES --------------------------------------------------------- */

/* fix an issue between POSIX and C99 */
#if __STDC_VERSION__ >= 199901L
    #define _XOPEN_SOURCE 600
#else
    #define _XOPEN_SOURCE 500
#endif

#include <stdio.h>      /* printf */
#include <stdlib.h>     /* llabs */

#include "loragw_aux.h"
#include "loragw_lbt.h"
#include "loragw_sx1261.h"
#include "loragw_sx1302.h"

/* -------------------------------------------------------------------------- */
/* --- PRIVATE MACROS ------------------------------------------------------- */

#if DEBUG_HAL == 1
    #define DEBUG_MSG(str)                fprintf(stderr, str)
    #define DEBUG_PRINTF(fmt, args...)    fprintf(stderr,"%s:%d: "fmt, __FUNCTION__, __LINE__, args)
#else
    #define DEBUG_MSG(str)
    #define DEBUG_PRINTF(fmt, args...)
#endif

/* -------------------------------------------------------------------------- */
/* --- PRIVATE FUNCTIONS DEFINITION ----------------------------------------- */

/* As given frequencies have been converted from float to integer, some aliasing
issues can appear, so we can't simply check for equality, but have to take some
margin */
static bool is_equal_freq(uint32_t a, uint32_t b) {
    int64_t diff;
    int64_t a64 = (int64_t)a;
    int64_t b64 = (int64_t)b;

    /* Calculate the difference */
    diff = llabs(a64 - b64);

    /* Check for acceptable diff range */
    return ((diff <= 10000) ? true : false);
}

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

static int is_lbt_channel(const struct lgw_conf_lbt_s * lbt_context, uint32_t freq_hz, uint8_t bandwidth) {
    int i;
    int lbt_channel_match = -1;

    for (i = 0; i <  lbt_context->nb_channel; i++) {
        if ((is_equal_freq(freq_hz, lbt_context->channels[i].freq_hz) == true) && (bandwidth == lbt_context->channels[i].bandwidth)) {
            printf("LBT: select channel %d (freq:%u Hz, bw:0x%02X)\n", i, lbt_context->channels[i].freq_hz, lbt_context->channels[i].bandwidth);
            lbt_channel_match = i;
            break;
        }
    }

    /* Return the index of the LBT channel which matched */
    return lbt_channel_match;
}

/* -------------------------------------------------------------------------- */
/* --- PUBLIC FUNCTIONS DEFINITION ------------------------------------------ */

int lgw_lbt_start(const struct lgw_conf_lbt_s * lbt_context, uint32_t freq_hz, uint8_t bandwidth) {
    int err;
    int lbt_channel_selected;

    /* Check if we have a LBT channel for this transmit frequency */
    lbt_channel_selected = is_lbt_channel(lbt_context, freq_hz, bandwidth);
    if (lbt_channel_selected == -1) {
        printf("ERROR: Cannot start LBT - wrong channel\n");
        return -1;
    }

    /* Set LBT scan frequency */
    err = sx1261_set_rx_params(freq_hz, bandwidth);
    if (err != 0) {
        printf("ERROR: Cannot start LBT - unable to set sx1261 RX parameters\n");
        return -1;
    }

    /* Start LBT */
    err = sx1261_lbt_start(lbt_context->channels[lbt_channel_selected].scan_time_us, lbt_context->rssi_target);
    if (err != 0) {
        printf("ERROR: Cannot start LBT - sx1261 LBT start\n");
        return -1;
    }

    return 0;
}

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

int lgw_lbt_tx_status(uint8_t rf_chain, bool * tx_ok) {
    int err;
    uint8_t status;

    /* Wait for transmit to be initiated */
    /* Bit 0 in status: TX has been initiated on Radio A */
    /* Bit 1 in status: TX has been initiated on Radio B */
    do {
        err = sx1302_agc_status(&status);
        if (err != 0) {
            printf("ERROR: %s: failed to get AGC status\n", __FUNCTION__);
            return -1;
        }
        wait_ms(1);
    } while ((status & (1 << rf_chain)) == 0x00);
    printf("==> AGC_STATUS 0x%02X\n", status);

    /* Check if the packet has been transmitted or blocked by LBT */
    /* Bit 6 in status: Radio A is not allowed to transmit */
    /* Bit 7 in status: Radio B is not allowed to transmit */
    if (TAKE_N_BITS_FROM(status, ((rf_chain == 0) ? 6 : 7), 1) == 0) {
        *tx_ok = true;
    } else {
        *tx_ok = false;
    }

    /* Clear AGC transmit status */
    sx1302_agc_mailbox_write(0, 0xFF);

    /* Wait for transmit status to be cleared */
    do {
        err = sx1302_agc_status(&status);
        if (err != 0) {
            printf("ERROR: %s: failed to get AGC status\n", __FUNCTION__);
            return -1;
        }
        wait_ms(1);
        printf("==> AGC_STATUS 0x%02X\n", status);
    } while (status != 0x00);

    /* Acknoledge */
    sx1302_agc_mailbox_write(0, 0x00);

    return 0;
}

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

int lgw_lbt_stop(void) {
    int err;

    err = sx1261_lbt_stop();
    if (err != 0) {
        printf("ERROR: Cannot stop LBT - failed\n");
        return -1;
    }

    return 0;
}

/* --- EOF ------------------------------------------------------------------ */
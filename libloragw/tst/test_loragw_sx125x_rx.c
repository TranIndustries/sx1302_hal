/*
 / _____)             _              | |
( (____  _____ ____ _| |_ _____  ____| |__
 \____ \| ___ |    (_   _) ___ |/ ___)  _ \
 _____) ) ____| | | || |_| ____( (___| | | |
(______/|_____)_|_|_| \__)_____)\____)_| |_|
  (C)2018 Semtech

Description:
    TODO

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

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <math.h>
#include <assert.h>

#include "loragw_spi.h"
#include "loragw_reg.h"
#include "loragw_sx125x.h"
#include "loragw_aux.h"
#include "loragw_hal.h"

/* -------------------------------------------------------------------------- */
/* --- PRIVATE MACROS ------------------------------------------------------- */

#define SX1302_FREQ_TO_REG(f)       (uint32_t)((uint64_t)f * (1 << 18) / 32000000U)
#define SX1257_FREQ_TO_REG(f)       (uint32_t)((uint64_t)f * (1 << 19) / 32000000U)
#define SX1255_FREQ_TO_REG(f)       (uint32_t)((uint64_t)f * (1 << 20) / 32000000U)
#define IF_HZ_TO_REG(f)             ((f << 5) / 15625)

/* -------------------------------------------------------------------------- */
/* --- PRIVATE CONSTANTS ---------------------------------------------------- */

#define BUFF_SIZE           1024
#define DEFAULT_FREQ_HZ     868500000U

const int32_t channel_if[8] = {
    700000,
    500000,
    300000,
    100000,
    -100000,
    -300000,
    -500000,
    -700000
};

/* -------------------------------------------------------------------------- */
/* --- PRIVATE VARIABLES ---------------------------------------------------- */

#include "src/arbiter_ludo.var"

static uint32_t nb_pkt_received = 0;

/* -------------------------------------------------------------------------- */
/* --- PRIVATE FUNCTIONS ---------------------------------------------------- */

int sx125x_init(uint32_t freq_hz) {
    uint8_t version;
    uint32_t freq_reg;

    /* Enable radio and perform chip reset */
    lgw_reg_w(SX1302_REG_AGC_MCU_RF_EN_B_RADIO_EN, 0x01);
    lgw_reg_w(SX1302_REG_AGC_MCU_RF_EN_B_RADIO_RST, 0x01);
    wait_ms(500);
    lgw_reg_w(SX1302_REG_AGC_MCU_RF_EN_B_RADIO_RST, 0x00);
    wait_ms(10);

    /* Set radio mode */
    lgw_reg_w(SX1302_REG_COMMON_CTRL0_SX1261_MODE_RADIO_B, 0x00);

    /* Configure radio */
#if 0
    lgw_setup_sx125x(LGW_SPI_MUX_TARGET_RADIOB, LGW_SPI_MUX_TARGET_RADIOB, true, LGW_RADIO_TYPE_SX1257, freq_hz);
#else
    {
        uint8_t val;

        uint8_t TxDacClkSel = 0;         // 0:int, 1:ext (default 0)
        uint8_t ClkOut = 1;              // 0:disabled, 1:enabled (default 1)
        uint8_t RfLoopBack = 0;          // 0:disabled, 1:enabled (default 0)
        uint8_t DigitalLoopBack = 0;     // 0:disabled, 1:enabled (default 0)

        val = sx125x_read(LGW_SPI_MUX_TARGET_RADIOB, 0x10);
        printf("sx1257:0x10:0x%02X\n", val);
        sx125x_write(LGW_SPI_MUX_TARGET_RADIOB, 0x10, TxDacClkSel + ClkOut * 2 + RfLoopBack * 4 + DigitalLoopBack * 8); /* RegClkSelect */

        /* Tx */
        uint8_t TxDacGain = 2; // 3:0, 2:-3, 1:-6, 0:-9 dBFS (default 2, max 3)
        uint8_t TxMixGain = 14; // gain = -38 + 2*TxMixGain dB (default 14, max 15)
        uint8_t TxPllBw = 3; // 0:75, 1:150, 2:225, 3:300 kHz (default 3, max 3)
        uint8_t TxAnaBw = 0; // 17.5 / 2*(41-TxAnaBw) MHz (default 0, max 31)
        uint8_t TxDacBw = 5; // 24 + 8*TxDacBw Nb FIR taps (default 2, max 5)

        sx125x_write(LGW_SPI_MUX_TARGET_RADIOB, 0x08, TxMixGain + TxDacGain * 16); /* RegTxGain */
        sx125x_write(LGW_SPI_MUX_TARGET_RADIOB, 0x0A, TxAnaBw + TxPllBw * 32); /* RegTxBw */
        sx125x_write(LGW_SPI_MUX_TARGET_RADIOB, 0x0B, TxDacBw); /* RegTxDacBw */

        /* Rx */
        uint8_t RxLnaGain = 1;           // 1:G, 2:G-6, 3:G-12, 4:G-24, 5:G-36, 6:G-48 (default 1, min 1, max 6)
        uint8_t RxBasebandGain = 15;     // gain = G+2*bb_gain (default 15, max 15)
        uint8_t LnaZin = 0;              // 0:50, 1:200 Ohm (default 1)
        uint8_t RxAdcBw = 7;             // 2:100<BW<200, 5:200<BW<400,7:400<BW kHz SSB (default 7, max 7)
        uint8_t RxAdcTrim = 6;           // 6 for 32MHz ref, 5 for 36MHz ref (default 7, max 7)
        uint8_t RxBasebandBw = 0;        // 2 // 0:750, 1:500, 2:375 3:250 kHz SSB (default 1, max 3)
        uint8_t RxPllBw = 0;             // 0:75 1:150 2:225 3:300 kHz (default 3, max 3)
        uint8_t RxAdcTemp = 0;           // ADC temperature measurement mode (default 0)

        sx125x_write(LGW_SPI_MUX_TARGET_RADIOB, 0x0C, LnaZin + RxBasebandGain * 2 + RxLnaGain * 32); /* RegRxAnaGain */
        sx125x_write(LGW_SPI_MUX_TARGET_RADIOB, 0x0D, RxBasebandBw + RxAdcTrim * 4 + RxAdcBw * 32); /* RegRxBw */
        sx125x_write(LGW_SPI_MUX_TARGET_RADIOB, 0x0E, RxAdcTemp + RxPllBw * 2); /* RegRxPLLBw */

        /* Check radio version */
        version = sx125x_read(LGW_SPI_MUX_TARGET_RADIOB, 0x07);
        switch (version) {
            case 0x11:
                printf("sx1255 detected\n");
                freq_reg = SX1255_FREQ_TO_REG(freq_hz);
                break;
            case 0x21:
                printf("sx1257 detected\n");
                freq_reg = SX1257_FREQ_TO_REG(freq_hz);
                break;
            default:
                printf("ERROR: failed to detect radio version (0x%02X)\n", version);
                return -1;
        }

        /* Set RX frequency */
        sx125x_write(LGW_SPI_MUX_TARGET_RADIOB, 0x01, (freq_reg & 0xFF0000) >> 16); /* RegFrfRxMsb */
        sx125x_write(LGW_SPI_MUX_TARGET_RADIOB, 0x02, (freq_reg & 0x00FF00) >>  8); /* RegFrfRxMid */
        sx125x_write(LGW_SPI_MUX_TARGET_RADIOB, 0x03, (freq_reg & 0x0000FF) >>  0); /* RegFrfRxLsb */

        /* Set TX frequency */
        sx125x_write(LGW_SPI_MUX_TARGET_RADIOB, 0x04, (freq_reg & 0xFF0000) >> 16); /* RegFrfTxMsb */
        sx125x_write(LGW_SPI_MUX_TARGET_RADIOB, 0x05, (freq_reg & 0x00FF00) >>  8); /* RegFrfTxMid */
        sx125x_write(LGW_SPI_MUX_TARGET_RADIOB, 0x06, (freq_reg & 0x0000FF) >>  0); /* RegFrfTxLsb */

        /* Get Status */
        val = sx125x_read(LGW_SPI_MUX_TARGET_RADIOB, 0x1A); /* RegLowBatThres */
        printf("LOW BAT THRESHOLD: 0x%02X\n", val);
        val = sx125x_read(LGW_SPI_MUX_TARGET_RADIOB, 0x11); /* RegModeStatus */
        printf("MODE_STATUS: 0x%02X\n", val);

        /* Enable clocks */
        wait_ms(100);
        sx125x_write(LGW_SPI_MUX_TARGET_RADIOB, 0x00, 1); /* RegMode:StandbyEnable */
        wait_ms(100);
        sx125x_write(LGW_SPI_MUX_TARGET_RADIOB, 0x00, 1 + 2 + 0); /* RegMode: Rx enabled, Tx disabled */
        wait_ms(100);

        /* Get Status */
        val = sx125x_read(LGW_SPI_MUX_TARGET_RADIOB, 0x11); /* RegModeStatus */
        printf("MODE_STATUS: 0x%02X\n", val);

        /* Enable PA driver */
        /* TODO */

        //sx125x_write(LGW_SPI_MUX_TARGET_RADIOB, 0x00, 0); /* RegMode:StandbyEnable */
    }
#endif

    /* Switch SX1302 clock from SPI clock to SX1262 clock */
    lgw_reg_w(SX1302_REG_CLK_CTRL_CLK_SEL_CLK_RADIO_A_SEL, 0x00);
    lgw_reg_w(SX1302_REG_CLK_CTRL_CLK_SEL_CLK_RADIO_B_SEL, 0x01);

    /* Enable clock dividers */
    lgw_reg_w(SX1302_REG_CLK_CTRL_CLK_SEL_CLKDIV_EN, 0x01); /* Mandatory */
    lgw_reg_w(SX1302_REG_COMMON_CTRL0_CLK32_RIF_CTRL, 0x00); /* ?? */

    return 0;
}

int sx125x_set_idle(void) {
    return 0;
}

int load_firmware_arb(const uint8_t *firmware) {
    int i;
    uint8_t fw_check[8192];
    int32_t gpio_sel = 0x02; /* ARB MCU */
    int32_t val;

    /* Configure GPIO to let AGC MCU access board LEDs */
    lgw_reg_w(SX1302_REG_GPIO_GPIO_SEL_0_SELECTION, gpio_sel);
    lgw_reg_w(SX1302_REG_GPIO_GPIO_SEL_1_SELECTION, gpio_sel);
    lgw_reg_w(SX1302_REG_GPIO_GPIO_SEL_2_SELECTION, gpio_sel);
    lgw_reg_w(SX1302_REG_GPIO_GPIO_SEL_3_SELECTION, gpio_sel);
    lgw_reg_w(SX1302_REG_GPIO_GPIO_SEL_4_SELECTION, gpio_sel);
    lgw_reg_w(SX1302_REG_GPIO_GPIO_SEL_5_SELECTION, gpio_sel);
    lgw_reg_w(SX1302_REG_GPIO_GPIO_SEL_6_SELECTION, gpio_sel);
    lgw_reg_w(SX1302_REG_GPIO_GPIO_SEL_7_SELECTION, gpio_sel);
    lgw_reg_w(SX1302_REG_GPIO_GPIO_DIR_DIRECTION, 0xFF); /* GPIO output direction */

    /* Take control over ARB MCU */
    lgw_reg_w(SX1302_REG_ARB_MCU_CTRL_MCU_CLEAR, 0x01);
    lgw_reg_w(SX1302_REG_ARB_MCU_CTRL_HOST_PROG, 0x01);
    lgw_reg_w(SX1302_REG_COMMON_PAGE_PAGE, 0x00);

    /* Write AGC fw in AGC MEM */
    for (i = 0; i < 8; i++) {
        lgw_mem_wb(0x2000+(i*1024), &firmware[i*1024], 1024);
    }

    /* Read back and check */
    for (i = 0; i < 8; i++) {
        lgw_mem_rb(0x2000+(i*1024), &fw_check[i*1024], 1024);
    }
    if (memcmp(firmware, fw_check, 8192) != 0) {
        printf ("ERROR: Failed to load fw\n");
        return -1;
    }

    /* Release control over AGC MCU */
    lgw_reg_w(SX1302_REG_ARB_MCU_CTRL_HOST_PROG, 0x00);
    lgw_reg_w(SX1302_REG_ARB_MCU_CTRL_MCU_CLEAR, 0x00);

    lgw_reg_r(SX1302_REG_ARB_MCU_CTRL_PARITY_ERROR, &val);
    printf("ARB fw loaded (parity error:0x%02X)\n", val);

    printf("Waiting for ARB fw to start...\n");
    wait_ms(3000);

    return 0;
}

int sx125x_configure_channels(void) {
    int32_t cnt, cnt2;
    int32_t if_freq;

    // printf("if0: %d (0x%04X)\n", IF_HZ_TO_REG(if0), IF_HZ_TO_REG(if0));
    // printf("if1: %d (0x%04X)\n", IF_HZ_TO_REG(if1), IF_HZ_TO_REG(if1));
    // printf("if2: %d (0x%04X)\n", IF_HZ_TO_REG(if2), IF_HZ_TO_REG(if2));
    // printf("if3: %d (0x%04X)\n", IF_HZ_TO_REG(if3), IF_HZ_TO_REG(if3));
    // printf("if4: %d (0x%04X)\n", IF_HZ_TO_REG(if4), IF_HZ_TO_REG(if4));
    // printf("if5: %d (0x%04X)\n", IF_HZ_TO_REG(if5), IF_HZ_TO_REG(if5));
    // printf("if6: %d (0x%04X)\n", IF_HZ_TO_REG(if6), IF_HZ_TO_REG(if6));
    // printf("if7: %d (0x%04X)\n", IF_HZ_TO_REG(if7), IF_HZ_TO_REG(if7));

    /* Configure channelizer */
    lgw_reg_w(SX1302_REG_RX_TOP_RADIO_SELECT_RADIO_SELECT, 0xFF); /* RadioB */

    if_freq = IF_HZ_TO_REG(channel_if[0]);
    lgw_reg_w(SX1302_REG_RX_TOP_FREQ_0_MSB_IF_FREQ_0, (if_freq >> 8) & 0x0000001F);
    lgw_reg_w(SX1302_REG_RX_TOP_FREQ_0_LSB_IF_FREQ_0, (if_freq >> 0) & 0x000000FF);

    if_freq = IF_HZ_TO_REG(channel_if[1]);
    lgw_reg_w(SX1302_REG_RX_TOP_FREQ_1_MSB_IF_FREQ_1, (if_freq >> 8) & 0x0000001F);
    lgw_reg_w(SX1302_REG_RX_TOP_FREQ_1_LSB_IF_FREQ_1, (if_freq >> 0) & 0x000000FF);

    if_freq = IF_HZ_TO_REG(channel_if[2]);
    lgw_reg_w(SX1302_REG_RX_TOP_FREQ_2_MSB_IF_FREQ_2, (if_freq >> 8) & 0x0000001F);
    lgw_reg_w(SX1302_REG_RX_TOP_FREQ_2_LSB_IF_FREQ_2, (if_freq >> 0) & 0x000000FF);

    if_freq = IF_HZ_TO_REG(channel_if[3]);
    lgw_reg_w(SX1302_REG_RX_TOP_FREQ_3_MSB_IF_FREQ_3, (if_freq >> 8) & 0x0000001F);
    lgw_reg_w(SX1302_REG_RX_TOP_FREQ_3_LSB_IF_FREQ_3, (if_freq >> 0) & 0x000000FF);

    if_freq = IF_HZ_TO_REG(channel_if[4]);
    lgw_reg_w(SX1302_REG_RX_TOP_FREQ_4_MSB_IF_FREQ_4, (if_freq >> 8) & 0x0000001F);
    lgw_reg_w(SX1302_REG_RX_TOP_FREQ_4_LSB_IF_FREQ_4, (if_freq >> 0) & 0x000000FF);

    if_freq = IF_HZ_TO_REG(channel_if[5]);
    lgw_reg_w(SX1302_REG_RX_TOP_FREQ_5_MSB_IF_FREQ_5, (if_freq >> 8) & 0x0000001F);
    lgw_reg_w(SX1302_REG_RX_TOP_FREQ_5_LSB_IF_FREQ_5, (if_freq >> 0) & 0x000000FF);

    if_freq = IF_HZ_TO_REG(channel_if[6]);
    lgw_reg_w(SX1302_REG_RX_TOP_FREQ_6_MSB_IF_FREQ_6, (if_freq >> 8) & 0x0000001F);
    lgw_reg_w(SX1302_REG_RX_TOP_FREQ_6_LSB_IF_FREQ_6, (if_freq >> 0) & 0x000000FF);

    if_freq = IF_HZ_TO_REG(channel_if[7]);
    lgw_reg_w(SX1302_REG_RX_TOP_FREQ_7_MSB_IF_FREQ_7, (if_freq >> 8) & 0x0000001F);
    lgw_reg_w(SX1302_REG_RX_TOP_FREQ_7_LSB_IF_FREQ_7, (if_freq >> 0) & 0x000000FF);

    /* Configure correlators */
    lgw_reg_w(SX1302_REG_RX_TOP_SF7_CFG1_ACC_2_SAME_PEAKS, 1);
    lgw_reg_w(SX1302_REG_RX_TOP_SF7_CFG1_ACC_AUTO_RESCALE, 1);
    lgw_reg_w(SX1302_REG_RX_TOP_SF7_CFG1_ACC_COEFF, 2);
    lgw_reg_w(SX1302_REG_RX_TOP_SF7_CFG1_ACC_PEAK_POS_SEL, 1);
    lgw_reg_w(SX1302_REG_RX_TOP_SF7_CFG1_ACC_PEAK_SUM_EN, 1);
    lgw_reg_w(SX1302_REG_RX_TOP_SF7_CFG3_MIN_SINGLE_PEAK, 11);
    lgw_reg_w(SX1302_REG_RX_TOP_SF7_CFG6_MSP_CNT_MODE, 0);
    lgw_reg_w(SX1302_REG_RX_TOP_SF7_CFG6_MSP_POS_SEL, 1);
    lgw_reg_w(SX1302_REG_RX_TOP_SF7_CFG7_NOISE_COEFF, 2);
    lgw_reg_w(SX1302_REG_RX_TOP_SF7_CFG2_ACC_PNR, 55);
    lgw_reg_w(SX1302_REG_RX_TOP_SF7_CFG4_MSP_PNR, 55);
    lgw_reg_w(SX1302_REG_RX_TOP_SF7_CFG5_MSP2_PNR, 55);
    lgw_reg_w(SX1302_REG_RX_TOP_SF7_CFG6_MSP_PEAK_NB, 3);
    lgw_reg_w(SX1302_REG_RX_TOP_SF7_CFG7_MSP2_PEAK_NB, 3);

    lgw_reg_w(SX1302_REG_RX_TOP_SF8_CFG1_ACC_2_SAME_PEAKS, 0);
    lgw_reg_w(SX1302_REG_RX_TOP_SF8_CFG1_ACC_AUTO_RESCALE, 1);
    lgw_reg_w(SX1302_REG_RX_TOP_SF8_CFG1_ACC_COEFF, 2);
    lgw_reg_w(SX1302_REG_RX_TOP_SF8_CFG1_ACC_PEAK_POS_SEL, 1);
    lgw_reg_w(SX1302_REG_RX_TOP_SF8_CFG1_ACC_PEAK_SUM_EN, 1);
    lgw_reg_w(SX1302_REG_RX_TOP_SF8_CFG3_MIN_SINGLE_PEAK, 11);
    lgw_reg_w(SX1302_REG_RX_TOP_SF8_CFG6_MSP_CNT_MODE, 0);
    lgw_reg_w(SX1302_REG_RX_TOP_SF8_CFG6_MSP_POS_SEL, 1);
    lgw_reg_w(SX1302_REG_RX_TOP_SF8_CFG7_NOISE_COEFF, 2);
    lgw_reg_w(SX1302_REG_RX_TOP_SF8_CFG2_ACC_PNR, 56);
    lgw_reg_w(SX1302_REG_RX_TOP_SF8_CFG4_MSP_PNR, 56);
    lgw_reg_w(SX1302_REG_RX_TOP_SF8_CFG5_MSP2_PNR, 56);
    lgw_reg_w(SX1302_REG_RX_TOP_SF8_CFG6_MSP_PEAK_NB, 3);
    lgw_reg_w(SX1302_REG_RX_TOP_SF8_CFG7_MSP2_PEAK_NB, 3);

    /* TODO: not working */
    lgw_reg_w(SX1302_REG_RX_TOP_SF9_CFG1_ACC_2_SAME_PEAKS, 0);
    lgw_reg_w(SX1302_REG_RX_TOP_SF9_CFG1_ACC_AUTO_RESCALE, 1);
    lgw_reg_w(SX1302_REG_RX_TOP_SF9_CFG1_ACC_COEFF, 2);
    lgw_reg_w(SX1302_REG_RX_TOP_SF9_CFG1_ACC_PEAK_POS_SEL, 1);
    lgw_reg_w(SX1302_REG_RX_TOP_SF9_CFG1_ACC_PEAK_SUM_EN, 1);
    lgw_reg_w(SX1302_REG_RX_TOP_SF9_CFG3_MIN_SINGLE_PEAK, 11);
    lgw_reg_w(SX1302_REG_RX_TOP_SF9_CFG6_MSP_CNT_MODE, 0);
    lgw_reg_w(SX1302_REG_RX_TOP_SF9_CFG6_MSP_POS_SEL, 1);
    lgw_reg_w(SX1302_REG_RX_TOP_SF9_CFG7_NOISE_COEFF, 2);
    lgw_reg_w(SX1302_REG_RX_TOP_SF9_CFG2_ACC_PNR, 58);
    lgw_reg_w(SX1302_REG_RX_TOP_SF9_CFG4_MSP_PNR, 58);
    lgw_reg_w(SX1302_REG_RX_TOP_SF9_CFG5_MSP2_PNR, 58);
    lgw_reg_w(SX1302_REG_RX_TOP_SF9_CFG6_MSP_PEAK_NB, 3);
    lgw_reg_w(SX1302_REG_RX_TOP_SF9_CFG7_MSP2_PEAK_NB, 3);

    /* TODO: not working */
    lgw_reg_w(SX1302_REG_RX_TOP_SF10_CFG1_ACC_2_SAME_PEAKS, 0);
    lgw_reg_w(SX1302_REG_RX_TOP_SF10_CFG1_ACC_AUTO_RESCALE, 1);
    lgw_reg_w(SX1302_REG_RX_TOP_SF10_CFG1_ACC_COEFF, 2);
    lgw_reg_w(SX1302_REG_RX_TOP_SF10_CFG1_ACC_PEAK_POS_SEL, 1);
    lgw_reg_w(SX1302_REG_RX_TOP_SF10_CFG1_ACC_PEAK_SUM_EN, 1);
    lgw_reg_w(SX1302_REG_RX_TOP_SF10_CFG3_MIN_SINGLE_PEAK, 11);
    lgw_reg_w(SX1302_REG_RX_TOP_SF10_CFG6_MSP_CNT_MODE, 0);
    lgw_reg_w(SX1302_REG_RX_TOP_SF10_CFG6_MSP_POS_SEL, 1);
    lgw_reg_w(SX1302_REG_RX_TOP_SF10_CFG7_NOISE_COEFF, 2);
    lgw_reg_w(SX1302_REG_RX_TOP_SF10_CFG2_ACC_PNR, 60);
    lgw_reg_w(SX1302_REG_RX_TOP_SF10_CFG4_MSP_PNR, 60);
    lgw_reg_w(SX1302_REG_RX_TOP_SF10_CFG5_MSP2_PNR, 60);
    lgw_reg_w(SX1302_REG_RX_TOP_SF10_CFG6_MSP_PEAK_NB, 3);
    lgw_reg_w(SX1302_REG_RX_TOP_SF10_CFG7_MSP2_PEAK_NB, 3);

    /* TODO: not working */
    lgw_reg_w(SX1302_REG_RX_TOP_SF11_CFG1_ACC_2_SAME_PEAKS, 0);
    lgw_reg_w(SX1302_REG_RX_TOP_SF11_CFG1_ACC_AUTO_RESCALE, 1);
    lgw_reg_w(SX1302_REG_RX_TOP_SF11_CFG1_ACC_COEFF, 2);
    lgw_reg_w(SX1302_REG_RX_TOP_SF11_CFG1_ACC_PEAK_POS_SEL, 1);
    lgw_reg_w(SX1302_REG_RX_TOP_SF11_CFG1_ACC_PEAK_SUM_EN, 1);
    lgw_reg_w(SX1302_REG_RX_TOP_SF11_CFG3_MIN_SINGLE_PEAK, 11);
    lgw_reg_w(SX1302_REG_RX_TOP_SF11_CFG6_MSP_CNT_MODE, 0);
    lgw_reg_w(SX1302_REG_RX_TOP_SF11_CFG6_MSP_POS_SEL, 1);
    lgw_reg_w(SX1302_REG_RX_TOP_SF11_CFG7_NOISE_COEFF, 2);
    lgw_reg_w(SX1302_REG_RX_TOP_SF11_CFG2_ACC_PNR, 60);
    lgw_reg_w(SX1302_REG_RX_TOP_SF11_CFG4_MSP_PNR, 60);
    lgw_reg_w(SX1302_REG_RX_TOP_SF11_CFG5_MSP2_PNR, 60);
    lgw_reg_w(SX1302_REG_RX_TOP_SF11_CFG6_MSP_PEAK_NB, 3);
    lgw_reg_w(SX1302_REG_RX_TOP_SF11_CFG7_MSP2_PEAK_NB, 3);

    /* TODO: not working */
    lgw_reg_w(SX1302_REG_RX_TOP_SF12_CFG1_ACC_2_SAME_PEAKS, 0);
    lgw_reg_w(SX1302_REG_RX_TOP_SF12_CFG1_ACC_AUTO_RESCALE, 1);
    lgw_reg_w(SX1302_REG_RX_TOP_SF12_CFG1_ACC_COEFF, 2);
    lgw_reg_w(SX1302_REG_RX_TOP_SF12_CFG1_ACC_PEAK_POS_SEL, 1);
    lgw_reg_w(SX1302_REG_RX_TOP_SF12_CFG1_ACC_PEAK_SUM_EN, 1);
    lgw_reg_w(SX1302_REG_RX_TOP_SF12_CFG3_MIN_SINGLE_PEAK, 11);
    lgw_reg_w(SX1302_REG_RX_TOP_SF12_CFG6_MSP_CNT_MODE, 0);
    lgw_reg_w(SX1302_REG_RX_TOP_SF12_CFG6_MSP_POS_SEL, 1);
    lgw_reg_w(SX1302_REG_RX_TOP_SF12_CFG7_NOISE_COEFF, 2);
    lgw_reg_w(SX1302_REG_RX_TOP_SF12_CFG2_ACC_PNR, 60);
    lgw_reg_w(SX1302_REG_RX_TOP_SF12_CFG4_MSP_PNR, 60);
    lgw_reg_w(SX1302_REG_RX_TOP_SF12_CFG5_MSP2_PNR, 60);
    lgw_reg_w(SX1302_REG_RX_TOP_SF12_CFG6_MSP_PEAK_NB, 3);
    lgw_reg_w(SX1302_REG_RX_TOP_SF12_CFG7_MSP2_PEAK_NB, 3);

    lgw_reg_w(SX1302_REG_RX_TOP_CORRELATOR_SF_EN_CORR_SF_EN, 0xF3); /* 9 10 11 12 5 6 7 8 */
    lgw_reg_w(SX1302_REG_RX_TOP_CORRELATOR_EN_CORR_EN, 0x01);

    /* Configure multi-sf */
    lgw_reg_w(SX1302_REG_RX_TOP_DC_NOTCH_CFG1_ENABLE, 0x00);
    lgw_reg_w(SX1302_REG_RX_TOP_RX_DFE_AGC1_FREEZE_ON_SYNC, 0x00);
    lgw_reg_w(SX1302_REG_RX_TOP_RX_DFE_AGC1_FORCE_DEFAULT_FIR, 0x01);
    lgw_reg_w(SX1302_REG_RX_TOP_RX_CFG0_SWAP_IQ, 0x00);
    lgw_reg_w(SX1302_REG_RX_TOP_RX_CFG0_CHIRP_INVERT, 0x01);
    lgw_reg_w(SX1302_REG_RX_TOP_MODEM_SYNC_DELTA_LSB_MODEM_SYNC_DELTA, 7);
    lgw_reg_w(SX1302_REG_RX_TOP_MODEM_SYNC_DELTA_MSB_MODEM_SYNC_DELTA, 0);

    /* Configure Syncwork Public/Private */
    lgw_reg_w(SX1302_REG_RX_TOP_FRAME_SYNCH0_PEAK1_POS, 6);
    lgw_reg_w(SX1302_REG_RX_TOP_FRAME_SYNCH1_PEAK2_POS, 8);

    /* Configure agc */
    lgw_reg_w(SX1302_REG_RADIO_FE_RSSI_BB_FILTER_ALPHA_RADIO_A_RSSI_BB_FILTER_ALPHA, 0x06);
    lgw_reg_w(SX1302_REG_RADIO_FE_RSSI_DEC_FILTER_ALPHA_RADIO_A_RSSI_DEC_FILTER_ALPHA, 0x07);
    lgw_reg_w(SX1302_REG_RADIO_FE_RSSI_DB_DEF_RADIO_A_RSSI_DB_DEFAULT_VALUE, 23);
    lgw_reg_w(SX1302_REG_RADIO_FE_RSSI_DEC_DEF_RADIO_A_RSSI_DEC_DEFAULT_VALUE, 66);
    lgw_reg_w(SX1302_REG_RX_TOP_RSSI_CONTROL_RSSI_FILTER_ALPHA, 0x00);
    lgw_reg_w(SX1302_REG_RX_TOP_RSSI_DEF_VALUE_CHAN_RSSI_DEF_VALUE, 85);
    lgw_reg_w(SX1302_REG_RADIO_FE_CTRL0_RADIO_A_DC_NOTCH_EN, 0x00);
    lgw_reg_w(SX1302_REG_RADIO_FE_CTRL0_RADIO_A_HOST_FILTER_GAIN, 0x08);
    lgw_reg_w(SX1302_REG_COMMON_CTRL0_HOST_RADIO_CTRL, 0x01);
    lgw_reg_w(SX1302_REG_RADIO_FE_CTRL0_RADIO_A_FORCE_HOST_FILTER_GAIN, 0x01);
    lgw_reg_w(SX1302_REG_RX_TOP_GAIN_CONTROL_CHAN_GAIN_VALID, 0x01);
    lgw_reg_w(SX1302_REG_RX_TOP_GAIN_CONTROL_CHAN_GAIN, 0x0F);

    /* Check that the SX1302 timestamp counter is running */
    lgw_reg_r(SX1302_REG_TIMESTAMP_TIMESTAMP_LSB1_TIMESTAMP, &cnt);
    lgw_reg_r(SX1302_REG_TIMESTAMP_TIMESTAMP_LSB1_TIMESTAMP, &cnt2);
    if (cnt == cnt2) {
        printf("ERROR: SX1302 timestamp counter is not running (val:%u)\n", cnt);
        return -1;
    }

    /* load ARB fw */
    load_firmware_arb(arb_firmware);

    /* Enable modem */
    lgw_reg_w(SX1302_REG_COMMON_GEN_CONCENTRATOR_MODEM_ENABLE, 0x01);
    lgw_reg_w(SX1302_REG_COMMON_GEN_FSK_MODEM_ENABLE, 0x01);
    lgw_reg_w(SX1302_REG_COMMON_GEN_GLOBAL_EN, 0x01);

    lgw_reg_w(SX1302_REG_COMMON_CTRL0_CLK32_RIF_CTRL, 0x01); /* ?? */

    return 0;
}

int sx125x_receive(void) {
    uint16_t nb_bytes;
    uint8_t buff[2];
    uint8_t fifo[1024];
    int i;
    int idx;
    uint8_t payload_size;
    uint32_t count_us;

    lgw_reg_rb(SX1302_REG_RX_TOP_RX_BUFFER_NB_BYTES_MSB_RX_BUFFER_NB_BYTES, buff, sizeof buff);
    nb_bytes  = (uint16_t)((buff[0] << 8) & 0xFF00);
    nb_bytes |= (uint16_t)((buff[1] << 0) & 0x00FF);

    if (nb_bytes > 1024) {
        printf("ERROR: more than 1024 bytes in the FIFO, to be reworked\n");
        assert(0);
    }

    if (nb_bytes > 0) {
        printf("nb_bytes received: %u (%u %u)\n", nb_bytes, buff[1], buff[0]);

        /* read bytes from fifo */
        memset(fifo, 0, sizeof fifo);
        lgw_mem_rb(0x4000, fifo, nb_bytes);
        for (i = 0; i < nb_bytes; i++) {
            printf("%02X ", fifo[i]);
        }
        printf("\n");

        /* parse packet */
        idx = 0;
        while (idx < nb_bytes) {
            if ((fifo[idx] == 0xA5) && (fifo[idx+1] == 0xC0)) {
                nb_pkt_received += 1;
                /* we found the start of a packet, parse it */
                printf("\n----- new packet (%u) -----\n", nb_pkt_received);
                payload_size = fifo[idx+2];
                printf("  size:     %u\n", payload_size);
                printf("  chan:     %u\n", fifo[idx+3]);
                printf("  crc_en:   %u\n", TAKE_N_BITS_FROM(fifo[idx+4], 0, 1));
                printf("  codr:     %u\n", TAKE_N_BITS_FROM(fifo[idx+4], 1, 3));
                printf("  datr:     %u\n", TAKE_N_BITS_FROM(fifo[idx+4], 4, 4));
                printf("  modem:    %u\n", fifo[idx+5]);
                printf("  payload: ");
                for (i = 0; i < payload_size; i++) {
                    printf("%02X ", fifo[idx+6+i]);
                }
                printf("\n");
                printf("  status:   %u\n", TAKE_N_BITS_FROM(fifo[idx+6+payload_size], 0, 1));
                printf("  snr_avg:  %d\n", fifo[idx+7+payload_size]);
                printf("  rssi_chan:%d\n", fifo[idx+8+payload_size]);
                printf("  rssi_sig: %d\n", fifo[idx+9+payload_size]);
                count_us  = (uint32_t)((fifo[idx+12+payload_size] <<  0) & 0x000000FF);
                count_us |= (uint32_t)((fifo[idx+13+payload_size] <<  8) & 0x0000FF00);
                count_us |= (uint32_t)((fifo[idx+14+payload_size] << 16) & 0x00FF0000);
                count_us |= (uint32_t)((fifo[idx+15+payload_size] << 24) & 0xFF000000);
                printf("  timestamp:%u (count_us:%u)\n", count_us, count_us/32);
            }
            idx += 1;
        }

    }

    return 0;
}

/* -------------------------------------------------------------------------- */
/* --- MAIN FUNCTION -------------------------------------------------------- */

/* describe command line options */
void usage(void) {
    //printf("Library version information: %s\n", lgw_version_info());
    printf( "Available options:\n");
    printf( " -h print this help\n");
    printf( " -f <float> Radio RX frequency in MHz\n");
}

int main(int argc, char **argv)
{
    int i, x;
    uint32_t ft = DEFAULT_FREQ_HZ;
    double arg_d = 0.0;

    /* parse command line options */
    while ((i = getopt (argc, argv, "hf:")) != -1) {
        switch (i) {
            case 'h':
                usage();
                return -1;
                break;
            case 'f': /* <float> Radio TX frequency in MHz */
                i = sscanf(optarg, "%lf", &arg_d);
                if (i != 1) {
                    printf("ERROR: argument parsing of -f argument. Use -h to print help\n");
                    return EXIT_FAILURE;
                } else {
                    ft = (uint32_t)((arg_d*1e6) + 0.5); /* .5 Hz offset to get rounding instead of truncating */
                }
                break;
            default:
                printf("ERROR: argument parsing\n");
                usage();
                return -1;
        }
    }

    printf("===== sx1302 sx125x RX test =====\n");

    /* Board reset */
    system("./reset_lgw.sh start");

    lgw_connect();

    sx125x_init(ft);

    printf("Channel configuration:\n");
    for (i = 0; i < 8; i++) {
        printf(" %d: %u Hz, if:%d Hz, reg:%d\n", i, (int32_t)ft + channel_if[i], channel_if[i], IF_HZ_TO_REG(channel_if[i]));
    }

    x = sx125x_configure_channels();
    if (x != 0) {
        printf("ERROR: failed to configure channels\n");
        return -1;
    }

    printf("Waiting for packets...\n");
    while (1) {
        sx125x_receive();
        wait_ms(10);
    }

    sx125x_set_idle();

    lgw_disconnect();
    printf("=========== Test End ===========\n");

    return 0;
}

/* --- EOF ------------------------------------------------------------------ */
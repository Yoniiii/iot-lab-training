#include <string.h>

#include "timex.h"
#include "ztimer.h"

#include "sx127x.h"
#include "sx127x_netdev.h"
#include "sx127x_params.h"

#include "net/loramac.h"
#include "semtech_loramac.h"

#include "hts221.h"
#include "hts221_params.h"

#include "board.h"

/* TODO: Add the cayenne_lpp header here */
#include "cayenne_lpp.h"
/* Declare globally the sx127x radio driver descriptor */
static sx127x_t sx127x;

/* Declare globally the loramac descriptor */
static semtech_loramac_t loramac;

/* Declare globally the sensor device descriptor */
static hts221_t hts221;

/* TODO: Declare globally Cayenne LPP descriptor here */
static cayenne_lpp_t lpp;

/* Device and application informations required for OTAA activation */
static const uint8_t deveui[LORAMAC_DEVEUI_LEN] = { 0x70, 0xB3, 0xD5, 0x7E, 0xD0, 0x04, 0xD2, 0x0F };
static const uint8_t appeui[LORAMAC_APPEUI_LEN] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
static const uint8_t appkey[LORAMAC_APPKEY_LEN] = { 0x69, 0x34, 0x17, 0x73, 0x5D, 0x0D, 0xFF, 0x01, 0x90, 0x5C, 0xD7, 0xF5, 0x4A, 0x87, 0x33, 0xCE };

static void sender(void)
{
    while (1) {
        /* wait 20 secs */
        ztimer_sleep(ZTIMER_MSEC, 20 * MS_PER_SEC);

        /* do some measurements */
        uint16_t humidity = 0;
        int16_t temperature = 0;
        if (hts221_read_humidity(&hts221, &humidity) != HTS221_OK) {
            puts(" -- failed to read humidity!");
        }
        if (hts221_read_temperature(&hts221, &temperature) != HTS221_OK) {
            puts(" -- failed to read temperature!");
        }

        /* TODO: prepare cayenne lpp payload here */
        cayenne_lpp_add_temperature(&lpp, 0, (float)temperature / 10);
        cayenne_lpp_add_relative_humidity(&lpp, 1, (float)humidity / 10);

        printf("Sending LPP data\n");

        /* send the LoRaWAN message */
        uint8_t ret = semtech_loramac_send(&loramac, lpp.buffer, lpp.cursor);
        if (ret != SEMTECH_LORAMAC_TX_DONE) {
            printf("Cannot send lpp message, ret code: %d\n", ret);
        }

        /* TODO: clear buffer once done here */
        cayenne_lpp_reset(&lpp);
    }

    /* this should never be reached */
    return;
}

int main(void)
{
    if (hts221_init(&hts221, &hts221_params[0]) != HTS221_OK) {
        puts("Sensor initialization failed");
        return 1;
    }
    if (hts221_power_on(&hts221) != HTS221_OK) {
        puts("Sensor initialization power on failed");
        return 1;
    }
    if (hts221_set_rate(&hts221, hts221.p.rate) != HTS221_OK) {
        puts("Sensor continuous mode setup failed");
        return 1;
    }

    /* initialize the radio driver */
    sx127x_setup(&sx127x, &sx127x_params[0], 0);
    loramac.netdev = &sx127x.netdev;
    loramac.netdev->driver = &sx127x_driver;

    /* initialize the loramac stack */
    semtech_loramac_init(&loramac);

    /* use a fast datarate so we don't use the physical layer too much */
    semtech_loramac_set_dr(&loramac, 5);

    /* set the LoRaWAN keys */
    semtech_loramac_set_deveui(&loramac, deveui);
    semtech_loramac_set_appeui(&loramac, appeui);
    semtech_loramac_set_appkey(&loramac, appkey);

    /* start the OTAA join procedure */
    puts("Starting join procedure");
    if (semtech_loramac_join(&loramac, LORAMAC_JOIN_OTAA) != SEMTECH_LORAMAC_JOIN_SUCCEEDED) {
        puts("Join procedure failed");
        return 1;
    }

    puts("Join procedure succeeded");

    /* call the sender function */
    sender();

    return 0;
}

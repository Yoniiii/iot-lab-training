#include <string.h>

#include "board.h"
#include "timex.h"
#include "ztimer.h"

#include "sx127x.h"
#include "sx127x_netdev.h"
#include "sx127x_params.h"

#include "thread.h"
#define RECV_MSG_QUEUE                   (4U)
static msg_t _recv_queue[RECV_MSG_QUEUE];
static char _recv_stack[THREAD_STACKSIZE_DEFAULT];

#include "net/loramac.h"     /* core loramac definitions */
#include "semtech_loramac.h" /* package API */

#include "hts221.h"
#include "hts221_params.h"

static sx127x_t sx127x;      /* The sx127x radio driver descriptor */

static semtech_loramac_t loramac;  /* The loramac stack descriptor */

static hts221_t hts221; /* The HTS221 device descriptor */

static const uint8_t appeui[LORAMAC_APPEUI_LEN] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
static const uint8_t deveui[LORAMAC_DEVEUI_LEN] = { 0x70, 0xB3, 0xD5, 0x7E, 0xD0, 0x04, 0xD2, 0x0F };
static const uint8_t appkey[LORAMAC_APPKEY_LEN] = { 0x69, 0x34, 0x17, 0x73, 0x5D, 0x0D, 0xFF, 0x01, 0x90, 0x5C, 0xD7, 0xF5, 0x4A, 0x87, 0x33, 0xCE };

static void *_recv(void *arg)
{
    msg_init_queue(_recv_queue, RECV_MSG_QUEUE);
    (void)arg;
    while (1) {
        /* blocks until a message is received */
        semtech_loramac_recv(&loramac);
        loramac.rx_data.payload[loramac.rx_data.payload_len] = 0;
        printf("Data received: %s, port: %d\n",
               (char *)loramac.rx_data.payload, loramac.rx_data.port);
    }
    return NULL;
}

int main(void)
{
    /* initialize the HTS221 sensor */
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

    sx127x_setup(&sx127x, &sx127x_params[0], 0);
    loramac.netdev = &sx127x.netdev;
    loramac.netdev->driver = &sx127x_driver;

    /* initialize the loramac stack */
    semtech_loramac_init(&loramac);
    
    /* configure the device parameters */
    semtech_loramac_set_deveui(&loramac, deveui);
    semtech_loramac_set_appeui(&loramac, appeui);
    semtech_loramac_set_appkey(&loramac, appkey);

    /* change datarate to DR5 (SF7/BW125kHz) */
    semtech_loramac_set_dr(&loramac, 5);
    
    /* start the OTAA join procedure */
    if (semtech_loramac_join(&loramac, LORAMAC_JOIN_OTAA) != SEMTECH_LORAMAC_JOIN_SUCCEEDED) {
        puts("Join procedure failed");
        return 1;
    }
    puts("Join procedure succeeded");

    thread_create(_recv_stack, sizeof(_recv_stack),
            THREAD_PRIORITY_MAIN - 1, 0, _recv, NULL, "recv thread");


    while (1) {
        /* do some measurements */
        uint16_t humidity = 0;
        int16_t temperature = 0;
        if (hts221_read_humidity(&hts221, &humidity) != HTS221_OK) {
            puts("Cannot read humidity!");
        }
        if (hts221_read_temperature(&hts221, &temperature) != HTS221_OK) {
            puts("Cannot read temperature!");
        }

        char message[64];
        sprintf(message, "H: %d.%d%%, T:%d.%dC",
                (humidity / 10), (humidity % 10),
                (temperature / 10), (temperature % 10));
        printf("Sending message '%s'\n", message);
        
        if(temperature / 10 => 25)
            print("Temperature trop haute !!!\n");
        
        if(temperature / 10 =< 20)
            print("Temperature trop basse !!!\n");

        /* send the message here */
        if (semtech_loramac_send(&loramac,
                                 (uint8_t *)message, strlen(message)) != SEMTECH_LORAMAC_TX_DONE) {
            printf("Cannot send message '%s'\n", message);
        }
        else {
            printf("Message '%s' sent\n", message);
        }

        /* wait 30 seconds between each message */
        ztimer_sleep(ZTIMER_MSEC, 30 * MS_PER_SEC);
    }

    return 0; /* should never be reached */
}

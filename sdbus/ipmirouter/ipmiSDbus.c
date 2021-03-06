#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <systemd/sd-bus.h>
#include <sys/time.h>
#include <errno.h>
#include "debug.h"


sd_bus *bus = NULL;

#define INT "org.openbmc.HostIpmi"
#define OBJ "/org/openbmc/HostIpmi/1"

#define FILTER "type='signal',sender='"INT"',member='ReceivedMessage'"
#define FILTER2 "type='signal',sender='"INT"',member='himom'"



// Good place to get the sdbus header definitions
// http://www.freedesktop.org/software/systemd/man/index.html#S

 
// This function is just to prove you can monitor specific signals
static int greeting(sd_bus_message *m, void *user_data, sd_bus_error
                         *ret_error) {

    int r = 0;
    char sequence, netfn, cmd;
    
    const char *message;
    unsigned char message_char;

    printf(" *** Received greeting signal: \n");


    r = sd_bus_message_read(m, "s",  &message);
    if (r < 0) {
        fprintf(stderr, "Failed to parse signal message: %s\n", strerror(-r));
        return -1;
    }

    printf("The message is %s\n", message);

    return 0;

}

static int send_ipmi_message(unsigned char seq, unsigned char netfn, unsigned char cmd, unsigned char *buf, unsigned char len) {

    sd_bus_error error = SD_BUS_ERROR_NULL;
    sd_bus_message *m = NULL;
    sd_bus *bus = NULL;
    sd_bus_message *reply = NULL;
    const char *path;
    int r, pty;



    /* Connect to the session bus */
    // r = sd_bus_open_user(&bus);
    r = sd_bus_open_system(&bus);

    if (r < 0) {
        fprintf(stderr, "Failed to connect to system bus: %s\n", strerror(-r));
        return -1;
    }

    r = sd_bus_message_new_method_call(bus,&m,INT,OBJ,INT,"sendMessage");
    if (r < 0) {
        fprintf(stderr, "Failed to add the method object: %s\n", strerror(-r));
        goto finish;
    }



    // Add the bytes needed for the method to be called
    r = sd_bus_message_append(m, "yyy", seq, netfn, cmd);
    if (r < 0) {
        fprintf(stderr, "Failed add the netfn and others : %s\n", strerror(-r));
        goto finish;
    }

    r = sd_bus_message_append_array(m, 'y', buf, 6);
    if (r < 0) {
        fprintf(stderr, "Failed to add the string of response bytes: %s\n", strerror(-r));
        goto finish;
    }



    // Call the IPMI responder on the bus so the message can be sent to the CEC
    r = sd_bus_call(bus, m, 0, &error, &reply);
    if (r < 0) {
        fprintf(stderr, "Failed to call the method: %s", strerror(-r));
        goto finish;
    }

    r = sd_bus_message_read(reply, "x", &pty);
    printf("RC from the ipmi dbus method :%d \n", pty);
    if (r < 0) {
       fprintf(stderr, "Failed to get a rc from the method: %s\n", strerror(-r));
       goto finish;
    }


finish:
    // since we used the _new_ above for the bus object
    // we need to remove it from the bus.  Proably could
    // just init something once and not need this free.
    sd_bus_unref(bus);

    return r < 0 ? EXIT_FAILURE : EXIT_SUCCESS;

}

static int bus_signal_cb(sd_bus_message *m, void *user_data, sd_bus_error
                         *ret_error) {
    int r = 0;
    const char *msg = NULL;
    char sequence, netfn, cmd;
    const void *a;
    size_t sz;

    printf(" *** Received Signal: \n");


    r = sd_bus_message_read(m, "yyy",  &sequence, &netfn, &cmd);
    if (r < 0) {
        fprintf(stderr, "Failed to parse signal message: %s\n", strerror(-r));
        return -1;
    }

    r = sd_bus_message_read_array(m, 'y',  &a, &sz );
    if (r < 0) {
        fprintf(stderr, "Failed to parse signal message: %s\n", strerror(-r));
        return -1;
    }



    // here you could call an ipmi function
    // that's beyond the goal of this sample 
    // code.  So just create a buffer like
    // an ipmi function could return
    unsigned char buf[6] = {0, 2, 4, 0, 0, 0x1f};



    // Send the response buffer from the ipmi command
    r = send_ipmi_message(sequence, netfn, cmd, buf, 6);
    if (r < 0) {
        fprintf(stderr, "Failed to send the response message\n");
        return -1;
    }


    return 0;
}


int main(int argc, char *argv[]) {
    sd_bus_slot *slot = NULL;
    int r;
    char *mode = NULL;


    /* Connect to system bus */
    // r = sd_bus_open_user(&bus);
    r = sd_bus_open_system(&bus);
    if (r < 0) {
        fprintf(stderr, "Failed to connect to system bus: %s\n",
                strerror(-r));
        goto finish;
    }

    r = sd_bus_add_match(bus, &slot, FILTER, bus_signal_cb, NULL);
    if (r < 0) {
        fprintf(stderr, "Failed: sd_bus_add_match: %s : %s\n", strerror(-r), FILTER);
        goto finish;
    }


    r = sd_bus_add_match(bus, &slot, FILTER2, greeting, NULL);
    if (r < 0) {
        fprintf(stderr, "Failed: %d sd_bus_add_match: %s : %s\n", __LINE__,  strerror(-r), FILTER);
        goto finish;
    }


    for (;;) {

        /* Process requests */
        r = sd_bus_process(bus, NULL);
        if (r < 0) {
            fprintf(stderr, "Failed to process bus: %s\n", strerror(-r));
            goto finish;
        }
        if (r > 0) {
            continue;
        }

        r = sd_bus_wait(bus, (uint64_t) - 1);
        if (r < 0) {
            fprintf(stderr, "Failed to wait on bus: %s\n", strerror(-r));
            goto finish;
        }
    }

finish:
    sd_bus_slot_unref(slot);
    sd_bus_unref(bus);
    return r < 0 ? EXIT_FAILURE : EXIT_SUCCESS;
}

#pragma once

#include <Arduino.h>
#include <WebServer.h>
#include <DNSServer.h>
#include "BirdyConfig.h"
#include "BirdyLED.h"

// Setup-mode captive portal (v6+). Started on a boot where the mode switch is
// SPARSE/CONTINUOUS but no credentials are stored (or the BOOT button forces
// re-provisioning). Opens open AP "Birdy-Setup-<chipid>" with a DNS wildcard
// so any phone lands on the config form. On save the credentials are written
// to flash and the caller reboots; on timeout the caller goes back to deep
// sleep to protect the battery.
class BirdySetup
{
public:
    // Runs the portal. Returns true when credentials were saved (caller must
    // reboot), false on timeout (caller should deep-sleep).
    bool run(BirdyConfig &config, BirdyLED &led, uint32_t timeoutMs);

private:
    WebServer        server;
    DNSServer        dns;
    bool             saved = false;
};

#include <WiFiManager.h> // https://github.com/tzapu/WiFiManager
#include <ESPmDNS.h>
#include "main.h"
#include "ice.h"
#include "spiffs.h"
#include "adc_c3.h"
#include "socket.h"

// GPIO pins we use
#define BOOT_PIN 9
#define LED_PIN 10

/* build version in simple format */
const char *fwVersionStr = "V0.1";

/* build time */
const char *bdate = __DATE__;
const char *btime = __TIME__;

// WiFManager object
WiFiManager wm;

unsigned long led_milli, boot_milli;
uint8_t boot_state;

// ----------------------------------
// Arduino Setup - runs once at start
// ----------------------------------
void setup()
{
  // delay is needed to allow USB CDC to set up (???)
  delay(1000);
  
  // startup
  log_i("-----------------------------");
  log_i("ICE-V Arduino starting...");
  log_i("Version: %s", fwVersionStr);
  log_i("Build Date: %s", bdate);
  log_i("Build Time: %s", btime);
  log_i("-----------------------------");
  
  /* init FPGA SPI port */
  ICE_Init();
  log_i("FPGA SPI port initialized");
  
#if 1
  log_i("Initializing SPIFFS");
  spiffs_init();
  
  /* configure FPGA from SPIFFS file */
  log_i("Reading file %s", cfg_file);
  uint8_t *bin = NULL;
  uint32_t sz;
  if(!spiffs_read((char *)cfg_file, &bin, &sz))
  {
    uint8_t cfg_stat; 
    /* loop on config failure */
    if((cfg_stat = ICE_FPGA_Config(bin, sz)))
      log_w("FPGA configured ERROR - status = %d", cfg_stat);
    else
      log_i("FPGA configured OK - status = %d", cfg_stat);
    free(bin);
  }
#endif
  
  /* init ADC for Vbat readings */
  if(!adc_c3_init())
    log_i("ADC Initialized");
  else
    log_w("ADC Init Failed");

  WiFi.mode(WIFI_STA); // explicitly set mode, esp defaults to STA+AP
  
  // custom hostname derived from MAC address
  String mac = WiFi.macAddress();
  String UID = String(mac.substring(9,11) + mac.substring(12,14) + mac.substring(15,17));
  wm.setHostname(String("ice-v_" + UID));

  // Automatically connect using saved credentials,
  // if connection fails, it starts an access point with the specified name ( "ICE-V_AP"),
  // if empty will auto generate SSID, if password is blank it will be anonymous AP (wm.autoConnect())
  // then goes into a blocking loop awaiting configuration and will return success result

  bool res;
  // res = wm.autoConnect(); // auto generated AP name from chipid
  // res = wm.autoConnect("AutoConnectAP"); // anonymous ap
  
  // password protected ap
  if(!wm.autoConnect("ICE-V_AP","ice-vpwd"))
  {
      log_e("Failed to connect");
      // ESP.restart();
  } 
  else
  {
    //if you get here you have connected to the WiFi    
    log_i("WiFi STA connected. Starting services...");
    
    // Set up mDNS responder:
    // - first argument is the domain name, in this example
    //   the fully-qualified domain name is "esp32.local"
    // - second argument is the IP address to advertise
    //   we send our IP address on the WiFi network
    if(!MDNS.begin(WiFi.getHostname()))
    {
        log_e("Error setting up MDNS responder!");
        while(1)
        {
            delay(1000);
        }
    }
    log_i("mDNS responder started. Name = %s.local", WiFi.getHostname());

#if 1
    // Start the TCP socket server for the FPGA command handler
    socket_init();
    
    // Add service to mDNS
    MDNS.addService("_FPGA", "tcp", PORT);
    log_i("mDNS _FPGA service added");
#else
    log_i("Skipped TCP Server startup");
#endif    
  }
      
  // initialize the BOOT pin as an input.
  pinMode(BOOT_PIN, INPUT_PULLUP);
  boot_milli = 0;
  boot_state = digitalRead(BOOT_PIN);
  
  // initialize the LED pin as an output.
  pinMode(LED_PIN, OUTPUT);
  led_milli = millis()+100;
}

// --------------------------------------------
// Arduino Loop - runs continuously after Setup
// --------------------------------------------
void loop()
{
  // Check for BOOT button pressed
  uint8_t curr_boot_state = digitalRead(BOOT_PIN);
  if(curr_boot_state != boot_state)
  {
    boot_state = curr_boot_state;
    boot_milli = millis();
  }

  // Check for long press on BOOT button
  if((curr_boot_state == LOW) && (boot_milli + 3000 < millis()))
  {
    log_i("Long press on BOOT - Resetting WiFi credentials.");
    boot_milli = millis();
    
    // reset settings - wipe stored credentials
    wm.resetSettings();

    // reboot into SoftAP
    ESP.restart();
  }
  
  // Nonblocking LED blink
  if(led_milli < millis())
  {
    // compute next toggle time
    led_milli = millis() + 100;

    // Toggle LED
    digitalWrite(LED_PIN, 1^digitalRead(LED_PIN));
  }

  vTaskDelay(1);  // yield to OS - is this needed?
}

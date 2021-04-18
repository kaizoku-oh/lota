#ifndef _WIFI_AP_H_
#define _WIFI_AP_H_

#include <inttypes.h>
#include <esp_wifi.h>

#define WIFI_AP_SSID                             "LOTA"
#define WIFI_AP_PASS                             "opensesame"
#define WIFI_AP_MAX_STA_CONN                     4

typedef void (*pf_wifi_ap_cb)(int32_t);

void wifi_ap_init(void);
void wifi_ap_register_cb(pf_wifi_ap_cb pf_cb);

#endif /* _WIFI_AP_H_ */
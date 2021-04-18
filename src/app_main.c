#include <esp_log.h>
#include "wifi_ap.h"
#include "web_server.h"

#define MAIN_APP_TAG                                 "MAIN_APP"

static void _wifi_ap_handler(int32_t s32EventId)
{
  switch(s32EventId)
  {
  case WIFI_EVENT_AP_STACONNECTED:
    ESP_LOGI(MAIN_APP_TAG, "Station connected");
    break;
  case WIFI_EVENT_AP_STADISCONNECTED:
    ESP_LOGI(MAIN_APP_TAG, "Station disconnected");
    break;
  default:
    break;
  }
}

void app_main(void)
{
  ESP_LOGI(MAIN_APP_TAG, "Main App started");
  wifi_ap_register_cb(_wifi_ap_handler);
  wifi_ap_init();

  web_server_start();
}

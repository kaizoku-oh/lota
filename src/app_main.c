#include <esp_log.h>
#include <nvs_flash.h>

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
  esp_err_t s32RetVal;

  /* Initialize NVS */
  s32RetVal = nvs_flash_init();
  if((ESP_ERR_NVS_NO_FREE_PAGES == s32RetVal) || (ESP_ERR_NVS_NEW_VERSION_FOUND == s32RetVal))
  {
    ESP_ERROR_CHECK(nvs_flash_erase());
    s32RetVal = nvs_flash_init();
  }
  ESP_ERROR_CHECK(s32RetVal);
  ESP_LOGI(MAIN_APP_TAG, "Main App started");
  wifi_ap_register_cb(_wifi_ap_handler);
  wifi_ap_init();

  web_server_start();
}

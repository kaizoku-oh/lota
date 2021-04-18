#include <string.h>
#include <esp_event.h>
#include <esp_netif.h>
#include <esp_system.h>
#include <esp_log.h>
#include <nvs_flash.h>
#include <lwip/err.h>
#include <lwip/sys.h>

#include "wifi_ap.h"

#define WIFI_AP_TAG                              "WIFI_AP"

wifi_config_t stWifiConfig =
{
  .ap =
  {
    .ssid = WIFI_AP_SSID,
    .ssid_len = strlen(WIFI_AP_SSID),
    .password = WIFI_AP_PASS,
    .max_connection = WIFI_AP_MAX_STA_CONN,
    .authmode = WIFI_AUTH_WPA_WPA2_PSK
  },
};

static pf_wifi_ap_cb pfWifiApCb;

static void _wifi_event_handler(void *pvArg,
                                esp_event_base_t pcEventBase,
                                int32_t s32EventId,
                                void *pvEventData)
{
  wifi_event_ap_staconnected_t *pstConEvent;
  wifi_event_ap_stadisconnected_t *pstDisconEvent;

  switch(s32EventId)
  {
  case WIFI_EVENT_AP_START:
    ESP_LOGI(WIFI_AP_TAG, "AP started");
    break;
  case WIFI_EVENT_AP_STOP:
    ESP_LOGW(WIFI_AP_TAG, "AP stopped");
    break;
  case WIFI_EVENT_AP_PROBEREQRECVED:
    ESP_LOGI(WIFI_AP_TAG, "AP probe received");
    break;
  case WIFI_EVENT_AP_STACONNECTED:
    pstConEvent = (wifi_event_ap_staconnected_t *)pvEventData;
    ESP_LOGI(WIFI_AP_TAG,
             "Station "MACSTR" is connected, AID=%d",
             MAC2STR(pstConEvent->mac),
             pstConEvent->aid);
    break;
  case WIFI_EVENT_AP_STADISCONNECTED:
    pstDisconEvent = (wifi_event_ap_stadisconnected_t *)pvEventData;
    ESP_LOGI(WIFI_AP_TAG,
             "station "MACSTR" left, AID=%d",
             MAC2STR(pstDisconEvent->mac),
             pstDisconEvent->aid);
    break;
  default:
    ESP_LOGW(WIFI_AP_TAG, "Unknown wifi event");
    break;
  }
  if(pfWifiApCb)
  {
    pfWifiApCb(s32EventId);
  }
  else
  {
    ESP_LOGW(WIFI_AP_TAG, "WiFi callback is not registered");
  }
}

void _wifi_init_soft_ap(void)
{
  ESP_ERROR_CHECK(esp_netif_init());
  ESP_ERROR_CHECK(esp_event_loop_create_default());
  esp_netif_create_default_wifi_ap();
  wifi_init_config_t stConfig = WIFI_INIT_CONFIG_DEFAULT();
  ESP_ERROR_CHECK(esp_wifi_init(&stConfig));
  ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                      ESP_EVENT_ANY_ID,
                                                      &_wifi_event_handler,
                                                      NULL,
                                                      NULL));
  if(0 == strlen(WIFI_AP_PASS))
  {
    stWifiConfig.ap.authmode = WIFI_AUTH_OPEN;
  }
  ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
  ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_AP, &stWifiConfig));
  ESP_ERROR_CHECK(esp_wifi_start());
  ESP_LOGI(WIFI_AP_TAG,
           "Finished configuring AP with SSID: %s password: %s",
           WIFI_AP_SSID,
           WIFI_AP_PASS);
}

void wifi_ap_init(void)
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
  ESP_LOGI(WIFI_AP_TAG, "Starting ESP32 WIFI in Access Point mode");
  _wifi_init_soft_ap();
}

void wifi_ap_register_cb(pf_wifi_ap_cb pfCb)
{
  if(pfCb)
  {
    pfWifiApCb = pfCb;
  }
}
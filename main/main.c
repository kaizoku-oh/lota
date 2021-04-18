/*
  This code is based on the ESP-IDF WebSocket Component:
  https://github.com/Molorius/esp32-websocket

  The ESP32 act as a WiFi Access Point,
  an http server for serving static files (html, js, css, etc.)
  and a websocket server for redirecting uart realtime data to websocket clients.

  default SSID:       "ESP32 AP"
  default PASSWORD:   "opensesame"
  default SERVER IP:  "192.168.4.1"
*/

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#include "lwip/api.h"

#include "esp_wifi.h"
#include "esp_log.h"
#include "esp_event_loop.h"
#include "nvs_flash.h"
#include "driver/uart.h"

#include "string.h"

#include "websocket_server.h"

#define LED_PIN                       CONFIG_LED_PIN
#define UART_NUM                      UART_NUM_0
#define AP_SSID                       CONFIG_AP_SSID
#define AP_PSSWD                      CONFIG_AP_PSSWD

/* Set the number of consecutive and identical characters
received by receiver which defines a UART pattern */
#define PATTERN_CHR_NUM               3
#define BUF_SIZE                      1024
#define BAUDRATE                      115200
#define RD_BUF_SIZE                   BUF_SIZE

static QueueHandle_t xUartQueue;
static QueueHandle_t xClientQueue;

const static int xClientQueueSize = 10;

/* Handle UART events */
static void uart_event_task(void *pvParameters)
{
  static const char *TAG = "uart_event_task";
  uart_event_t event;
  size_t buffered_size;
  uint8_t* dtmp = (uint8_t*) malloc(RD_BUF_SIZE);

  for(;;)
  {
    /* Blocked waiting for UART event */
    if(xQueueReceive(xUartQueue, (void * )&event, (portTickType)portMAX_DELAY))
    {
      bzero(dtmp, RD_BUF_SIZE);
      ESP_LOGI(TAG, "uart[%d] event:", UART_NUM);
      switch(event.type)
      {
        /* Event of UART receving data: We'd better handler data event fast,
        there would be much more data events than other types of events.
        If we take too much time on data event, the queue might be full.*/
        case UART_DATA:
          ESP_LOGI(TAG, "[UART DATA]: %d", event.size);
          uart_read_bytes(UART_NUM, dtmp, event.size, portMAX_DELAY);
          ESP_LOGI(TAG, "[DATA EVT]:");
          uart_write_bytes(UART_NUM, (const char*) dtmp, event.size);
          ws_server_send_text_all((char*) dtmp, event.size);
          break;
        /* Event of HW FIFO overflow detected */
        case UART_FIFO_OVF:
          ESP_LOGI(TAG, "hw fifo overflow");
          /* If fifo overflow happened, you should consider adding flow control for your application.
          The ISR has already reset the rx FIFO, As an example, we directly flush the rx buffer here
          in order to read more data. */
          uart_flush_input(UART_NUM);
          xQueueReset(xUartQueue);
          break;
        /* Event of UART ring buffer full */
        case UART_BUFFER_FULL:
          ESP_LOGI(TAG, "ring buffer full");
          /* If buffer full happened, you should consider encreasing your buffer size
          As an example, we directly flush the rx buffer here in order to read more data. */
          uart_flush_input(UART_NUM);
          xQueueReset(xUartQueue);
          break;
        /* Event of UART RX break detected */
        case UART_BREAK:
          ESP_LOGI(TAG, "uart rx break");
          break;
        /* Event of UART parity check error */
        case UART_PARITY_ERR:
          ESP_LOGI(TAG, "uart parity error");
          break;
        /* Event of UART frame error */
        case UART_FRAME_ERR:
          ESP_LOGI(TAG, "uart frame error");
          break;
        /* Event of detecting a UART pattern */
        case UART_PATTERN_DET:
          uart_get_buffered_data_len(UART_NUM, &buffered_size);
          int pos = uart_pattern_pop_pos(UART_NUM);
          ESP_LOGI(TAG, "[UART PATTERN DETECTED] pos: %d, buffered size: %d", pos, buffered_size);
          if (pos == -1)
          {
            /* There used to be a UART_PATTERN_DET event, but the pattern position queue is full
            so that it can not record the position. We should set a larger queue size.
            As an example, we directly flush the rx buffer here. */
            uart_flush_input(UART_NUM);
          }
          else
          {
            uart_read_bytes(UART_NUM, dtmp, pos, 100 / portTICK_PERIOD_MS);
            uint8_t pat[PATTERN_CHR_NUM + 1];
            memset(pat, 0, sizeof(pat));
            uart_read_bytes(UART_NUM, pat, PATTERN_CHR_NUM, 100 / portTICK_PERIOD_MS);
            ESP_LOGI(TAG, "read data: %s", dtmp);
            ESP_LOGI(TAG, "read pat : %s", pat);
          }
          break;
        default:
          ESP_LOGI(TAG, "uart event type: %d", event.type);
          break;
      }
    }
  }
  free(dtmp);
  dtmp = NULL;
  vTaskDelete(NULL);
}

/* Handle WiFi events */
static esp_err_t wifi_event_handler(void* ctx, system_event_t* event)
{
  const char* TAG = "wifi_event_handler";

  switch(event->event_id)
  {
    case SYSTEM_EVENT_AP_START:
      //ESP_ERROR_CHECK(tcpip_adapter_set_hostname(TCPIP_ADAPTER_IF_STA, "esp32"));
      ESP_LOGI(TAG, "Access Point Started");
      break;
    case SYSTEM_EVENT_AP_STOP:
      ESP_LOGI(TAG, "Access Point Stopped");
      break;
    case SYSTEM_EVENT_AP_STACONNECTED:
      ESP_LOGI(TAG, "STA Connected, MAC=%02x:%02x:%02x:%02x:%02x:%02x AID=%i",
        event->event_info.sta_connected.mac[0], event->event_info.sta_connected.mac[1],
        event->event_info.sta_connected.mac[2], event->event_info.sta_connected.mac[3],
        event->event_info.sta_connected.mac[4], event->event_info.sta_connected.mac[5],
        event->event_info.sta_connected.aid);
      break;
    case SYSTEM_EVENT_AP_STADISCONNECTED:
      ESP_LOGI(TAG, "STA Disconnected, MAC=%02x:%02x:%02x:%02x:%02x:%02x AID=%i",
        event->event_info.sta_disconnected.mac[0], event->event_info.sta_disconnected.mac[1],
        event->event_info.sta_disconnected.mac[2], event->event_info.sta_disconnected.mac[3],
        event->event_info.sta_disconnected.mac[4], event->event_info.sta_disconnected.mac[5],
        event->event_info.sta_disconnected.aid);
      break;
    case SYSTEM_EVENT_AP_PROBEREQRECVED:
      ESP_LOGI(TAG, "AP Probe Received");
      break;
    case SYSTEM_EVENT_AP_STA_GOT_IP6:
      ESP_LOGI(TAG,"Got IP6=%01x:%01x:%01x:%01x",
        event->event_info.got_ip6.ip6_info.ip.addr[0], event->event_info.got_ip6.ip6_info.ip.addr[1],
        event->event_info.got_ip6.ip6_info.ip.addr[2], event->event_info.got_ip6.ip6_info.ip.addr[3]);
      break;
    default:
      ESP_LOGI(TAG,"Unregistered event=%i",event->event_id);
      break;
  }
  return ESP_OK;
}

/* Handle websocket events */
void websocket_callback(uint8_t num, WEBSOCKET_TYPE_t type, char* msg, uint64_t len)
{
  const static char* TAG = "websocket_callback";
  
  switch(type)
  {
    case WEBSOCKET_CONNECT:
      ESP_LOGI(TAG, "client %i connected!", num);
      break;
    case WEBSOCKET_DISCONNECT_EXTERNAL:
      ESP_LOGW(TAG, "client %i sent a disconnect message", num);
      break;
    case WEBSOCKET_DISCONNECT_INTERNAL:
      ESP_LOGE(TAG, "client %i was disconnected", num);
      break;
    case WEBSOCKET_DISCONNECT_ERROR:
      ESP_LOGE(TAG, "client %i was disconnected due to an error", num);
      break;
    case WEBSOCKET_TEXT:
      if(len)
      {
        switch(msg[0])
        {
          case 'O':
            /* Message contains Odometry variables */
            ESP_LOGI(TAG, "Message contains odometry variables");
            ESP_LOGI(TAG, "X = %c | Y = %c | R = %c", msg[1], msg[2], msg[3]);
            break;
          case 'L':
            /* Message contains Logging info */
            ESP_LOGI(TAG, "Message contains Logging info");
            ESP_LOGI(TAG, "got message length %i: %s", (int)len-1, &(msg[1]));
            break;
          default:
            ESP_LOGW(TAG, "got an unknown message with length %i", (int)len);
            break;
        }
      }
      break;
    case WEBSOCKET_BIN:
      ESP_LOGI(TAG, "client %i sent binary message of size %i:\n%s", num, (uint32_t)len, msg);
      break;
    case WEBSOCKET_PING:
      ESP_LOGI(TAG, "client %i pinged us with message of size %i:\n%s", num, (uint32_t)len, msg);
      break;
    case WEBSOCKET_PONG:
      ESP_LOGI(TAG, "client %i responded to the ping" ,num);
      break;
  }
}

/* Setup WiFi */
static void wifi_setup()
{
  const char* TAG = "wifi_setup";

  ESP_LOGI(TAG,"starting tcpip adapter");
  tcpip_adapter_init();
  nvs_flash_init();
  ESP_ERROR_CHECK(tcpip_adapter_dhcps_stop(TCPIP_ADAPTER_IF_AP));
  //tcpip_adapter_set_hostname(TCPIP_ADAPTER_IF_AP,"esp32");
  tcpip_adapter_ip_info_t info;
  memset(&info, 0, sizeof(info));
  IP4_ADDR(&info.ip, 192, 168, 4, 1);
  IP4_ADDR(&info.gw, 192, 168, 4, 1);
  IP4_ADDR(&info.netmask, 255, 255, 255, 0);
  ESP_LOGI(TAG,"setting gateway IP");
  ESP_ERROR_CHECK(tcpip_adapter_set_ip_info(TCPIP_ADAPTER_IF_AP, &info));
  //ESP_ERROR_CHECK(tcpip_adapter_set_hostname(TCPIP_ADAPTER_IF_AP,"esp32"));
  //ESP_LOGI(TAG,"set hostname to \"%s\"",hostname);
  ESP_LOGI(TAG,"starting DHCPS adapter");
  ESP_ERROR_CHECK(tcpip_adapter_dhcps_start(TCPIP_ADAPTER_IF_AP));
  //ESP_ERROR_CHECK(tcpip_adapter_set_hostname(TCPIP_ADAPTER_IF_AP,hostname));
  ESP_LOGI(TAG,"starting event loop");
  ESP_ERROR_CHECK(esp_event_loop_init(wifi_event_handler, NULL));

  ESP_LOGI(TAG,"initializing WiFi");
  wifi_init_config_t wifi_init_config = WIFI_INIT_CONFIG_DEFAULT();
  ESP_ERROR_CHECK(esp_wifi_init(&wifi_init_config));
  ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
  ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));

  wifi_config_t wifi_config =
  {
    .ap =
    {
      .ssid = AP_SSID,
      .password= AP_PSSWD,
      .channel = 0,
      .authmode = WIFI_AUTH_WPA2_PSK,
      .ssid_hidden = 0,
      .max_connection = 4,
      .beacon_interval = 100
    }
  };

  ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));
  ESP_ERROR_CHECK(esp_wifi_start());
  ESP_LOGI(TAG,"WiFi set up");
}

/* Setup UART */
static void uart_setup()
{
  const char* TAG = "uart_setup";

  uart_config_t uart_config =
  {
    .baud_rate = BAUDRATE,
    .data_bits = UART_DATA_8_BITS,
    .parity = UART_PARITY_DISABLE,
    .stop_bits = UART_STOP_BITS_1,
    .flow_ctrl = UART_HW_FLOWCTRL_DISABLE
  };

  /* Set UART configurations */
  uart_param_config(UART_NUM, &uart_config);
  
  /* Set UART pins (using UART0 default pins ie no changes.) */
  uart_set_pin(UART_NUM, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
  
  /* Install UART driver, and get the queue */
  uart_driver_install(UART_NUM, BUF_SIZE * 2, BUF_SIZE * 2, 20, &xUartQueue, 0);

  /* Set uart pattern detect function. */
  uart_enable_pattern_det_baud_intr(UART_NUM, '+', PATTERN_CHR_NUM, 9, 0, 0);
  
  /* Reset the pattern queue length to record at most 20 pattern positions. */
  uart_pattern_queue_reset(UART_NUM, 20);

  ESP_LOGI(TAG, "UART set up");
}

/* Serve web files to client */
static void http_serve(struct netconn *conn)
{
  const static char* TAG = "http_server";
  const static char HTML_HEADER[] = "HTTP/1.1 200 OK\nContent-type: text/html\n\n";
  const static char ERROR_HEADER[] = "HTTP/1.1 404 Not Found\nContent-type: text/html\n\n";
  const static char JS_HEADER[] = "HTTP/1.1 200 OK\nContent-type: text/javascript\n\n";
  const static char CSS_HEADER[] = "HTTP/1.1 200 OK\nContent-type: text/css\n\n";
  //const static char PNG_HEADER[] = "HTTP/1.1 200 OK\nContent-type: image/png\n\n";
  const static char ICO_HEADER[] = "HTTP/1.1 200 OK\nContent-type: image/x-icon\n\n";
  //const static char PDF_HEADER[] = "HTTP/1.1 200 OK\nContent-type: application/pdf\n\n";
  //const static char EVENT_HEADER[] = "HTTP/1.1 200 OK\nContent-Type: text/event-stream\nCache-Control: no-cache\nretry: 3000\n\n";
  struct netbuf* inbuf;
  static char* buf;
  static uint16_t buflen;
  static err_t err;

  /* Home page */
  extern const uint8_t index_html_start[] asm("_binary_index_html_start");
  extern const uint8_t index_html_end[] asm("_binary_index_html_end");
  const uint32_t index_html_len = index_html_end - index_html_start;

  /* script.js */
  extern const uint8_t script_js_start[] asm("_binary_script_js_start");
  extern const uint8_t script_js_end[] asm("_binary_script_js_end");
  const uint32_t script_js_len = script_js_end - script_js_start;

  /* style.css */
  extern const uint8_t style_css_start[] asm("_binary_style_css_start");
  extern const uint8_t style_css_end[] asm("_binary_style_css_end");
  const uint32_t style_css_len = style_css_end - style_css_start;

  /* favicon.ico */
  extern const uint8_t favicon_ico_start[] asm("_binary_favicon_ico_start");
  extern const uint8_t favicon_ico_end[] asm("_binary_favicon_ico_end");
  const uint32_t favicon_ico_len = favicon_ico_end - favicon_ico_start;

  /* Error page */
  extern const uint8_t error_html_start[] asm("_binary_error_html_start");
  extern const uint8_t error_html_end[] asm("_binary_error_html_end");
  const uint32_t error_html_len = error_html_end - error_html_start;
 
  /* Allow a connection timeout of 1 second */
  netconn_set_recvtimeout(conn, 1000);
  ESP_LOGI(TAG, "reading from client...");
  err = netconn_recv(conn, &inbuf);
  ESP_LOGI(TAG, "read from client");
  if(err == ERR_OK)
  {
    netbuf_data(inbuf, (void**)&buf, &buflen);
    if(buf)
    {
      /* Default page */
      if (strstr(buf, "GET / ") && !strstr(buf, "Upgrade: websocket"))
      {
        ESP_LOGI(TAG, "Sending /");
        netconn_write(conn, HTML_HEADER, sizeof(HTML_HEADER)-1, NETCONN_NOCOPY);
        netconn_write(conn, index_html_start, index_html_len, NETCONN_NOCOPY);
        netconn_close(conn);
        netconn_delete(conn);
        netbuf_delete(inbuf);
      }

      /* Home page websocket */
      else if(strstr(buf, "GET / ") && strstr(buf, "Upgrade: websocket"))
      {
        ESP_LOGI(TAG, "Requesting websocket on /");
        ws_server_add_client(conn, buf, buflen, "/", websocket_callback);
        netbuf_delete(inbuf);
      }

      else if(strstr(buf, "GET /script.js "))
      {
        ESP_LOGI(TAG, "Sending /script.js");
        netconn_write(conn, JS_HEADER, sizeof(JS_HEADER)-1, NETCONN_NOCOPY);
        netconn_write(conn, script_js_start, script_js_len, NETCONN_NOCOPY);
        netconn_close(conn);
        netconn_delete(conn);
        netbuf_delete(inbuf);
      }

      else if(strstr(buf, "GET /style.css "))
      {
        ESP_LOGI(TAG, "Sending /style.css");
        netconn_write(conn, CSS_HEADER, sizeof(CSS_HEADER)-1, NETCONN_NOCOPY);
        netconn_write(conn, style_css_start, style_css_len, NETCONN_NOCOPY);
        netconn_close(conn);
        netconn_delete(conn);
        netbuf_delete(inbuf);
      }

      else if(strstr(buf, "GET /favicon.ico "))
      {
        ESP_LOGI(TAG, "Sending favicon.ico");
        netconn_write(conn, ICO_HEADER, sizeof(ICO_HEADER)-1, NETCONN_NOCOPY);
        netconn_write(conn, favicon_ico_start, favicon_ico_len, NETCONN_NOCOPY);
        netconn_close(conn);
        netconn_delete(conn);
        netbuf_delete(inbuf);
      }

      else if(strstr(buf, "GET /"))
      {
        ESP_LOGI(TAG, "Unknown request, sending error page: %s", buf);
        netconn_write(conn, ERROR_HEADER, sizeof(ERROR_HEADER)-1, NETCONN_NOCOPY);
        netconn_write(conn, error_html_start, error_html_len, NETCONN_NOCOPY);
        netconn_close(conn);
        netconn_delete(conn);
        netbuf_delete(inbuf);
      }

      else
      {
        ESP_LOGI(TAG, "Unknown request");
        netconn_close(conn);
        netconn_delete(conn);
        netbuf_delete(inbuf);
      }
    }
    else
    {
      ESP_LOGI(TAG, "Unknown request (empty?...)");
      netconn_close(conn);
      netconn_delete(conn);
      netbuf_delete(inbuf);
    }
  }
  else
  { // if err == ERR_OK
    ESP_LOGI(TAG, "error on read, closing connection");
    netconn_close(conn);
    netconn_delete(conn);
    netbuf_delete(inbuf);
  }
}

/* Handle clients when they first connect and passes them to queue */
static void server_task(void* pvParameters)
{
  const static char* TAG = "server_task";
  struct netconn *conn, *newconn;
  static err_t err;
  xClientQueue = xQueueCreate(xClientQueueSize, sizeof(struct netconn*));

  conn = netconn_new(NETCONN_TCP);
  netconn_bind(conn, NULL, 80);
  netconn_listen(conn);
  ESP_LOGI(TAG, "server listening");
  do
  {
    err = netconn_accept(conn, &newconn);
    ESP_LOGI(TAG, "new client");
    if(err == ERR_OK)
    {
      xQueueSendToBack(xClientQueue, &newconn, portMAX_DELAY);
      // http_serve(newconn);
    }
  } while(err == ERR_OK);
  netconn_close(conn);
  netconn_delete(conn);
  ESP_LOGE(TAG, "task ending, rebooting board");
  esp_restart();
}

/* Receives clients from queue and handle them */
static void server_handle_task(void* pvParameters)
{
  const static char* TAG = "server_handle_task";
  struct netconn* conn;
  ESP_LOGI(TAG, "task starting");
  for(;;)
  {
    xQueueReceive(xClientQueue, &conn, portMAX_DELAY);
    if(!conn)
      continue;
    http_serve(conn);
  }
  vTaskDelete(NULL);
}

/* Send counter value to clients periodically */
static void count_task(void* pvParameters)
{
  const static char* TAG = "count_task";
  char out[20];
  int len;
  int clients;
  const static char* word = "%i";
  uint8_t n = 0;
  const int DELAY = 1000 / portTICK_PERIOD_MS;

  ESP_LOGI(TAG, "starting task");
  for(;;)
  {
    len = sprintf(out, word, n);
    clients = ws_server_send_text_all(out, len);
    if(clients > 0)
    {
      // ESP_LOGI(TAG, "sent: \"%s\" to %i clients", out, clients);
    }
    n++;
    vTaskDelay(DELAY);
  }
}

void app_main()
{
  uart_setup();
  wifi_setup();
  ws_server_start();
  
  xTaskCreate(&uart_event_task, "uart_event_task", 2048, NULL, 12, NULL);
  xTaskCreate(&server_task, "server_task", 3000, NULL, 9, NULL);
  xTaskCreate(&server_handle_task, "server_handle_task", 4000, NULL, 6, NULL);
  xTaskCreate(&count_task, "count_task", 6000, NULL, 2, NULL);
}

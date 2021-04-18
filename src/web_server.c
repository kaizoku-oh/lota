#include <esp_log.h>
#include "web_server.h"

static esp_err_t _root_get_handler(httpd_req_t *pstRequest);

#define WEB_SERVER_TAG                           "WEB_SERVER"
#define WEB_SERVER_QUERY_PARAM_MAX_SIZE          32

static const httpd_uri_t stRootUriHandler =
{
  .uri = "/",
  .method = HTTP_GET,
  .handler = _root_get_handler,
  .user_ctx = NULL
};

static esp_err_t _root_get_handler(httpd_req_t *pstRequest)
{
  char *pcString;
  char tcQueryParam[WEB_SERVER_QUERY_PARAM_MAX_SIZE];
  uint32_t u32StringLength;

  /* 1. Get header value string length and allocate memory for length + 1,
   * extra byte for null termination */
  u32StringLength = httpd_req_get_hdr_value_len(pstRequest, "Host") + 1;
  if(u32StringLength > 1)
  {
    pcString = malloc(u32StringLength);
    /* Copy null terminated value string into buffer */
    if(ESP_OK == httpd_req_get_hdr_value_str(pstRequest, "Host", pcString, u32StringLength))
    {
      ESP_LOGI(WEB_SERVER_TAG, "Found header => Host: %s", pcString);
    }
    free(pcString);
  }
  /* 2. Read URL query string length and allocate memory for length + 1,
   * extra byte for null termination */
  u32StringLength = httpd_req_get_url_query_len(pstRequest) + 1;
  if(u32StringLength > 1)
  {
    pcString = malloc(u32StringLength);
    if(ESP_OK == httpd_req_get_url_query_str(pstRequest, pcString, u32StringLength))
    {
      ESP_LOGI(WEB_SERVER_TAG, "Found URL query => %s", pcString);
      /* Get value of expected key from query string */
      if(ESP_OK == httpd_query_key_value(pcString, "query1", tcQueryParam, sizeof(tcQueryParam)))
      {
        ESP_LOGI(WEB_SERVER_TAG, "Found URL query parameter => query1=%s", tcQueryParam);
      }
      if(ESP_OK == httpd_query_key_value(pcString, "query3", tcQueryParam, sizeof(tcQueryParam)))
      {
        ESP_LOGI(WEB_SERVER_TAG, "Found URL query parameter => query3=%s", tcQueryParam);
      }
      if(ESP_OK == httpd_query_key_value(pcString, "query2", tcQueryParam, sizeof(tcQueryParam)))
      {
        ESP_LOGI(WEB_SERVER_TAG, "Found URL query parameter => query2=%s", tcQueryParam);
      }
    }
    free(pcString);
  }
  /* 3. Send hardcoded response */
  httpd_resp_send(pstRequest,
                  "<h1>Welcome to LOTA web dashboard</h1>",
                  (sizeof("<h1>Welcome to LOTA web dashboard</h1>")-1));
  /* After sending the HTTP response the old HTTP request
   * headers are lost. Check if HTTP request headers can be read now. */
  if(0 == httpd_req_get_hdr_value_len(pstRequest, "Host"))
  {
    ESP_LOGI(WEB_SERVER_TAG, "Request headers lost");
  }
  return ESP_OK;
}

httpd_handle_t web_server_start(void)
{
  httpd_handle_t pvServer;
  httpd_config_t stConfig = HTTPD_DEFAULT_CONFIG();

  ESP_LOGI(WEB_SERVER_TAG, "Starting server on port: '%d'", stConfig.server_port);
  if(ESP_OK == httpd_start(&pvServer, &stConfig))
  {
    ESP_LOGI(WEB_SERVER_TAG, "Registering URI handlers");
    httpd_register_uri_handler(pvServer, &stRootUriHandler);
  }
  else
  {
    ESP_LOGI(WEB_SERVER_TAG, "Error starting server!");
    pvServer = NULL;
  }
  return pvServer;
}

void web_server_stop(httpd_handle_t pvServer)
{
  httpd_stop(pvServer);
}
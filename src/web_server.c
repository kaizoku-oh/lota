#include <esp_log.h>
#include <esp_event.h>
#include <esp_system.h>
#include <esp_spiffs.h>
#include <nvs_flash.h>
#include <sys/param.h>

#include "web_server.h"

static esp_err_t _root_get_handler(httpd_req_t *pstRequest);
static esp_err_t _booststrap_js_get_handler(httpd_req_t *pstRequest);
static esp_err_t _booststrap_css_get_handler(httpd_req_t *pstRequest);
static esp_err_t _jquery_js_get_handler(httpd_req_t *pstRequest);

#define WEB_SERVER_TAG                           "WEB_SERVER"
#define FILE_CONTENT_LENGTH                      8192
#define WEB_SERVER_QUERY_PARAM_MAX_SIZE          32

static const esp_vfs_spiffs_conf_t stSpiffsConfig =
{
  .base_path = "/spiffs",
  .partition_label = NULL,
  .max_files = 5,
  .format_if_mount_failed = true
};

static const httpd_uri_t stRootUriHandler =
{
  .uri = "/",
  .method = HTTP_GET,
  .handler = _root_get_handler,
  .user_ctx = NULL
};

static const httpd_uri_t stIndexUriHandler =
{
  .uri = "/index.html",
  .method = HTTP_GET,
  .handler = _root_get_handler,
  .user_ctx = NULL
};

static const httpd_uri_t stBootstrapJsUriHandler =
{
  .uri = "/bootstrap.bundle.min.js",
  .method = HTTP_GET,
  .handler = _booststrap_js_get_handler,
  .user_ctx = NULL
};

static const httpd_uri_t stBootstrapCssUriHandler =
{
  .uri = "/bootstrap.min.css",
  .method = HTTP_GET,
  .handler = _booststrap_css_get_handler,
  .user_ctx = NULL
};

static const httpd_uri_t stJQueryJsUriHandler =
{
  .uri = "/jquery-3.6.0.js",
  .method = HTTP_GET,
  .handler = _jquery_js_get_handler,
  .user_ctx = NULL
};

static esp_err_t _init_filesystem(void)
{
  esp_err_t s32RetVal;
  uint32_t u32Total;
  uint32_t u32Used;

  ESP_LOGI(WEB_SERVER_TAG, "Initializing SPIFFS");
  s32RetVal = esp_vfs_spiffs_register(&stSpiffsConfig);
  switch(s32RetVal)
  {
  case ESP_OK:
    s32RetVal = esp_spiffs_info(NULL, &u32Total, &u32Used);
    if(ESP_OK != s32RetVal)
    {
      ESP_LOGE(WEB_SERVER_TAG,
               "Failed to get SPIFFS partition information (%s)",
               esp_err_to_name(s32RetVal));
    }
    else
    {
      ESP_LOGI(WEB_SERVER_TAG, "Partition size: total = %d - used = %d", u32Total, u32Used);
    }
    break;
  case ESP_FAIL:
    ESP_LOGE(WEB_SERVER_TAG, "Failed to mount or format filesystem");
    break;
  case ESP_ERR_NOT_FOUND:
    ESP_LOGE(WEB_SERVER_TAG, "Failed to find SPIFFS partition");
    break;
  default:
    ESP_LOGE(WEB_SERVER_TAG, "Failed to initialize SPIFFS (%s)", esp_err_to_name(s32RetVal));
    break;
  }
  return s32RetVal;
}

esp_err_t _root_get_handler(httpd_req_t *pstRequest)
{
  esp_err_t s32RetVal;
  char *pcFileContent;
  int32_t s32ChunkLength;
  FILE *pstFileDescriptor;

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
  /* 3. Open html file */
  s32RetVal = ESP_OK;
  pstFileDescriptor = fopen("/spiffs/index.html", "r");
  if(pstFileDescriptor)
  {
    pcFileContent = malloc(FILE_CONTENT_LENGTH);
    do
    {
      /* 4. Read a chunk of data from the file */
      s32ChunkLength = fread(pcFileContent,
                            sizeof(uint8_t),
                            sizeof(pcFileContent),
                            pstFileDescriptor);
      if(s32ChunkLength > 0)
      {
        /* 5. Send the chunk */
        if(ESP_OK != httpd_resp_send_chunk(pstRequest, pcFileContent, s32ChunkLength))
        {
          ESP_LOGE(WEB_SERVER_TAG, "File sending failed");
          httpd_resp_sendstr_chunk(pstRequest, NULL);
          httpd_resp_send_err(pstRequest, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to send file");
          s32RetVal = ESP_FAIL;
          break;
        }
      }
      else if(0 == s32ChunkLength)
      {
        ESP_LOGI(WEB_SERVER_TAG, "File reading complete");
        break;
      }
    }
    while(s32ChunkLength != 0);
    fclose(pstFileDescriptor);
    free(pcFileContent);
    ESP_LOGI(WEB_SERVER_TAG, "File sending complete");
    httpd_resp_send_chunk(pstRequest, NULL, 0);
  }
  else
  {
    ESP_LOGE(WEB_SERVER_TAG, "Failed to open file");
    s32RetVal = ESP_FAIL;
  }
  return s32RetVal;
}

esp_err_t _booststrap_js_get_handler(httpd_req_t *pstRequest)
{
  esp_err_t s32RetVal;
  char *pcFileContent;
  int32_t s32ChunkLength;
  FILE *pstFileDescriptor;

  /* 1. Open JS file */
  s32RetVal = ESP_OK;
  pstFileDescriptor = fopen("/spiffs/bootstrap.bundle.min.js", "r");
  if(pstFileDescriptor)
  {
    pcFileContent = malloc(FILE_CONTENT_LENGTH);
    do
    {
      /* 2. Read a chunk of data from the file */
      s32ChunkLength = fread(pcFileContent,
                            sizeof(uint8_t),
                            sizeof(pcFileContent),
                            pstFileDescriptor);
      if(s32ChunkLength > 0)
      {
        /* 3. Send the chunk */
        if(ESP_OK != httpd_resp_send_chunk(pstRequest, pcFileContent, s32ChunkLength))
        {
          ESP_LOGE(WEB_SERVER_TAG, "File sending failed");
          httpd_resp_sendstr_chunk(pstRequest, NULL);
          httpd_resp_send_err(pstRequest, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to send file");
          s32RetVal = ESP_FAIL;
          break;
        }
      }
      else if(0 == s32ChunkLength)
      {
        ESP_LOGI(WEB_SERVER_TAG, "File reading complete");
        break;
      }
    }
    while(s32ChunkLength != 0);
    fclose(pstFileDescriptor);
    free(pcFileContent);
    ESP_LOGI(WEB_SERVER_TAG, "File sending complete");
    httpd_resp_send_chunk(pstRequest, NULL, 0);
  }
  else
  {
    ESP_LOGE(WEB_SERVER_TAG, "Failed to open file");
    s32RetVal = ESP_FAIL;
  }
  return s32RetVal;
}

esp_err_t _booststrap_css_get_handler(httpd_req_t *pstRequest)
{
  esp_err_t s32RetVal;
  char *pcFileContent;
  int32_t s32ChunkLength;
  FILE *pstFileDescriptor;

  /* 1. Open CSS file */
  s32RetVal = ESP_OK;
  pstFileDescriptor = fopen("/spiffs/bootstrap.min.css", "r");
  if(pstFileDescriptor)
  {
    pcFileContent = malloc(FILE_CONTENT_LENGTH);
    do
    {
      /* 2. Read a chunk of data from the file */
      s32ChunkLength = fread(pcFileContent,
                            sizeof(uint8_t),
                            sizeof(pcFileContent),
                            pstFileDescriptor);
      if(s32ChunkLength > 0)
      {
        /* 3. Send the chunk */
        if(ESP_OK != httpd_resp_send_chunk(pstRequest, pcFileContent, s32ChunkLength))
        {
          ESP_LOGE(WEB_SERVER_TAG, "File sending failed");
          httpd_resp_sendstr_chunk(pstRequest, NULL);
          httpd_resp_send_err(pstRequest, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to send file");
          s32RetVal = ESP_FAIL;
          break;
        }
      }
      else if(0 == s32ChunkLength)
      {
        ESP_LOGI(WEB_SERVER_TAG, "File reading complete");
        break;
      }
    }
    while(s32ChunkLength != 0);
    fclose(pstFileDescriptor);
    free(pcFileContent);
    ESP_LOGI(WEB_SERVER_TAG, "File sending complete");
    httpd_resp_send_chunk(pstRequest, NULL, 0);
  }
  else
  {
    ESP_LOGE(WEB_SERVER_TAG, "Failed to open file");
    s32RetVal = ESP_FAIL;
  }
  return s32RetVal;
}

esp_err_t _jquery_js_get_handler(httpd_req_t *pstRequest)
{
  esp_err_t s32RetVal;
  char *pcFileContent;
  int32_t s32ChunkLength;
  FILE *pstFileDescriptor;

  /* 1. Open jQuery file */
  s32RetVal = ESP_OK;
  pstFileDescriptor = fopen("/spiffs/jquery-3.6.0.js", "r");
  if(pstFileDescriptor)
  {
    pcFileContent = malloc(FILE_CONTENT_LENGTH);
    do
    {
      /* 2. Read a chunk of data from the file */
      s32ChunkLength = fread(pcFileContent,
                            sizeof(uint8_t),
                            sizeof(pcFileContent),
                            pstFileDescriptor);
      if(s32ChunkLength > 0)
      {
        /* 3. Send the chunk */
        if(ESP_OK != httpd_resp_send_chunk(pstRequest, pcFileContent, s32ChunkLength))
        {
          ESP_LOGE(WEB_SERVER_TAG, "File sending failed");
          httpd_resp_sendstr_chunk(pstRequest, NULL);
          httpd_resp_send_err(pstRequest, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to send file");
          s32RetVal = ESP_FAIL;
          break;
        }
      }
      else if(0 == s32ChunkLength)
      {
        ESP_LOGI(WEB_SERVER_TAG, "File reading complete");
        break;
      }
    }
    while(s32ChunkLength != 0);
    fclose(pstFileDescriptor);
    free(pcFileContent);
    ESP_LOGI(WEB_SERVER_TAG, "File sending complete");
    httpd_resp_send_chunk(pstRequest, NULL, 0);
  }
  else
  {
    ESP_LOGE(WEB_SERVER_TAG, "Failed to open file");
    s32RetVal = ESP_FAIL;
  }
  return s32RetVal;
}

httpd_handle_t web_server_start(void)
{
  httpd_handle_t pvServer;
  httpd_config_t stConfig = HTTPD_DEFAULT_CONFIG();

  _init_filesystem();
  ESP_LOGI(WEB_SERVER_TAG, "Starting server on port: '%d'", stConfig.server_port);
  if(ESP_OK == httpd_start(&pvServer, &stConfig))
  {
    ESP_LOGI(WEB_SERVER_TAG, "Registering URI handlers");
    httpd_register_uri_handler(pvServer, &stRootUriHandler);
    httpd_register_uri_handler(pvServer, &stIndexUriHandler);
    httpd_register_uri_handler(pvServer, &stBootstrapJsUriHandler);
    httpd_register_uri_handler(pvServer, &stBootstrapCssUriHandler);
    httpd_register_uri_handler(pvServer, &stJQueryJsUriHandler);
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
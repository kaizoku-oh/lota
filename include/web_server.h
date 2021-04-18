#ifndef _WEB_SERVER_H_
#define _WEB_SERVER_H_

#include <esp_http_server.h>

httpd_handle_t web_server_start(void);
void web_server_stop(httpd_handle_t);

#endif /* _WEB_SERVER_H_ */
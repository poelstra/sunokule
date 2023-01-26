#include <esp_http_server.h>

#include "esp_log.h"
#include "tcpip_adapter.h"

#define MAX_PAYLOAD_LEN 128

static const char *TAG = "webserver";

httpd_handle_t server = NULL;

static esp_err_t index_handler(httpd_req_t *req) {
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, "<html><body><h1>Hello World!</h1><script>let sock=new WebSocket(`ws://${location.hostname}/ws`);</script></body></html>", -1);

    // tcpip_adapter_ip_info_t ip_info;
    // ESP_ERROR_CHECK(tcpip_adapter_get_ip_info(TCPIP_ADAPTER_IF_STA, &ip_info));

    // char url[80];
    // sprintf(url, "http://localhost:3078/?%s", ip4addr_ntoa(&ip_info.ip));

    // httpd_resp_set_status(req, "303 See Other");
    // httpd_resp_set_hdr(req, "Location", url);
    // httpd_resp_send(req, NULL, 0);
    return ESP_OK;
}

esp_err_t ws_send_string(httpd_req_t *req, char *str) {
    httpd_ws_frame_t packet;
    packet.type = HTTPD_WS_TYPE_TEXT;
    packet.payload = (uint8_t *)str;
    packet.len = strlen(str);
    esp_err_t ret = httpd_ws_send_frame(req, &packet);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "httpd_ws_send_frame failed with %d", ret);
    }
    return ret;
}

esp_err_t ws_read_length(httpd_req_t *req, httpd_ws_frame_t *packet) {
    /* Set max_len = 0 to get the frame len */
    esp_err_t ret = httpd_ws_recv_frame(req, packet, 0);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "httpd_ws_recv_frame failed to get frame len with %d", ret);
        return ret;
    }
    // ESP_LOGI(TAG, "frame length is %d", packet.len);
    return ret;
}

esp_err_t ws_read_string(httpd_req_t *req, uint8_t *buf, size_t max_len) {
    esp_err_t ret = ESP_OK;

    // packet
    httpd_ws_frame_t ws_pkt;
    memset(&ws_pkt, 0, sizeof(httpd_ws_frame_t));
    ws_pkt.type = HTTPD_WS_TYPE_TEXT;

    // get length
    ret = ws_read_length(req, &ws_pkt);
    ESP_LOGI(TAG, "frame len is %d", ws_pkt.len);

    if (ws_pkt.len + 1 > max_len) {
        ESP_LOGE(TAG, "Not enough memory to store payload, need %d", ws_pkt.len);
        return ESP_ERR_NO_MEM;
    }

    if (ws_pkt.len) {
        ws_pkt.payload = buf;
        /* Set max_len = ws_pkt.len to get the frame payload */
        ret = httpd_ws_recv_frame(req, &ws_pkt, ws_pkt.len);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "httpd_ws_recv_frame failed with %d", ret);
            return ret;
        }
    }
    ESP_LOGI(TAG, "Packet type: %d", ws_pkt.type);
    return ret;
}

static esp_err_t ws_handler(httpd_req_t *req) {
    if (req->method == HTTP_GET) {
        // handle handshake
        ESP_LOGI(TAG, "Handshake done, the new connection was opened");
        return ESP_OK;
    }
    esp_err_t ret = ESP_OK;

    // read into buffer
    uint8_t buf[128];
    memset(buf, 0, sizeof(buf));
    ret = ws_read_string(req, buf, sizeof(buf));
    ESP_LOGI(TAG, "Got packet with message: %s", buf);

    // send back
    ret = ws_send_string(req, "blalala");

    return ret;
}

void start_webserver(void) {
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.uri_match_fn = httpd_uri_match_wildcard;
    config.core_id = 0;
    config.task_priority = 4;

    ESP_ERROR_CHECK(httpd_start(&server, &config));

    httpd_uri_t index_uri = {
        .uri = "/",
        .method = HTTP_GET,
        .handler = index_handler,
        .user_ctx = NULL,
    };

    httpd_uri_t ws_uri = {
        .uri = "/ws",
        .method = HTTP_GET,
        .handler = ws_handler,
        .user_ctx = NULL,
        .is_websocket = true,
        .handle_ws_control_frames = true,
    };

    ESP_ERROR_CHECK(httpd_register_uri_handler(server, &index_uri));
    ESP_ERROR_CHECK(httpd_register_uri_handler(server, &ws_uri));
}

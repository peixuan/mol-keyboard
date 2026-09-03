/* SPDX-License-Identifier: Apache-2.0 */
#include "device_web.h"

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#include "device_control.h"
#include "esp_event.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_netif.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "esp_wifi_default.h"
#include "nvs.h"
#include "web_config_protocol.h"

#define MOL_WEB_AP_PASSWORD_HEX_LENGTH 16u

static const char* const kTag = "mol-web";
static const char* const kCredentialNamespace = "mol-web";
static const char* const kPasswordKey = "ap-psk";
static const char* const kTokenKey = "api-token";
static const char* const kOrigin = "http://192.168.4.1";
static esp_netif_t* ap_netif;
static httpd_handle_t server;
static bool network_initialized;
static bool active;
static int64_t deadline_us;
static char ap_password[MOL_WEB_AP_PASSWORD_HEX_LENGTH + 1u];
static char request_token[MOL_WEB_FORM_TOKEN_HEX_LENGTH + 1u];

static bool is_lower_hex(const char* value, size_t length) {
  size_t index;
  for (index = 0u; index < length; ++index) {
    if (!((value[index] >= '0' && value[index] <= '9') ||
          (value[index] >= 'a' && value[index] <= 'f'))) {
      return false;
    }
  }
  return value[length] == '\0';
}

static void bytes_to_hex(const uint8_t* input, size_t input_size, char* output) {
  static const char digits[] = "0123456789abcdef";
  size_t index;
  for (index = 0u; index < input_size; ++index) {
    output[index * 2u] = digits[input[index] >> 4u];
    output[index * 2u + 1u] = digits[input[index] & 0x0fu];
  }
  output[input_size * 2u] = '\0';
}

static esp_err_t load_credentials(void) {
  nvs_handle_t handle;
  size_t password_size = sizeof(ap_password);
  size_t token_size = sizeof(request_token);
  esp_err_t result = nvs_open(kCredentialNamespace, NVS_READONLY, &handle);
  if (result != ESP_OK) {
    return result;
  }
  result = nvs_get_str(handle, kPasswordKey, ap_password, &password_size);
  if (result == ESP_OK) {
    result = nvs_get_str(handle, kTokenKey, request_token, &token_size);
  }
  nvs_close(handle);
  if (result != ESP_OK || password_size != sizeof(ap_password) ||
      token_size != sizeof(request_token) ||
      !is_lower_hex(ap_password, MOL_WEB_AP_PASSWORD_HEX_LENGTH) ||
      !is_lower_hex(request_token, MOL_WEB_FORM_TOKEN_HEX_LENGTH)) {
    memset(ap_password, 0, sizeof(ap_password));
    memset(request_token, 0, sizeof(request_token));
    return result == ESP_OK ? ESP_ERR_INVALID_SIZE : result;
  }
  return ESP_OK;
}

static esp_err_t create_credentials(void) {
  uint8_t password_random[MOL_WEB_AP_PASSWORD_HEX_LENGTH / 2u];
  uint8_t token_random[MOL_WEB_FORM_TOKEN_HEX_LENGTH / 2u];
  nvs_handle_t handle;
  bool opened = false;
  esp_err_t result;
  esp_fill_random(password_random, sizeof(password_random));
  esp_fill_random(token_random, sizeof(token_random));
  bytes_to_hex(password_random, sizeof(password_random), ap_password);
  bytes_to_hex(token_random, sizeof(token_random), request_token);
  result = nvs_open(kCredentialNamespace, NVS_READWRITE, &handle);
  if (result == ESP_OK) {
    opened = true;
    result = nvs_set_str(handle, kPasswordKey, ap_password);
  }
  if (result == ESP_OK) {
    result = nvs_set_str(handle, kTokenKey, request_token);
  }
  if (result == ESP_OK) {
    result = nvs_commit(handle);
  }
  if (opened) {
    nvs_close(handle);
  }
  if (result != ESP_OK) {
    memset(ap_password, 0, sizeof(ap_password));
    memset(request_token, 0, sizeof(request_token));
  }
  return result;
}

static esp_err_t ensure_credentials(void) {
  esp_err_t result = load_credentials();
  if (result == ESP_OK) {
    return ESP_OK;
  }
  return create_credentials();
}

static esp_err_t set_security_headers(httpd_req_t* request) {
  esp_err_t result = httpd_resp_set_hdr(request, "Cache-Control", "no-store");
  if (result == ESP_OK) {
    result = httpd_resp_set_hdr(
        request, "Content-Security-Policy",
        "default-src 'none'; style-src 'unsafe-inline'; form-action 'self'; frame-ancestors "
        "'none'; base-uri 'none'");
  }
  if (result == ESP_OK) {
    result = httpd_resp_set_hdr(request, "X-Content-Type-Options", "nosniff");
  }
  if (result == ESP_OK) {
    result = httpd_resp_set_hdr(request, "Referrer-Policy", "no-referrer");
  }
  if (result == ESP_OK) {
    result = httpd_resp_set_hdr(request, "X-Frame-Options", "DENY");
  }
  return result;
}

static esp_err_t send_error(httpd_req_t* request, const char* status, const char* message) {
  esp_err_t result = set_security_headers(request);
  if (result == ESP_OK) {
    result = httpd_resp_set_status(request, status);
  }
  if (result == ESP_OK) {
    result = httpd_resp_set_type(request, "text/plain; charset=utf-8");
  }
  if (result == ESP_OK) {
    result = httpd_resp_sendstr(request, message);
  }
  return result;
}

static esp_err_t send_chunk(httpd_req_t* request, const char* chunk) {
  return httpd_resp_sendstr_chunk(request, chunk);
}

static esp_err_t get_root(httpd_req_t* request) {
  static const char opening[] =
      "<!doctype html><html lang=en><head><meta charset=utf-8><meta name=viewport "
      "content=\"width=device-width,initial-scale=1\"><title>MoL Keyboard</title><style>"
      "body{font:16px system-ui;max-width:38rem;margin:2rem auto;padding:0 1rem;background:#111;"
      "color:#eee}form{display:grid;gap:.8rem}label{display:grid;gap:.25rem}input,select,button{"
      "font:inherit;padding:.55rem;background:#222;color:#fff;border:1px solid #666;border-radius:"
      ".3rem}button{background:#2868d7;border:0;font-weight:700}</style></head><body><h1>MoL "
      "Keyboard</h1><p>This private access point closes automatically.</p><form method=post "
      "action=/api/settings><input type=hidden name=token value=\"";
  static const char controls[] =
      "\"><label>Master gain (0-2)<input name=gain type=number min=0 max=2 step=.01 value=\"";
  mol_device_settings_t settings;
  char chunk[768];
  size_t preset;
  int length;
  esp_err_t result;
  if (!mol_device_control_get_settings(&settings)) {
    return send_error(request, "500 Internal Server Error", "settings unavailable\n");
  }
  result = set_security_headers(request);
  if (result == ESP_OK) {
    result = httpd_resp_set_type(request, "text/html; charset=utf-8");
  }
  if (result == ESP_OK) {
    result = send_chunk(request, opening);
  }
  if (result == ESP_OK) {
    result = send_chunk(request, request_token);
  }
  if (result == ESP_OK) {
    result = send_chunk(request, controls);
  }
  length = snprintf(chunk, sizeof(chunk), "%.2f\"></label><label>Preset<select name=preset>",
                    settings.master_gain);
  if (result == ESP_OK && length > 0 && (size_t)length < sizeof(chunk)) {
    result = send_chunk(request, chunk);
  }
  for (preset = 0u; result == ESP_OK && preset < MOL_PRESET_COUNT; ++preset) {
    length = snprintf(chunk, sizeof(chunk), "<option value=%u%s>%s</option>", (unsigned)preset,
                      settings.preset == preset ? " selected" : "",
                      mol_preset_english_name((mol_preset_id_t)preset));
    if (length <= 0 || (size_t)length >= sizeof(chunk)) {
      return ESP_FAIL;
    }
    result = send_chunk(request, chunk);
  }
  length = snprintf(
      chunk, sizeof(chunk),
      "</select></label><label>Octave (-3 to 3)<input name=octave type=number min=-3 max=3 "
      "value=\"%" PRId32
      "\"></label><label>Transpose (-24 to 24)<input name=transpose type=number min=-24 "
      "max=24 value=\"%" PRId32
      "\"></label><label>Tempo (30-300 BPM)<input name=tempo type=number min=30 max=300 "
      "step=.1 value=\"%.1f\"></label><label>Metronome<select name=metronome><option value=0%s>"
      "Off</option><option value=1%s>On</option></select></label>",
      settings.octave_shift, settings.transpose, settings.tempo,
      settings.metronome_enabled == 0u ? " selected" : "",
      settings.metronome_enabled != 0u ? " selected" : "");
  if (result == ESP_OK && length > 0 && (size_t)length < sizeof(chunk)) {
    result = send_chunk(request, chunk);
  }
  length = snprintf(
      chunk, sizeof(chunk),
      "<label>Metronome level (0-1)<input name=metronome_level type=number min=0 max=1 "
      "step=.01 value=\"%.2f\"></label><label>Output<select name=output><option value=i2s%s>"
      "I2S</option>",
      settings.metronome_level, settings.output_mode == MOL_DEVICE_OUTPUT_I2S ? " selected" : "");
  if (result == ESP_OK && length > 0 && (size_t)length < sizeof(chunk)) {
    result = send_chunk(request, chunk);
  }
#if CONFIG_MOL_A2DP_ENABLE
  if (result == ESP_OK) {
    length = snprintf(chunk, sizeof(chunk), "<option value=a2dp%s>Bluetooth A2DP</option>",
                      settings.output_mode == MOL_DEVICE_OUTPUT_A2DP ? " selected" : "");
    if (length <= 0 || (size_t)length >= sizeof(chunk)) {
      return ESP_FAIL;
    }
    result = send_chunk(request, chunk);
  }
#endif
  if (result == ESP_OK) {
    result = send_chunk(request,
                        "</select></label><button type=submit>Save settings</button></form></body>"
                        "</html>");
  }
  if (result == ESP_OK) {
    result = httpd_resp_send_chunk(request, NULL, 0u);
  }
  return result;
}

static bool header_equals(httpd_req_t* request, const char* name, const char* expected) {
  char value[64];
  const size_t expected_size = strlen(expected);
  if (httpd_req_get_hdr_value_len(request, name) != expected_size ||
      expected_size + 1u > sizeof(value) ||
      httpd_req_get_hdr_value_str(request, name, value, sizeof(value)) != ESP_OK) {
    return false;
  }
  return memcmp(value, expected, expected_size + 1u) == 0;
}

static esp_err_t post_settings(httpd_req_t* request) {
  char body[MOL_WEB_FORM_MAX_BODY_SIZE + 1u];
  size_t received = 0u;
  mol_device_settings_t current;
  mol_device_settings_t candidate;
  mol_web_form_result_t form_result;
  if (!header_equals(request, "Origin", kOrigin)) {
    return send_error(request, "403 Forbidden", "origin rejected\n");
  }
  if (!header_equals(request, "Content-Type", "application/x-www-form-urlencoded")) {
    return send_error(request, "415 Unsupported Media Type", "content type rejected\n");
  }
  if (request->content_len == 0u || request->content_len > MOL_WEB_FORM_MAX_BODY_SIZE) {
    return send_error(request, "413 Payload Too Large", "invalid body size\n");
  }
  while (received < request->content_len) {
    const int count = httpd_req_recv(request, body + received, request->content_len - received);
    if (count <= 0) {
      return send_error(request, "400 Bad Request", "body receive failed\n");
    }
    received += (size_t)count;
  }
  body[received] = '\0';
  if (!mol_device_control_get_settings(&current)) {
    return send_error(request, "503 Service Unavailable", "settings unavailable\n");
  }
  form_result = mol_web_form_apply(body, received, request_token,
#if CONFIG_MOL_A2DP_ENABLE
                                   true,
#else
                                   false,
#endif
                                   &current, &candidate);
  if (form_result == MOL_WEB_FORM_UNAUTHORIZED) {
    return send_error(request, "403 Forbidden", "request token rejected\n");
  }
  if (form_result != MOL_WEB_FORM_OK) {
    return send_error(
        request,
        form_result == MOL_WEB_FORM_TOO_LARGE ? "413 Payload Too Large" : "400 Bad Request",
        mol_web_form_result_string(form_result));
  }
  if (!mol_device_control_submit_settings(&candidate)) {
    return send_error(request, "503 Service Unavailable", "settings queue busy\n");
  }
  if (set_security_headers(request) != ESP_OK ||
      httpd_resp_set_status(request, "303 See Other") != ESP_OK ||
      httpd_resp_set_hdr(request, "Location", "/") != ESP_OK) {
    return ESP_FAIL;
  }
  return httpd_resp_send(request, NULL, 0u);
}

static esp_err_t initialize_network(void) {
  wifi_init_config_t wifi_initialization = WIFI_INIT_CONFIG_DEFAULT();
  esp_err_t result;
  if (network_initialized) {
    return ESP_OK;
  }
  result = esp_netif_init();
  if (result != ESP_OK && result != ESP_ERR_INVALID_STATE) {
    return result;
  }
  result = esp_event_loop_create_default();
  if (result != ESP_OK && result != ESP_ERR_INVALID_STATE) {
    return result;
  }
  ap_netif = esp_netif_create_default_wifi_ap();
  if (ap_netif == NULL) {
    return ESP_ERR_NO_MEM;
  }
  result = esp_wifi_init(&wifi_initialization);
  if (result == ESP_OK) {
    network_initialized = true;
  } else {
    esp_netif_destroy_default_wifi(ap_netif);
    ap_netif = NULL;
  }
  return result;
}

static esp_err_t start_access_point(char ssid[32]) {
  uint8_t mac[6];
  wifi_config_t configuration;
  int length;
  esp_err_t result = esp_read_mac(mac, ESP_MAC_WIFI_SOFTAP);
  if (result != ESP_OK) {
    return result;
  }
  length = snprintf(ssid, 32u, "MoL-Keyboard-%02X%02X%02X", mac[3], mac[4], mac[5]);
  if (length <= 0 || length >= 32) {
    return ESP_FAIL;
  }
  memset(&configuration, 0, sizeof(configuration));
  memcpy(configuration.ap.ssid, ssid, strlen(ssid));
  memcpy(configuration.ap.password, ap_password, sizeof(ap_password));
  configuration.ap.ssid_len = (uint8_t)strlen(ssid);
  configuration.ap.channel = CONFIG_MOL_DEVICE_WEB_AP_CHANNEL;
  configuration.ap.authmode = WIFI_AUTH_WPA2_PSK;
  configuration.ap.max_connection = CONFIG_MOL_DEVICE_WEB_MAX_CLIENTS;
  configuration.ap.beacon_interval = 100u;
  configuration.ap.pmf_cfg.required = true;
  result = esp_wifi_set_mode(WIFI_MODE_AP);
  if (result == ESP_OK) {
    result = esp_wifi_set_config(WIFI_IF_AP, &configuration);
  }
  if (result == ESP_OK) {
    result = esp_wifi_start();
  }
  return result;
}

static esp_err_t start_server(void) {
  struct ifreq interface_request;
  httpd_config_t configuration = HTTPD_DEFAULT_CONFIG();
  const httpd_uri_t root = {.uri = "/", .method = HTTP_GET, .handler = get_root, .user_ctx = NULL};
  const httpd_uri_t settings = {
      .uri = "/api/settings", .method = HTTP_POST, .handler = post_settings, .user_ctx = NULL};
  esp_err_t result;
  memset(&interface_request, 0, sizeof(interface_request));
  result = esp_netif_get_netif_impl_name(ap_netif, interface_request.ifr_name);
  if (result != ESP_OK) {
    return result;
  }
  configuration.task_priority = CONFIG_MOL_DEVICE_WEB_TASK_PRIORITY;
  configuration.stack_size = CONFIG_MOL_DEVICE_WEB_TASK_STACK_SIZE;
  configuration.core_id = CONFIG_MOL_DEVICE_WEB_TASK_CORE;
  configuration.max_open_sockets = CONFIG_MOL_DEVICE_WEB_MAX_CLIENTS + 3u;
  configuration.max_uri_handlers = 2u;
  configuration.max_resp_headers = 8u;
  configuration.backlog_conn = CONFIG_MOL_DEVICE_WEB_MAX_CLIENTS;
  configuration.lru_purge_enable = true;
  configuration.recv_wait_timeout = 3u;
  configuration.send_wait_timeout = 3u;
  configuration.if_name = &interface_request;
  result = httpd_start(&server, &configuration);
  if (result == ESP_OK) {
    result = httpd_register_uri_handler(server, &root);
  }
  if (result == ESP_OK) {
    result = httpd_register_uri_handler(server, &settings);
  }
  if (result != ESP_OK && server != NULL) {
    (void)httpd_stop(server);
    server = NULL;
  }
  return result;
}

esp_err_t mol_device_web_start(void) {
  char ssid[32];
  esp_err_t result;
  if (active) {
    deadline_us = esp_timer_get_time() + (int64_t)CONFIG_MOL_DEVICE_WEB_SESSION_SECONDS * 1000000;
    ESP_LOGI(kTag, "Physical authorization extended the local configuration session");
    return ESP_OK;
  }
  result = ensure_credentials();
  if (result == ESP_OK) {
    result = initialize_network();
  }
  if (result == ESP_OK) {
    result = start_access_point(ssid);
  }
  if (result == ESP_OK) {
    result = start_server();
  }
  if (result != ESP_OK) {
    (void)esp_wifi_stop();
    ESP_LOGE(kTag, "Could not start physically authorized configuration AP: %s",
             esp_err_to_name(result));
    return result;
  }
  deadline_us = esp_timer_get_time() + (int64_t)CONFIG_MOL_DEVICE_WEB_SESSION_SECONDS * 1000000;
  active = true;
  ESP_LOGW(kTag, "Private configuration AP enabled for %u seconds: SSID=%s password=%s",
           (unsigned)CONFIG_MOL_DEVICE_WEB_SESSION_SECONDS, ssid, ap_password);
  ESP_LOGI(kTag, "Open http://192.168.4.1 while the physical authorization window is active");
  return ESP_OK;
}

bool mol_device_web_poll(void) {
  if (active && esp_timer_get_time() >= deadline_us) {
    ESP_LOGI(kTag, "Configuration session expired; stopping HTTP and SoftAP services");
    mol_device_web_stop();
  }
  return active;
}

void mol_device_web_stop(void) {
  if (server != NULL) {
    (void)httpd_stop(server);
    server = NULL;
  }
  if (network_initialized) {
    (void)esp_wifi_stop();
  }
  active = false;
  deadline_us = 0;
  memset(ap_password, 0, sizeof(ap_password));
  memset(request_token, 0, sizeof(request_token));
}

esp_err_t mol_device_web_erase_credentials(void) {
  nvs_handle_t handle;
  esp_err_t result;
  mol_device_web_stop();
  result = nvs_open(kCredentialNamespace, NVS_READWRITE, &handle);
  if (result == ESP_ERR_NVS_NOT_FOUND) {
    return ESP_OK;
  }
  if (result != ESP_OK) {
    return result;
  }
  result = nvs_erase_all(handle);
  if (result == ESP_OK) {
    result = nvs_commit(handle);
  }
  nvs_close(handle);
  return result;
}

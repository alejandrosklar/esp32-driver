/* ESP32 implementation of NetworkInterfaceAPI
 * Copyright (c) 2017 Renesas Electronics Corporation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <string.h>
#include "ESP32Interface.h"

// ESP32Interface implementation
ESP32Interface::ESP32Interface() :
    ESP32Stack(MBED_CONF_ESP32_WIFI_EN, MBED_CONF_ESP32_WIFI_IO0, MBED_CONF_ESP32_WIFI_TX, MBED_CONF_ESP32_WIFI_RX, MBED_CONF_ESP32_WIFI_DEBUG,
               MBED_CONF_ESP32_WIFI_RTS, MBED_CONF_ESP32_WIFI_CTS, MBED_CONF_ESP32_WIFI_BAUDRATE, 0),
    _dhcp(true),
    _ap_ssid(),
    _ap_pass(),
    _ap_sec(NSAPI_SECURITY_NONE),
    _ip_address(),
    _netmask(),
    _gateway(),
    _connection_status(NSAPI_STATUS_DISCONNECTED),
    _connection_status_cb(NULL)
{
    memset(_ap_ssid, 0, sizeof(_ap_ssid));
    _esp->attach_wifi_status(callback(this, &ESP32Interface::wifi_status_cb));
}

ESP32Interface::ESP32Interface(PinName en, PinName io0, PinName tx, PinName rx, bool debug,
    PinName rts, PinName cts, int baudrate) :
    ESP32Stack(en, io0, tx, rx, debug, rts, cts, baudrate, 0),
    _dhcp(true),
    _ap_ssid(),
    _ap_pass(),
    _ap_sec(NSAPI_SECURITY_NONE),
    _ip_address(),
    _netmask(),
    _gateway(),
    _connection_status(NSAPI_STATUS_DISCONNECTED),
    _connection_status_cb(NULL)
{
    memset(_ap_ssid, 0, sizeof(_ap_ssid));
    _esp->attach_wifi_status(callback(this, &ESP32Interface::wifi_status_cb));
}

ESP32Interface::ESP32Interface(PinName tx, PinName rx, bool debug) :
    ESP32Stack(NC, NC, tx, rx, debug, NC, NC, 230400, 0),
    _dhcp(true),
    _ap_ssid(),
    _ap_pass(),
    _ap_sec(NSAPI_SECURITY_NONE),
    _ip_address(),
    _netmask(),
    _gateway(),
    _connection_status(NSAPI_STATUS_DISCONNECTED),
    _connection_status_cb(NULL)
{
    memset(_ap_ssid, 0, sizeof(_ap_ssid));
    _esp->attach_wifi_status(callback(this, &ESP32Interface::wifi_status_cb));
}

nsapi_error_t ESP32Interface::set_network(const char *ip_address, const char *netmask, const char *gateway)
{
    _dhcp = false;

    strncpy(_ip_address, ip_address ? ip_address : "", sizeof(_ip_address));
    _ip_address[sizeof(_ip_address) - 1] = '\0';
    strncpy(_netmask, netmask ? netmask : "", sizeof(_netmask));
    _netmask[sizeof(_netmask) - 1] = '\0';
    strncpy(_gateway, gateway ? gateway : "", sizeof(_gateway));
    _gateway[sizeof(_gateway) - 1] = '\0';

    return NSAPI_ERROR_OK;
}

nsapi_error_t ESP32Interface::set_dhcp(bool dhcp)
{
    _dhcp = dhcp;

    return NSAPI_ERROR_OK;
}

int ESP32Interface::connect(const char *ssid, const char *pass, nsapi_security_t security,
                                        uint8_t channel)
{
    if (channel != 0) {
        return NSAPI_ERROR_UNSUPPORTED;
    }

    int ret = set_credentials(ssid, pass, security);
    if (ret != NSAPI_ERROR_OK) {
        return ret;
    }
    return connect();
}

int ESP32Interface::connect()
{
    if (_ap_ssid[0] == 0) {
        return NSAPI_ERROR_NO_SSID;
    }

    if (!_esp->dhcp(_dhcp, 1)) {
        return NSAPI_ERROR_DHCP_FAILURE;
    }

    if (!_dhcp) {
        if (!_esp->set_network(_ip_address, _netmask, _gateway)) {
            return NSAPI_ERROR_DEVICE_ERROR;
        }
    }

    set_connection_status(NSAPI_STATUS_CONNECTING);
    if (!_esp->connect(_ap_ssid, _ap_pass)) {
        set_connection_status(NSAPI_STATUS_DISCONNECTED);
        return NSAPI_ERROR_NO_CONNECTION;
    }

    return NSAPI_ERROR_OK;
}

int ESP32Interface::set_credentials(const char *ssid, const char *pass, nsapi_security_t security)
{
    int ret = NSAPI_ERROR_OK;
    size_t pass_len;

    if ((ssid == NULL) || (ssid[0] == 0)) {
        return NSAPI_ERROR_PARAMETER;
    }

    if ((pass == NULL) || (pass[0] == 0)) {
        pass_len = 0;
    } else {
        pass_len = strlen(pass);
    }

    switch (security) {
        case NSAPI_SECURITY_NONE:
            if (pass_len != 0) {
                ret = NSAPI_ERROR_PARAMETER;
            }
            break;
        case NSAPI_SECURITY_WEP:
            if ((pass_len < 5) || (pass_len > 26)) {
                ret = NSAPI_ERROR_PARAMETER;
            }
            break;
        case NSAPI_SECURITY_WPA:
        case NSAPI_SECURITY_WPA2:
        case NSAPI_SECURITY_WPA_WPA2:
            if ((pass_len < 8) || (pass_len > 63)) {
                ret = NSAPI_ERROR_PARAMETER;
            }
            break;
        case NSAPI_SECURITY_UNKNOWN:
            // do nothing
            break;
        default:
            ret = NSAPI_ERROR_UNSUPPORTED;
            break;
    }

    if (ret != NSAPI_ERROR_OK) {
        return ret;
    }

    memset(_ap_ssid, 0, sizeof(_ap_ssid));
    strncpy(_ap_ssid, ssid, sizeof(_ap_ssid));

    memset(_ap_pass, 0, sizeof(_ap_pass));
    if (pass_len != 0) {
        strncpy(_ap_pass, pass, pass_len);
    }

    _ap_sec = security;

    return NSAPI_ERROR_OK;
}

int ESP32Interface::set_channel(uint8_t channel)
{
    return NSAPI_ERROR_UNSUPPORTED;
}

int ESP32Interface::disconnect()
{
    if (_connection_status == NSAPI_STATUS_DISCONNECTED) {
        return NSAPI_ERROR_NO_CONNECTION;
    }

    if (!_esp->disconnect()) {
        return NSAPI_ERROR_DEVICE_ERROR;
    }

    return NSAPI_ERROR_OK;
}

const char *ESP32Interface::get_ip_address()
{
    return _esp->getIPAddress();
}

const char *ESP32Interface::get_mac_address()
{
    return _esp->getMACAddress();
}

const char *ESP32Interface::get_gateway()
{
    return _esp->getGateway();
}

const char *ESP32Interface::get_netmask()
{
    return _esp->getNetmask();
}

int8_t ESP32Interface::get_rssi()
{
    return _esp->getRSSI();
}

int ESP32Interface::scan(WiFiAccessPoint *res, unsigned count)
{
    return _esp->scan(res, count);
}

void ESP32Interface::attach(mbed::Callback<void(nsapi_event_t, intptr_t)> status_cb)
{
    _connection_status_cb = status_cb;
}

nsapi_connection_status_t ESP32Interface::get_connection_status() const
{
    return _connection_status;
}

void ESP32Interface::set_connection_status(nsapi_connection_status_t connection_status)
{
    if (_connection_status != connection_status) {
        _connection_status = connection_status;
        if (_connection_status_cb) {
            _connection_status_cb(NSAPI_EVENT_CONNECTION_STATUS_CHANGE, _connection_status);
        }
    }
}

void ESP32Interface::wifi_status_cb(int8_t wifi_status)
{
    switch (wifi_status) {
        case ESP32::STATUS_DISCONNECTED:
            set_connection_status(NSAPI_STATUS_DISCONNECTED);
            break;
        case ESP32::STATUS_GOT_IP:
            set_connection_status(NSAPI_STATUS_GLOBAL_UP);
            break;
        case ESP32::STATUS_CONNECTED:
        default:
            // do nothing
            break;
    }
}

#if MBED_CONF_ESP32_PROVIDE_DEFAULT

WiFiInterface *WiFiInterface::get_default_instance() {
    static ESP32Interface esp32;
    return &esp32;
}

#endif


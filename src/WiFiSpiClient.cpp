
/*
  WiFiSpiClient.cpp - Library for Arduino SPI connection to ESP8266
  Copyright (c) 2017 Jiri Bilek. All rights reserved.

  -----
  
  Based on WiFiClient.cpp - Library for Arduino Wifi shield.
  Copyright (c) 2011-2014 Arduino LLC.  All right reserved.

  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation; either
  version 2.1 of the License, or (at your option) any later version.

  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with this library; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

extern "C" {
  #include "utility/wl_definitions.h"
  #include "utility/wl_types.h"
  #include <string.h>
  #include "utility/debug.h"
}

#include "WiFiSpi.h"
#include "WiFiSpiClient.h"
#include "utility/wifi_spi.h"
#include "utility/srvspi_drv.h"


WiFiSpiClient::WiFiSpiClient() : _sock(SOCK_NOT_AVAIL) {
}

WiFiSpiClient::WiFiSpiClient(uint8_t sock) : _sock(sock) {
}

/*
 * 
 */
int WiFiSpiClient::connect(const char* host, uint16_t port)
{
	IPAddress remote_addr;
	if (WiFiSpi.hostByName(host, remote_addr))
	{
		return connect(remote_addr, port);
	}
	return 0;
}

/*
 * 
 */
int WiFiSpiClient::connect(IPAddress ip, uint16_t port)
{
    _sock = WiFiSpiClass::getSocket();
    if (_sock != SOCK_NOT_AVAIL)
    {
        if (! ServerSpiDrv::startClient(uint32_t(ip), port, _sock))
            return 0;   // unsuccessfull

        WiFiSpiClass::_state[_sock] = _sock;
    } 
    else 
    {
        Serial.println("No Socket available");
        return 0;
    }
    return 1;
}

/*
 * 
 */
size_t WiFiSpiClient::write(uint8_t b) {
	  return write(&b, 1);
}

/*
 * 
 */
size_t WiFiSpiClient::write(const uint8_t *buf, size_t size)
{
    if (_sock >= MAX_SOCK_NUM || size == 0)
    {
        setWriteError();
        return 0;
    }

    if (!ServerSpiDrv::sendData(_sock, buf, size))
    {
        setWriteError();
        return 0;
    }

    return size;
}

/*
 * 
 */
int WiFiSpiClient::available() 
{
    if (_sock == SOCK_NOT_AVAIL)
        return 0;
    if (availData == 0) {
      availData = ServerSpiDrv::availData(_sock);
    }
    return availData;
}

/*
 * 
 */
int WiFiSpiClient::read() 
{
    int16_t b;
    ServerSpiDrv::getData(_sock, &b);  // returns -1 when error
    if (availData > 0) {
      availData--;
    }
    return b;
}

/*
    Reads data into a buffer.
    Return: 0 = success, size bytes read
           -1 = error (either no data or communication error)
 */
int WiFiSpiClient::read(uint8_t* buf, size_t size) {
    // sizeof(size_t) is architecture dependent
    // but we need a 16 bit data type here
    uint16_t _size = size;
    if (_size > availData) {
      _size = availData; // patch, because firmware doesn't really send _size
    }
    if (!ServerSpiDrv::getDataBuf(_sock, buf, &_size))
        return -1;
    availData -= _size;
    if (availData < 0) {
      availData = 0;
    }
    return _size;
}

/*
 * 
 */
int WiFiSpiClient::peek() 
{
    int16_t b;
    ServerSpiDrv::getData(_sock, &b, 1);  // returns -1 when error
    return b;
}

/*
 * 
 */
void WiFiSpiClient::flush() {
  // TODO: a real check to ensure transmission has been completed
}

/*
 * 
 */
void WiFiSpiClient::stop() {
  if (_sock == SOCK_NOT_AVAIL)
    return;
    
  availData = 0;

  ServerSpiDrv::stopClient(_sock);

  int count = 0;
  // wait maximum 5 secs for the connection to close
  while (status() != CLOSED && ++count < 500)
    delay(10);

  WiFiSpiClass::_state[_sock] = NA_STATE;
  _sock = SOCK_NOT_AVAIL;
}

/*
 * 
 */
uint8_t WiFiSpiClient::connected()
{
    if (_sock == SOCK_NOT_AVAIL)
        return 0;
    else    
        return (availData > 0 || status() == ESTABLISHED);
}

/*
 * 
 */
uint8_t WiFiSpiClient::status() 
{
    if (_sock == SOCK_NOT_AVAIL)
        return CLOSED;
    else
        return ServerSpiDrv::getClientState(_sock);
}

/*
 * 
 */
WiFiSpiClient::operator bool() {
  return (_sock != SOCK_NOT_AVAIL);
}


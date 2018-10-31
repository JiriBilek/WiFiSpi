/*
  espspi_proxy.h - Library for Arduino SPI connection with ESP8266
  
  Copyright (c) 2017 Jiri Bilek. All right reserved.

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

  -------------

    WiFi SPI Safe Master for connecting with ESP8266
    On ESP8266 must be flashed WiFiSPIESP application
    Connect the SPI Master device to the following pins on the esp8266:

            ESP8266         |
    GPIO    NodeMCU   Name  |   Uno
   ===================================
     15       D8       SS   |   D10**
     13       D7      MOSI  |   D11
     12       D6      MISO  |   D12
     14       D5      SCK   |   D13

    **) User changeable

    Based on Hristo Gochkov's SPISlave library.
*/

#ifndef _ESPSPI_PROXY_H_INCLUDED
#define _ESPSPI_PROXY_H_INCLUDED

#include <SPI.h>
#include "debug.h"

// The command codes are fixed by ESP8266 hardware
#define CMD_WRITESTATUS  0x01
#define CMD_WRITEDATA    0x02
#define CMD_READDATA     0x03
#define CMD_READSTATUS   0x04

// Message indicators
#define MESSAGE_FINISHED     0xDF
#define MESSAGE_CONTINUES    0xDC

// SPI Status
enum {
    SPISLAVE_RX_BUSY,
    SPISLAVE_RX_READY
};
enum {
    SPISLAVE_TX_NODATA,
    SPISLAVE_TX_READY,
    SPISLAVE_TX_PREPARING_DATA
};

// How long we will wait for slave to be ready
#define SLAVE_RX_READY_TIMEOUT     3000UL
#define SLAVE_TX_READY_TIMEOUT     3000UL

// How long will be SS held high when starting transmission
#define SS_PULSE_DELAY_MICROSECONDS   50


class EspSpiProxy
{
private:
    SPIClass *spi_obj;

    uint8_t _ss_pin;
    uint8_t buffer[32];
    uint8_t buflen;
    uint8_t bufpos;
    
    void _pulseSS(boolean start)
    {
        if (_ss_pin >= 0)
        {
            if (start) {  // tested ok: 5, 15 / 5
                digitalWrite(_ss_pin, HIGH);
                delayMicroseconds(1);
                
                digitalWrite(_ss_pin, LOW);
                delayMicroseconds(15);  // 10us is low (some errors), 20 us is safe (no errors)
            }
            else {
                digitalWrite(_ss_pin, HIGH);
                delayMicroseconds(1);
                digitalWrite(_ss_pin, LOW);
            }
        }
    }
    
public:
    EspSpiProxy()
    {
       _ss_pin = -1;
       buflen = 0;
    }

    void begin(uint8_t pin, SPIClass *in_spi)
    {
        spi_obj = in_spi;

        _ss_pin = pin;
        pinMode(_ss_pin, OUTPUT);
        digitalWrite(_ss_pin, LOW);
    }

    uint32_t readStatus()
    {
        _pulseSS(true);

        spi_obj->transfer(CMD_READSTATUS);
        uint32_t status = (spi_obj->transfer(0) | ((uint32_t)(spi_obj->transfer(0)) << 8) | ((uint32_t)(spi_obj->transfer(0)) << 16) | ((uint32_t)(spi_obj->transfer(0)) << 24));
        
        _pulseSS(false);

        return status;
    }

    void writeStatus(uint32_t status)
    {
        _pulseSS(true);

        spi_obj->transfer(CMD_WRITESTATUS);
        spi_obj->transfer(status & 0xFF);
        spi_obj->transfer((status >> 8) & 0xFF);
        spi_obj->transfer((status >> 16) & 0xFF);
        spi_obj->transfer((status >> 24) & 0xFF);

        _pulseSS(false);
    }

    void readData(uint8_t* buf)
    {
        _pulseSS(true);

        spi_obj->transfer(CMD_READDATA);
        spi_obj->transfer(0x00);
        for(uint8_t i=0; i<32; i++) {
            buf[i] = spi_obj->transfer(0);  // the value is not important
        }

        _pulseSS(false);
    }

    void writeData(uint8_t * data, size_t len)
    {
        uint8_t i=0;
        _pulseSS(true);
        
        spi_obj->transfer(CMD_WRITEDATA);
        spi_obj->transfer(0x00);
        while(len-- && i < 32) {
            spi_obj->transfer(data[i++]);
        }
        while(i++ < 32) {
            spi_obj->transfer(0);
        }

        _pulseSS(false);
    }


    void flush(uint8_t indicator)
    {
        // Is buffer empty?
        if (buflen == 0)
            return;  

        // Message state indicator
        buffer[0] = indicator;
        
        // Wait for slave ready
        if (waitForSlaveRxReady() == SPISLAVE_RX_READY)  // TODO: move the waiting loop to writeByte
        {
            // Send the buffer
            writeData(buffer, buflen+1);
        }
            
        buflen = 0;
    }
    
    void writeByte(uint8_t b)
    {
        bufpos = 0;  // discard input data in the buffer
        
        if (buflen >= 31)
            flush(MESSAGE_CONTINUES);
            
        buffer[++buflen] = b;
    }

    uint8_t readByte()
    {
        buflen = 0;  // discard output data in the buffer
        
        if (bufpos >= 32)  // the buffer segment was read
        {
            if (buffer[0] != MESSAGE_CONTINUES)
              return 0;

            bufpos = 0;  // read next chunk
            
            // Wait for the slave
            waitForSlaveTxReady();
        }
        
        if (bufpos == 0)  // buffer empty
        {
            uint32_t endTime = millis() + 1000;

            do {
                readData(buffer);
            }  while (buffer[0] != MESSAGE_FINISHED && buffer[0] != MESSAGE_CONTINUES && millis() < endTime);
            
            if (buffer[0] != MESSAGE_FINISHED && buffer[0] != MESSAGE_CONTINUES)
              return 0;

            bufpos = 1;
        }
        return buffer[bufpos++];
    }

    /*
        Waits for slave receiver ready status.
        Return: status (SPISLAVE_RX_BUSY, SPISPLAVE_RX_READY or SPISLAVE_RX_PREPARING_DATA)
     */
    int8_t waitForSlaveRxReady()
    {
        uint32_t endTime = (millis() & 0x0fffffff) + SLAVE_RX_READY_TIMEOUT;
        uint32_t status = SPISLAVE_RX_BUSY;

        do
        {
            status = readStatus();

            if (((status >> 28) == SPISLAVE_RX_READY))
                return (status >> 28);  // status
            
            yield();
        } while ((millis() & 0x0fffffff) < endTime);

        WARN("Slave rx is not ready");
        WARN2("Returning: ", status >> 28);
        
        return (status >> 28);  // timeout
    }



    /*
        Waits for slave transmitter ready status.
        Return: status (SPISLAVE_TX_NODATA, SPISPLAVE_TX_READY
     */
    int8_t waitForSlaveTxReady()
    {
        uint32_t endTime = (millis() & 0x0fffffff) + SLAVE_RX_READY_TIMEOUT;
        uint32_t status = SPISLAVE_TX_NODATA;

        do
        {
            status = readStatus();

            if ((((status >> 24) & 0x0f) == SPISLAVE_TX_READY))
                return ((status >> 24) & 0x0f);  // status

            yield();
        } while ((millis() & 0x0fffffff) < endTime);

        WARN("Slave tx is not ready");
        WARN2("Returning: ", (status >> 24) & 0x0f);
        
        return ((status >> 24) & 0x0f);  // timeout
    }
    
};

extern EspSpiProxy espSpiProxy;

#endif


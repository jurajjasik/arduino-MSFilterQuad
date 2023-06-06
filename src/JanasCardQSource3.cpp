#include "JanasCardQSource3.h"
#include <stdio.h>

// #define TRACE_QSOURCE3(x) x
#define TRACE_QSOURCE3(x)

#ifdef TEST
#pragma message ("JanasCardQSource3 in test mode!")
#endif

void initCommJanasCardQSource3(uint32_t interrupt_priority)
{
    Serial2.begin(1500000);
    Serial2.setInterruptPriority(interrupt_priority);
    
    // See: https://forum.arduino.cc/t/arduino-due-rs485/434163/10
    // Serial2 => USART1:
    //// USART mode normal
    //// Master Clock MCK is selected
    //// Character length is 8 bits
    //// USART operates in Asynchronous Mode
    //// No parity
    //// 1 stop bit
    //// MSBF: Least Significant Bit is sent/received first.
    //// MODE9: CHRL defines character length.
    //// 
    USART1->US_WPMR = 0x55534100;  //Unlock the USART Mode register, just in case. (mine wasn't locked).
    
    // Default: USART1->US_MR = 0x8C0 = 
    USART1->US_MR |= (US_MR_USART_MODE_RS485 /*| US_MR_MSBF*/);  //Set mode to RS485
    USART1->US_TTGR = 16;  // Transmitter Timeguard - number of periods 
                          // after transmition and before turn off the RTS signal
    
    // Set 1500000 bauds/s - bug in Arduino lib
    USART1->US_MR |= US_MR_OVER;
    USART1->US_BRGR = US_BRGR_CD(7);
    
    // USART1 - RTS1 -> PA14 (Arduino pin 23)
    REG_PIOA_ABSR &= ~PIO_ABSR_P14;   // Ensure that peripheral pin is switched to peripheral A
    REG_PIOA_PDR |= PIO_PDR_P14;      // Disable the GPIO and switch to the peripheral
}

inline int32_t _limit(int32_t x, int32_t max, int32_t min)
{
    if (x > max) return max;
    if (x < min) return min;
    return x;
}


void JanasCardQSource3::_clearBuffer()
{
#ifndef TEST
    while(_comm->available() > 0) {
        _comm->read();
    }
#endif
}


bool JanasCardQSource3::_query(const char* query, char* buffer, size_t buff_len)
{
#ifndef TEST
    TRACE_QSOURCE3( printf("\r\n=> JanasCardQSource3::_query(\"%s\")\r\n", query); )
    
    _clearBuffer();
    _comm->print(query);
    _comm->print('\r');
    size_t n = _comm->readBytesUntil('\r', buffer, buff_len);
    if (n < buff_len) buffer[n] = '\0';  /* add terminal zero */
    
    TRACE_QSOURCE3(
        printf("_comm->readBytesUntil() -> %d, buffer = \"%s\"\r\n", n, buffer); 
    ) 
    
    if (n) return true;
    return false;
#else
    return true;
#endif
}


bool JanasCardQSource3::_queryOK(const char* query)
{
#ifndef TEST
    char buff[8];
    bool rc = _query(query, buff, 8);
    return (rc && (buff[0] == 'O') && (buff[1] == 'K'));
#else
    return true;
#endif
}


bool JanasCardQSource3::readTest(void)
{
    return _queryOK("#Q");
}


bool JanasCardQSource3::readSerialNo(char* buffer, size_t buff_len)
{
    return _query("#N", buffer, buff_len);
}


bool JanasCardQSource3::writeRSMode(uint32_t value)
{
    if (value) return _queryOK("#R1");
    return _queryOK("#R0");
}


bool JanasCardQSource3::writeDC(uint32_t output, int32_t value)
{
    value = _limit(value, Q_SOURCE3_MAX_DC, Q_SOURCE3_MIN_DC);
    char buff[64];
    if (output == 1)
    {
        snprintf(buff, 64, "#DC1 %d", value);
        if (_queryOK(buff))
        {
            return true;
        }
    }
    else if (output == 2)
    {
        snprintf(buff, 64, "#DC2 %d", value);
        if (_queryOK(buff))
        {
            return true;
        }
    }
    return false;
}


bool JanasCardQSource3::writeAC(uint32_t value)
{
    value = value > Q_SOURCE3_MAX_AC ? Q_SOURCE3_MAX_AC : 0;
    char buff[64];

    snprintf(buff, 64, "#AC %d", value);
    if (_queryOK(buff))
    {
        return true;
    }

    return false;
}


bool JanasCardQSource3::writeVoltages(int32_t dc1, int32_t dc2, uint32_t ac)
{
    dc1 = _limit(dc1, Q_SOURCE3_MAX_DC, Q_SOURCE3_MIN_DC);
    dc2 = _limit(dc2, Q_SOURCE3_MAX_DC, Q_SOURCE3_MIN_DC);
    ac = ac > Q_SOURCE3_MAX_AC ? Q_SOURCE3_MAX_AC : ac;

    char buff[128];

    snprintf(buff, 128, "#C %d %d %d", dc1, dc2, ac);
    if (_queryOK(buff))
    {
        return true;
    }

    return false;
}


bool JanasCardQSource3::writeFreqRange(uint32_t range)
{
    switch (range)
    {
    case 0:
        if (_queryOK("#B 0"))
        {
            return true;
        }
        break;
    case 1:
        if (_queryOK("#B 1"))
        {
            return true;
        }
        break;
    case 2:
        if (_queryOK("#B 2"))
        {
            return true;
        }
        break;
    }
    return false;
}


bool JanasCardQSource3::writeFreq(uint32_t value)
{
    value = _limit(value, Q_SOURCE3_MAX_FREQ, Q_SOURCE3_MIN_FREQ);
    char buff[64];

    snprintf(buff, 64, "#F %d", value);
    if (_queryOK(buff))
    {
        return true;
    }

    return false;
}


bool JanasCardQSource3::storeFreq(void)
{
    return _queryOK("#S");
}


int32_t JanasCardQSource3::readFreq(void)
{
#ifndef TEST
    char buff[64];

    if (_query("#G", buff, 64))
    {
        char* pEnd;
        return strtol(buff, &pEnd, 10);
    }

    return -1;
#else
    return 10000; // 1 MHz
#endif
}


int32_t JanasCardQSource3::readCurrent(void)
{
    char buff[64];

    if (_query("#U", buff, 64))
    {
        char* pEnd;
        return strtol(buff, &pEnd, 10);
    }

    return -1;
}


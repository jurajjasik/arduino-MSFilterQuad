#include "rtos_stream.h"
#include <task.h>

// #define TRACE_RTOS_STREAM(x_) printf("%d ms -> RTOS_Stream: ", millis()); x_
#define TRACE_RTOS_STREAM(x_)

RTOS_Stream::RTOS_Stream(USARTClass *usart, int timeout)
:_usart(usart)
{
    _timeout = pdMS_TO_TICKS(timeout);
}

bool RTOS_Stream::init()
{
    TRACE_RTOS_STREAM( printf("init() ... _xMessageBufferTx=%p, _xMessageBufferRx=%p\r\n", _xMessageBufferTx, _xMessageBufferRx); )
    if (_xMessageBufferTx == NULL)
    {
        TRACE_RTOS_STREAM( printf("... create _xMessageBufferTx...\r\n"); )
        _xMessageBufferTx = xMessageBufferCreate( TX_BUFFER_LENGTH );
        TRACE_RTOS_STREAM( printf("... _xMessageBufferTx=%p\r\n", _xMessageBufferTx); )
    if (_xMessageBufferTx == NULL) return false;
    }

    if (_xMessageBufferRx == NULL)
    {
        TRACE_RTOS_STREAM( printf("... create _xMessageBufferRx...\r\n"); )
        _xMessageBufferRx = xMessageBufferCreate( RX_BUFFER_LENGTH );
        TRACE_RTOS_STREAM( printf("... _xMessageBufferRx=%p\r\n", _xMessageBufferRx); )
        if (_xMessageBufferRx == NULL) return false;
    }

    return true;
}

size_t RTOS_Stream::write(const char* str)
{
    TRACE_RTOS_STREAM( printf("write(\"%s\")\r\n", str); )
    if (str == NULL) return 0;

    TRACE_RTOS_STREAM( printf("... _xMessageBufferTx=%p\r\n", _xMessageBufferTx); )
    if (_xMessageBufferTx == NULL) return 0;

    size_t xBytesSent;
    if( xPortIsInsideInterrupt() )
    {
        BaseType_t xHigherPriorityTaskWoken = pdFALSE; /* Initialised to pdFALSE. */

        /* Attempt to send the string to the message buffer. */
        xBytesSent = xMessageBufferSendFromISR( _xMessageBufferTx,
                                                ( void * ) str,
                                                strlen( str ),
                                                &xHigherPriorityTaskWoken );

        TRACE_RTOS_STREAM( printf("... %u bytes sent from interrupt.\r\n", xBytesSent); )

        portYIELD_FROM_ISR( xHigherPriorityTaskWoken );
        return xBytesSent;
    }

    xBytesSent = xMessageBufferSend( _xMessageBufferTx,
                               ( void * ) str,
                               strlen( str ),
                               _timeout );
    TRACE_RTOS_STREAM( printf("... %u bytes sent.\r\n", xBytesSent); )
    return xBytesSent;
}

int RTOS_Stream::available()
{
    if (_xMessageBufferRx == NULL) return false;
    return xMessageBufferIsEmpty( _xMessageBufferRx ) == pdFALSE;
}

int RTOS_Stream::read()
{
    if (_xMessageBufferRx == NULL) return 0;

    uint8_t ucRxData[ RX_BUFFER_LENGTH ];
    size_t xReceivedBytes;

    if( xPortIsInsideInterrupt() )
    {
        BaseType_t xHigherPriorityTaskWoken = pdFALSE;
        xReceivedBytes = xMessageBufferReceiveFromISR( _xMessageBufferRx,
                                                       ( void * ) ucRxData,
                                                       sizeof( ucRxData ),
                                                       &xHigherPriorityTaskWoken );
        portYIELD_FROM_ISR( xHigherPriorityTaskWoken );
        return xReceivedBytes;
    }

    return xMessageBufferReceive( _xMessageBufferRx,
                                            ( void * ) ucRxData,
                                            sizeof( ucRxData ),
                                            _timeout );
}

size_t RTOS_Stream::readBytesUntil( char terminator, char *buffer, size_t length)
{
    TRACE_RTOS_STREAM( printf("readBytesUntil(terminator=0x%02x, buffer=%p, length=%u)\r\n", reinterpret_cast<uint32_t*>(terminator), static_cast<void*>(buffer), length); )

    TRACE_RTOS_STREAM( printf("... _xMessageBufferRx=%p.\r\n", _xMessageBufferRx); )
    if (_xMessageBufferRx == NULL) return 0;

    uint8_t ucRxData[ RX_BUFFER_LENGTH ];
    size_t xReceivedBytes;
    size_t idx;
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    if( xPortIsInsideInterrupt() )
    {

        xReceivedBytes = xMessageBufferReceiveFromISR( _xMessageBufferRx,
                                                       ( void * ) ucRxData,
                                                       sizeof( ucRxData ),
                                                       &xHigherPriorityTaskWoken );
        TRACE_RTOS_STREAM( printf("... %u bytes received from interrupt.\r\n", xReceivedBytes); )
    }
    else
    {
        xReceivedBytes = xMessageBufferReceive( _xMessageBufferRx,
                                                ( void * ) ucRxData,
                                                sizeof( ucRxData ),
                                                _timeout );
        TRACE_RTOS_STREAM( printf("... %u bytes received.\r\n", xReceivedBytes); )
    }

    for(idx = 0; (idx < length) && (idx < xReceivedBytes) && (idx < RX_BUFFER_LENGTH); ++idx)
    {
        if( terminator == ucRxData[idx] ) break;
        buffer[idx] = ucRxData[idx];
    }

    if( xPortIsInsideInterrupt() ) portYIELD_FROM_ISR( xHigherPriorityTaskWoken );

    TRACE_RTOS_STREAM( printf("... %u bytes copied into the buffer.\r\n", idx); )
    return idx;
}

void RTOS_Stream::workTx(const TickType_t xTicksToWaitBufferReceive)
{
    if (_xMessageBufferTx == NULL) return;

    uint8_t ucRxData[ RX_BUFFER_LENGTH ];
    size_t xReceivedBytes;

    TRACE_RTOS_STREAM( printf("RTOS_Stream::workTx() waiting for message ...\r\n"); )
    // wait indefinitely (without timing out), provided INCLUDE_vTaskSuspend is set to 1
    xReceivedBytes = xMessageBufferReceive( _xMessageBufferTx,
                                            ( void * ) ucRxData,
                                            sizeof( ucRxData ),
                                            xTicksToWaitBufferReceive  );
    TRACE_RTOS_STREAM( printf("RTOS_Stream::workTx() ... %u bytes received. Writing to usart: \r\n", xReceivedBytes); )
    for(size_t i = 0; i < xReceivedBytes; ++i)
    {
        // TRACE_RTOS_STREAM( printf("%c\r\n", ucRxData[i]); )
        _usart->write(ucRxData[i]);
    }
    TRACE_RTOS_STREAM( printf("RTOS_Stream::workTx() Writing to usart done\r\n"); )
}

void RTOS_Stream::workRx(char terminator, const TickType_t xTicksToWaitStream, const TickType_t xTicksToWaitBufferSend)
{
    TRACE_RTOS_STREAM( printf("RTOS_Stream::workRx() waiting for usart read ...\r\n"); )
    
    while(!_usart->available())
    {
        vTaskDelay(xTicksToWaitStream);
        // TRACE_RTOS_STREAM( printf("RTOS_Stream::workRx() waiting for usart read ...\r\n"); )
    }
    
    TRACE_RTOS_STREAM( printf("RTOS_Stream::workRx() ... usart data available\r\n"); )
    
    bool isTerminator = false;
    while((_usart->available()) && (_rxIdx < RX_BUFFER_LENGTH))
    {
        char c = _usart->read();
        _rxBuffer[_rxIdx++] = c;
        if(c == terminator)
        {
            isTerminator = true;
            break;
        }
    }

    TRACE_RTOS_STREAM( printf("RTOS_Stream::workRx() ... %u bytes read from usart, isTerminator=%d\r\n", _rxIdx, isTerminator); )

    if(isTerminator || (_rxIdx == RX_BUFFER_LENGTH))
    {
        if (_xMessageBufferRx != NULL)
        {
            TRACE_RTOS_STREAM( printf("RTOS_Stream::workRx() sending to _xMessageBufferRx ...\r\n"); )
            xMessageBufferSend(
                _xMessageBufferRx,
                ( void * ) _rxBuffer,
                _rxIdx,
                xTicksToWaitBufferSend
            );
            TRACE_RTOS_STREAM( printf("RTOS_Stream::workRx() sending to _xMessageBufferRx ... done\r\n"); )
        }
        _rxIdx = 0;
    }
}

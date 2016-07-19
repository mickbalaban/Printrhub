//
// Created by Phillip Schuster on 09.07.16.
//

#include "CommStack.h"
#include "Application.h"

CommStack::CommStack(Stream* port, CommStackDelegate* delegate):
_port(port),
_delegate(delegate),
_expectedPacketType(Header),
_receiveBufferIndex(0),
_packetMarker(COMM_STACK_PACKET_MARKER)
{
    pinMode(3,OUTPUT);
    digitalWrite(3,HIGH);

    pinMode(33,OUTPUT);
    digitalWrite(33,HIGH);
}

CommStack::~CommStack()
{ }

bool CommStack::readHeader(CommHeader* commHeader)
{
    int numBytes = _port->readBytes((uint8_t*)commHeader,sizeof(CommHeader));
    if (numBytes < sizeof(CommHeader))
    {
        return false;
    }

    return true;
}

bool CommStack::prepareResponse(CommHeader *commHeader)
{
    //Flip Request to Response, if this is a Response switch to next task. If all tasks
    //have been processed just return true and do nothing
    if (commHeader->commType == Request)
    {
        commHeader->commType = Response;
    }
    else
    {
        commHeader->commType = Request;
        commHeader->currentTaskIndex++;

        if (commHeader->isFinished())
        {
            return false;
        }
    }

    return true;
}

bool CommStack::writeHeader(CommHeader* commHeader)
{
    int numBytes = _port->write((uint8_t*)commHeader,sizeof(CommHeader));
    if (numBytes < sizeof(CommHeader))
    {
        return false;
    }

    return true;
}
/*
 * Taken from PacketSerial COBS encoding (https://github.com/bakercp/PacketSerial/blob/master/src/Encoding/COBS.h)
 */
size_t CommStack::encode(const uint8_t* source, size_t size, uint8_t* destination)
{
    size_t read_index  = 0;
    size_t write_index = 1;
    size_t code_index  = 0;
    uint8_t code       = 1;

    while(read_index < size)
    {
        if(source[read_index] == 0)
        {
            destination[code_index] = code;
            code = 1;
            code_index = write_index++;
            read_index++;
        }
        else
        {
            destination[write_index++] = source[read_index++];
            code++;

            if(code == 0xFF)
            {
                destination[code_index] = code;
                code = 1;
                code_index = write_index++;
            }
        }
    }

    destination[code_index] = code;

    return write_index;
}

/*
 * Taken from PacketSerial COBS encoding (https://github.com/bakercp/PacketSerial/blob/master/src/Encoding/COBS.h)
 */
size_t CommStack::decode(const uint8_t* source, size_t size, uint8_t* destination)
{
    size_t read_index  = 0;
    size_t write_index = 0;
    uint8_t code;
    uint8_t i;

    while(read_index < size)
    {
        code = source[read_index];

        if(read_index + code > size && code != 1)
        {
            return 0;
        }

        read_index++;

        for(i = 1; i < code; i++)
        {
            destination[write_index++] = source[read_index++];
        }

        if(code != 0xFF && read_index != size)
        {
            destination[write_index++] = '\0';
        }
    }

    return write_index;
}

/*
 * Taken from PacketSerial COBS encoding (https://github.com/bakercp/PacketSerial/blob/master/src/Encoding/COBS.h)
 */
size_t CommStack::getEncodedBufferSize(size_t sourceSize)
{
    return sourceSize + sourceSize / 254 + 1;
}

void CommStack::send(const uint8_t* buffer, size_t size)
{
    if(_port == 0 || buffer == 0 || size == 0) return;

    uint8_t _encodeBuffer[getEncodedBufferSize(size)];
    size_t numEncoded = encode(buffer, size, _encodeBuffer);

    LOG("Sending encoded data");
    _port->write(_encodeBuffer, numEncoded);
    _port->write(_packetMarker);
}

void CommStack::runTask(const uint8_t* buffer, size_t size)
{
    LOG("Running task and preparing response buffer");
    //Clear response data buffer
    memset(_responseBuffer,0,COMM_STACK_BUFFER_SIZE);

    //Trigger the application to run the task and send responded data
    uint16_t responseDataSize = 0;
    bool sendResponse = true;
    _delegate->runTask(_currentHeader,buffer,size,_responseBuffer,&responseDataSize,&sendResponse);

    LOG_VALUE("Running task complete, Response data size",responseDataSize);
    //Prepare header for the response
    if (sendResponse && prepareResponse(&_currentHeader))
    {
        if (_currentHeader.commType == Request)
        {
            LOG_VALUE("Sending Request",_currentHeader.getCurrentTask());
        }
        else
        {
            LOG_VALUE("Sending Response",_currentHeader.getCurrentTask());
        }

        //Set size of responded data
        _currentHeader.contentLength = responseDataSize;

        //Send the header
        send((uint8_t*)&_currentHeader, sizeof(CommHeader));

        //Now send back data if they are available
        if (responseDataSize > 0)
        {
            LOG_VALUE("Sending response data with size",responseDataSize);
            send(_responseBuffer,responseDataSize);
        }
    }
    else
    {
        LOG("Task sequence complete");
    }
}

void CommStack::packetReceived(const uint8_t* buffer, size_t size)
{
    LOG_VALUE("Packet received with size",size);

    if (_expectedPacketType == Header)
    {
        if (size == sizeof(CommHeader))
        {
            LOG("Received header");
            //Copy data into current header
            memcpy(&_currentHeader,buffer,size);

            LOG_VALUE("Header.numberOfTasks",_currentHeader.numberOfTasks);
            LOG_VALUE("Header.currentTaskIndex",_currentHeader.currentTaskIndex);
            LOG_VALUE("Header.currentTask",_currentHeader.getCurrentTask());
            LOG_VALUE("Header.commType",_currentHeader.commType);
            LOG_VALUE("Header.contentLength",_currentHeader.contentLength);

            //Check if data are expected to be sent along the header
            if (_currentHeader.contentLength > 0)
            {
                LOG_VALUE("Now expecting data with length",_currentHeader.contentLength);
                _expectedPacketType = Data;
            }
            else
            {
                LOG("No data expected, expecting next package to be a header");
                //No data will follow this header, we expect an header again next time
                _expectedPacketType = Header;

                //Run the task as no data will follow
                LOG("Run task with data less header");
                runTask(buffer,size);
            }
        }
        else
        {
            LOG("Expected header, but received packet with different size");
            LOG_VALUE("Expected header size",sizeof(CommHeader));
            LOG_VALUE("Got packet with size",size);

            digitalWrite(3,LOW);
            delayMicroseconds(1);
            digitalWrite(3,HIGH);
            delayMicroseconds(1);
            digitalWrite(3,LOW);
            delayMicroseconds(1);
            digitalWrite(3,HIGH);
        }
    }
    else if (_expectedPacketType == Data)
    {
        if (size == _currentHeader.contentLength)
        {
            LOG("Received data, running task with data");
            //Everythings fine, we received an header and data with the expected size followed along, run the task
            runTask(buffer,size);
        }
        else
        {
            LOG("Expected data, but received packet with different size than specified in previous header");
            LOG_VALUE("Expected data size given in header",_currentHeader.contentLength);
            LOG_VALUE("Got packet with size",size);

            digitalWrite(3,LOW);
            delayMicroseconds(2);
            digitalWrite(3,HIGH);
        }

        //Now that the data have been sent/received the next package should be a header
        LOG("Next packet is expected to be a header");
        _expectedPacketType = Header;
    }
}

void CommStack::process()
{
    while (_port->available() > 0)
    {
        int data = _port->read();
        if (data >= 0)
        {
            if (data == COMM_STACK_PACKET_MARKER)
            {
                digitalWrite(33,LOW);
                LOG_VALUE("Packet received, decoding number of bytes",_receiveBufferIndex);
                uint8_t _decodeBuffer[_receiveBufferIndex];
                size_t numDecoded = decode(_receiveBuffer, _receiveBufferIndex, _decodeBuffer);
                _receiveBufferIndex = 0;

                LOG_VALUE("Handling decoded packet with size",numDecoded);
                //Packet received
                packetReceived(_decodeBuffer,numDecoded);
                digitalWrite(33,HIGH);
            }
            else
            {
                if ((_receiveBufferIndex + 1) < COMM_STACK_BUFFER_SIZE)
                {
                    _receiveBuffer[_receiveBufferIndex++] = data;
                }
                else
                {
                    // Error, buffer overflow if we write.
                    LOG("OVERFLOW, OVERFLOW!! No more space left to store the package!");
                }
            }
        }
        else
        {
            LOG("Read from Serial port failed, trying again later");
            //Break and come back later
            break;
        }
    }
}

bool CommStack::sendMessage(CommHeader &header, size_t contentLength, const uint8_t *data)
{
    if (contentLength > 0)
    {
        //Send the header and data
        LOG_VALUE("Sending Header and data with size",contentLength);
        send((uint8_t*)&header, sizeof(CommHeader));
        send(data, contentLength);
    }
    else
    {
        //Only send the header
        LOG("Sending the header only (no data)");
        send((uint8_t*)&header, sizeof(CommHeader));
    }

    LOG("Request sent");
    return true;
}

bool CommStack::requestTask(TaskID task, size_t contentLength, const uint8_t *data)
{
    LOG_VALUE("Request Task without data with ID",task);
    CommHeader header(task,contentLength);

    return sendMessage(header,contentLength,data);
}

bool CommStack::requestTask(TaskID task)
{
    LOG_VALUE("Request Task without data with ID",task);
    CommHeader header(task, 0);

    return sendMessage(header);
}

bool CommStack::responseTask(TaskID task, size_t contentLength, const uint8_t *data)
{
    LOG_VALUE("Response Task with data with ID",task);
    CommHeader header(task,contentLength);

    //Set as response
    header.commType = Response;

    return sendMessage(header,contentLength,data);
}

bool CommStack::responseTask(TaskID task)
{
    LOG_VALUE("Response Task without data with ID",task);
    CommHeader header(task, 0);

    //Set as response
    header.commType = Response;

    return sendMessage(header);
}
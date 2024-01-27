/*=====================================================================================================

Packet structure:
-------------------------------------------------------------------------------------------------------
Byte  |  Value
-------------------------------------------------------------------------------------------------------
0     |  uint8_t packet_type
      |   1 - Command
      |   2 - Response
      |   3 - REQ_ACK
      |   4 - REPLY_ACK
-------------------------------------------------------------------------------------------------------
1     |  uint32_t packet_id
2     |   The value is incremented sequentially by the master when sending the next command packet.
3     |   The same value is set when generating packages of type Response, REQ_ACK, REPLY_ACK.
4     |   
-------------------------------------------------------------------------------------------------------
5     |  For Command packet type:
      }  uint8_t command_type
      |   0 - read register
      |   1 - write register
      |
      }  For Response packet type:
      |  uint8_t response_status
      |   0 - fault
      |   1 - ok
      |
      |  For other packet types this field is not present.
-------------------------------------------------------------------------------------------------------
6     |  uint32_t reg_address
7     |   For Command and Response packet types only.
8     |   For other packet types this field is not present.
9     |   
-------------------------------------------------------------------------------------------------------
10    |  uint32_t reg_value
11    |   For Command and Response packet types only.
12    |   For other packet types this field is not present.
13    |
-------------------------------------------------------------------------------------------------------

=====================================================================================================*/

#define PACKET_MAX_SIZE               14

#define PACKET_TYPE_COMMAND           1
#define PACKET_TYPE_RESPONSE          2
#define PACKET_TYPE_REQ_ACK           3
#define PACKET_TYPE_REPLY_ACK         4

#define COMMAND_TYPE_READ_REGISTER    0
#define COMMAND_TYPE_WRITE_REGISTER   1

#define RESPONSE_FAULT                0
#define RESPONSE_OK                   1

#define SEND_RESPONSE_ATTEMPTS        3

#define TIMEOUT_SEND_COMMAND_MS       1000
#define TIMEOUT_SEND_REPLY_ACK_MS     1000
#define TIMEOUT_RECEIVE_COMMAND_MS    portMAX_DELAY
#define TIMEOUT_RECEIVE_REPLY_ACK_MS  100

typedef struct
{
  uint32_t packet_id;
  uint8_t  command_type;
  uint32_t reg_address;
  uint32_t reg_value;
} commandData_t;

typedef struct
{
  uint32_t packet_id;
  uint8_t  response_status;
  uint32_t reg_address;
  uint32_t reg_value;
} responseData_t;

typedef struct
{
  uint32_t packet_id;
} ackData_t;

QueueHandle_t commandDataQueue;
QueueHandle_t replyAckQueue;

// use round buffer for storing command and respons packets. I do not provide the specific implementation of the methods here.
RoundBuf<commandData_t, 10> commandDataRingBuf;
RoundBuf<responseData_t, 10> responseDataRingBuf;

//
// Task that reads data via UDP, parses the data and puts data structures in the appropriate queues.
//
void updInputDataParseTask(void)
{
  commandData_t commandData;
  ackData_t replyAckData;
  uint8_t packet[PACKET_MAX_SIZE];

  commandDataQueue = xQueueCreate(10, sizeof(commandData_t));
  replyAckQueue = xQueueCreate(1, sizeof(ackData_t));

  while(1)
  {
    if(FreeRTOS_recvfrom(...) == true)
    {
      if(getPacketFromUdp() == true) // here we got valid command or reply_ack packet
      {
        if(packet[0] == PACKET_TYPE_COMMAND)
        {
          packetToCommandData();
          if(xQueueSend(commandDataQueue, &commandData, pdMS_TO_TICKS(TIMEOUT_SEND_COMMAND_MS)) != pdPASS) 
          {
            // Make some signal for fail to adding message to queue
            continue;
          }
        }
        else if(packet[0] == PACKET_TYPE_REPLY_ACK)
        {
          packetToReplyAck();
          if(xQueueSend(replyAckQueue, &replyAckData, pdMS_TO_TICKS(TIMEOUT_SEND_REPLY_ACK_MS)) != pdPASS) 
          {
            // Make some signal for fail to adding message to queue
            continue;
          }
        }
      }
    }
  }
}

//
// Task that reads a command from the queue, executes it, generates a response and transmits the corresponding ACKs.
//
void commandsProcessTask(void)
{
  commandData_t commandData;
  responseData_t responseData;
  ackData_t reqAckData;
  ackData_t replyAckData;

  while(1)
  {
    // get command data structure from the queue
    if(xQueueReceive(commandDataQueue, &commandData, TIMEOUT_RECEIVE_COMMAND_MS) == pdPASS)
    {
      commandDataRingBuf.put(&commandData); // store for history

      // make REQ_ACK
      reqAckData.packet_id = commandData.packet_id;
      udpSendBlocked(PACKET_TYPE_REQ_ACK, (uint8_t *)(&reqAckData), sizeof(reqAckData) );

      // make response
      responseData.packet_id = commandData.packet_id;
      responseData.reg_address = commandData.reg_address;

      if(commandData.command_type == COMMAND_TYPE_READ_REGISTER)
      {
        responseData.response_status = reg_read(commandData.reg_address, &responseData.reg_value);
      }
      else if(commandData.command_type == COMMAND_TYPE_WRITE_REGISTER)
      {
        responseData.reg_value = commandData.reg_value;
        responseData.response_status = reg_write(commandData.reg_address, commandData.reg_value);
      }
      else
      {
        responseData.response_status = RESPONSE_FAULT;
      }

      // try to send response and wait for REPLY_ACK
      xQueueReset(replyAckQueue);
      uint8_t attempt;
      for(attempt = 0; attempt < SEND_RESPONSE_ATTEMPTS; attempt++)  
      {
        responseDataRingBuf.put(&responseData); // store for history

        udpSendBlocked(PACKET_TYPE_RESPONSE, (uint8_t *)(&responseData), sizeof(responseData) );

        if(xQueueReceive(replyAckQueue, &replyAckData, pdMS_TO_TICKS(TIMEOUT_RECEIVE_REPLY_ACK_MS)) == pdPASS)
        {
          if(replyAckData.packet_id == commandData.packet_id) // all packet transfer is ok
            break;
        }
      }

      if(attempt == SEND_RESPONSE_ATTEMPTS)
      {
        // There is no REPLY_ACK from master.
        // Make some signal.
      }
    }
  }
}

//
//
//
void udpSendBlocked(uint8_t packet_type, uint8_t data, uint32_t size)
{
  uint8_t packet[PACKET_MAX_SIZE];
  uint8_t packet_size;

  makePacket(data, size, packet, &packet_size); // make packet from the data struct

  xMutexAcquire(udpSendMutex, portMAX_DELAY); // lock
  FreeRTOS_sendto(packet, packet_size);
  xMutexRelease(udpSendMutex); // unlock
}
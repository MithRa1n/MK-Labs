#include "CommunicationService.h"
#include <thread>

CommunicationService::CommunicationService(SoftwareSerial& serial, uint32_t baudRate)
    : communicationSerial(serial), baudRate(baudRate)
{
   

}

void CommunicationService::init()
{
    communicationSerial.begin(baudRate, SWSERIAL_8E2);
}

void CommunicationService::send(ToogleCommand command)
{
    communicationSerial.write((uint8_t)command);
    Serial.print("Sent data: ");
    Serial.println((uint8_t)command);
}

void CommunicationService::onReceive(CommandDelegate commandDelegate)
{
    if (communicationSerial.available())
    {
        int receivedData = communicationSerial.read(); 
        Serial.print("Received data (HEX): ");
        Serial.println(receivedData, HEX);

        if (receivedData == (uint8_t)ToogleCommand::ON ||  
            receivedData == (uint8_t)ToogleCommand::OFF ||  
            receivedData == (uint8_t)ToogleCommand::STOP) {
            commandDelegate((ToogleCommand)receivedData);
        } else {
            Serial.println("Received unknown data!");
        }
    }
}


CommunicationService::~CommunicationService()
{
    communicationSerial.end();
}
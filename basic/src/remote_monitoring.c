/*
 * IoT Gateway BLE Script - Microsoft Sample Code - Copyright (c) 2016 - Licensed MIT
 */

#include <stdlib.h>
#include "remote_monitoring.h"
#include "azure_c_shared_utility/threadapi.h"
#include "azure_c_shared_utility/xlogging.h"
#include "azure_c_shared_utility/crt_abstractions.h"
#include "messageproperties.h"
#include "message.h"
#include "module.h"
#include "broker.h"

#include <parson.h>

static char deviceInfo[] = "{\
  \"ObjectType\": \"DeviceInfo\",\
  \"Version\": \"1.0\",\
  \"DeviceProperties\": {\
    \"HubEnabledState\": true\
  },\
\"Commands\": [{\
  \"Name\": \"TelemetrySwitch\",\
  \"Parameters\" : [{\
	\"Name\": \"TelemetryStatus\",\
	\"Type\" : \"int\"\
   }]\
  }],\
  \"Telemetry\": [\
    {\
      \"Name\": \"Temperature\",\
      \"DisplayName\": \"Temperature\",\
      \"Type\": \"double\"\
    }\
  ]\
}";

static int messageCount = 0;
const char * temperature_uuid = "F000AA01-0451-4000-B000-000000000000";
BROKER_HANDLE myBroker;
void* RemoteMonitoring_ParseConfigurationFromJson(const char* configuration)
{
    (void)configuration;
    return NULL;
}

void RemoteMonitoring_FreeConfiguration(void * configuration)
{
    (void)configuration;
    return;
}

MODULE_HANDLE RemoteMonitoring_Create(BROKER_HANDLE broker, const void* configuration)
{
    myBroker = broker;
    (void)configuration;
    return (MODULE_HANDLE)0x42;
}

void RemoteMonitoring_Destroy(MODULE_HANDLE module)
{
    (void)module;
}

float getTemperature(const CONSTBUFFER * buffer)
{
    const float SCALE_LSB = 0.03125;
    if (buffer->size == 4)
    {
        uint16_t* temps = (uint16_t *)buffer->buffer;
        uint16_t rawAmbTemp = temps[0];
        int it = (int)((rawAmbTemp) >> 2);
        return (float)it * SCALE_LSB;
    }
    return 0;
}


void RemoteMonitoring_Receive(MODULE_HANDLE module, MESSAGE_HANDLE message)
{
    if (message != NULL)
    {
        CONSTMAP_HANDLE props = Message_GetProperties(message);
        if (props != NULL)
        {
            MAP_HANDLE newMap = Map_Create(NULL);
            const char* source = ConstMap_GetValue(props, GW_SOURCE_PROPERTY);
            const char * macAddr = ConstMap_GetValue(props, GW_MAC_ADDRESS_PROPERTY);
            if (source != NULL && strcmp(source, GW_SOURCE_BLE_TELEMETRY) == 0)
            {
                Map_Add(newMap, GW_SOURCE_PROPERTY, GW_SOURCE_BLE_TELEMETRY);
                Map_Add(newMap, GW_MAC_ADDRESS_PROPERTY, macAddr);
                const char* characteristic_uuid = ConstMap_GetValue(props, GW_CHARACTERISTIC_UUID_PROPERTY);
                const CONSTBUFFER* buffer = Message_GetContent(message);
                char content[256];
                float temperature;
                if (buffer != NULL && characteristic_uuid != NULL)
                {
                    if (g_ascii_strcasecmp(temperature_uuid, characteristic_uuid) == 0)
                    {
                        // it is the temperature data
                        temperature = getTemperature(buffer);
                    }
                }

				MESSAGE_CONFIG newMessageCfg = {0};
				if(messageCount++ != 0)
				{
					sprintf_s(content, sizeof(content), "{\"Temperature\": %f}", temperature);
					newMessageCfg.size = strlen(content);
					newMessageCfg.source = (const unsigned char*)content;
					printf("sending message: %s\r\n", content);
				}
				else
				{
					newMessageCfg.size = strlen(deviceInfo);
					newMessageCfg.source = (const unsigned char*)deviceInfo;
					printf("sending device info\r\n");
				}
                newMessageCfg.sourceProperties = newMap;
                MESSAGE_HANDLE newMessage = Message_Create(&newMessageCfg);
                if (newMessage != NULL)
                {
                    if(Broker_Publish(myBroker, module, newMessage) != BROKER_OK)
                    {
                        LogError("Failed to publish\r\n");                    
                    }
                    Message_Destroy(newMessage);
                }
                else
                {
                    LogError("Failed to create message\r\n");
                }
                
                Map_Destroy(newMap);
            }
        }
        else
        {
            LogError("Message_GetProperties for the message returned NULL");
        }
    }
    else
    {
        LogError("message is NULL");
    }
}

void RemoteMonitoring_Start(MODULE_HANDLE module)
{
    (void)module;
    return;
}

static const MODULE_API_1 Module_GetApi_Impl =
{
    {MODULE_API_VERSION_1},

    RemoteMonitoring_ParseConfigurationFromJson,
    RemoteMonitoring_FreeConfiguration,
    RemoteMonitoring_Create,
    RemoteMonitoring_Destroy,
    RemoteMonitoring_Receive,
    RemoteMonitoring_Start
};

MODULE_EXPORT const MODULE_API* Module_GetApi(MODULE_API_VERSION gateway_api_version)
{
    (void)gateway_api_version;
    return (const MODULE_API*)&Module_GetApi_Impl;
}

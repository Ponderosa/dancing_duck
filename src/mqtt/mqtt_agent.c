#include "FreeRTOS.h"
#include "core_mqtt.h"
#include "core_mqtt_agent.h"
#include "queue.h"
#include "task.h"

#ifndef SOCK_TARGET_HOST
#define SOCK_TARGET_HOST "192.168.5.6"
#endif

#ifndef SOCK_TARGET_PORT
#define SOCK_TARGET_PORT 1883
#endif

static void connectAndCreateTasks(void);
static void prvConnectToMQTTBroker(void);
static MQTTStatus_t prvMQTTInit(void);
static void prvMQTTAgentTask(void);

void vStartMQTTAgent(void) {
  /* prvConnectAndCreateDemoTasks() connects to the MQTT broker, creates the
   * tasks that will interact with the broker via the MQTT agent, then turns
   * itself into the MQTT agent task. */
  xTaskCreate(connectAndCreateTasks, "MQTT App Task", 1024, NULL, 1, NULL);
}

static void connectAndCreateTasks() {
  /* Create the TCP connection to the broker, then the MQTT connection to the
   * same. */
  prvConnectToMQTTBroker();

  /* This task has nothing left to do, so rather than create the MQTT
   * agent as a separate thread, it simply calls the function that implements
   * the agent - in effect turning itself into the agent. */
  prvMQTTAgentTask(NULL);
}

static void prvConnectSocket() {
  int sock;
  struct sockaddr_in server_addr;
  int flags, ret;

  // Create a socket
  sock = lwip_socket(AF_INET, SOCK_STREAM, 0);
  if (sock < 0) {
    printf("Failed to create socket\n");
    vTaskDelete(NULL);
    return;
  }

  // Set the socket to non-blocking mode
  flags = lwip_fcntl(sock, F_GETFL, 0);
  lwip_fcntl(sock, F_SETFL, flags | O_NONBLOCK);

  // Setup the server address
  server_addr.sin_family = AF_INET;
  server_addr.sin_port = htons(SOCK_TARGET_PORT);               // Example port
  server_addr.sin_addr.s_addr = inet_addr("SOCK_TARGET_HOST");  // Example IP

  // Connect to the server
  ret = lwip_connect(sock, (struct sockaddr*)&server_addr, sizeof(server_addr));
  if (ret < 0 && errno != EINPROGRESS) {
    printf("Failed to connect\n");
    lwip_close(sock);
    vTaskDelete(NULL);
    return;
  }

  // Here you can handle other tasks or poll the socket to see if it's ready
  // For simplicity, we'll just sleep and then check the socket
  vTaskDelay(pdMS_TO_TICKS(1000));

  // Check if the connection was successful
  fd_set write_fds;
  struct timeval timeout;

  FD_ZERO(&write_fds);
  FD_SET(sock, &write_fds);

  timeout.tv_sec = 0;
  timeout.tv_usec = 0;

  ret = lwip_select(sock + 1, NULL, &write_fds, NULL, &timeout);
  if (ret > 0 && FD_ISSET(sock, &write_fds)) {
    int so_error;
    socklen_t len = sizeof(so_error);

    getsockopt(sock, SOL_SOCKET, SO_ERROR, &so_error, &len);
    if (so_error == 0) {
      printf("Connected successfully\n");
      // Now you can use the socket to send/receive data
    } else {
      printf("Connection failed with error: %d\n", so_error);
    }
  } else {
    printf("Socket not ready or failed to connect\n");
  }

  // Close the socket
  lwip_close(sock);
  vTaskDelete(NULL);
}

static void prvConnectToMQTTBroker(void) {
  BaseType_t xNetworkStatus = pdFAIL;
  MQTTStatus_t xMQTTStatus;

  /* Connect a TCP socket to the broker. */
  xNetworkStatus = prvSocketConnect(&xNetworkContext);
  configASSERT(xNetworkStatus == pdPASS);

  /* Initialize the MQTT context with the buffer and transport interface. */
  xMQTTStatus = prvMQTTInit();
  configASSERT(xMQTTStatus == MQTTSuccess);

  /* Form an MQTT connection without a persistent session. */
  xMQTTStatus = prvMQTTConnect(true);
  configASSERT(xMQTTStatus == MQTTSuccess);
}

static void prvMQTTAgentTask() {
  for (;;) {
    vTaskDelay(1000);
    printf("MQTT Agent Spin");
  }
}

// static MQTTStatus_t prvMQTTInit(void) {
//   TransportInterface_t xTransport;
//   MQTTStatus_t xReturn;
//   MQTTFixedBuffer_t xFixedBuffer = {.pBuffer = xNetworkBuffer,
//                                     .size = MQTT_AGENT_NETWORK_BUFFER_SIZE};
//   static uint8_t staticQueueStorageArea[MQTT_AGENT_COMMAND_QUEUE_LENGTH *
//                                         sizeof(MQTTAgentCommand_t*)];
//   static StaticQueue_t staticQueueStructure;
//   MQTTAgentMessageInterface_t messageInterface = {
//       .pMsgCtx = NULL,
//       .send = Agent_MessageSend,
//       .recv = Agent_MessageReceive,
//       .getCommand = Agent_GetCommand,
//       .releaseCommand = Agent_ReleaseCommand};

//   LogDebug(("Creating command queue."));
//   xCommandQueue.queue = xQueueCreateStatic(
//       MQTT_AGENT_COMMAND_QUEUE_LENGTH, sizeof(MQTTAgentCommand_t*),
//       staticQueueStorageArea, &staticQueueStructure);
//   configASSERT(xCommandQueue.queue);
//   messageInterface.pMsgCtx = &xCommandQueue;

//   /* Initialize the task pool. */
//   Agent_InitializePool();

//   /* Fill in Transport Interface send and receive function pointers. */
//   xTransport.pNetworkContext = &xNetworkContext;
// #if defined(democonfigUSE_TLS) && (democonfigUSE_TLS == 1)
//   xTransport.send = TLS_FreeRTOS_send;
//   xTransport.recv = TLS_FreeRTOS_recv;
// #else
//   xTransport.send = Plaintext_FreeRTOS_send;
//   xTransport.recv = Plaintext_FreeRTOS_recv;
// #endif

//   /* Initialize MQTT library. */
//   xReturn =
//       MQTTAgent_Init(&xGlobalMqttAgentContext, &messageInterface,
//       &xFixedBuffer,
//                      &xTransport, prvGetTimeMs, prvIncomingPublishCallback,
//                      /* Context to pass into the callback. Passing the
//                      pointer
//                         to subscription array. */
//                      xGlobalSubscriptionList);

//   return xReturn;
// }

// static MQTTStatus_t prvMQTTConnect( bool xCleanSession )
// {
//     MQTTStatus_t xResult;
//     MQTTConnectInfo_t xConnectInfo;
//     bool xSessionPresent = false;

//     /* Many fields are not used in this demo so start with everything at 0.
//     */ memset( &xConnectInfo, 0x00, sizeof( xConnectInfo ) );

//     /* Start with a clean session i.e. direct the MQTT broker to discard any
//      * previous session data. Also, establishing a connection with clean
//      session
//      * will ensure that the broker does not store any data when this client
//      * gets disconnected. */
//     xConnectInfo.cleanSession = xCleanSession;

//     /* The client identifier is used to uniquely identify this MQTT client to
//      * the MQTT broker. In a production device the identifier can be
//      something
//      * unique, such as a device serial number. */
//     xConnectInfo.pClientIdentifier = democonfigCLIENT_IDENTIFIER;
//     xConnectInfo.clientIdentifierLength = ( uint16_t ) strlen(
//     democonfigCLIENT_IDENTIFIER );

//     /* Set MQTT keep-alive period. It is the responsibility of the
//     application
//      * to ensure that the interval between Control Packets being sent does
//      not
//      * exceed the Keep Alive value. In the absence of sending any other
//      Control
//      * Packets, the Client MUST send a PINGREQ Packet.  This responsibility
//      will
//      * be moved inside the agent. */
//     xConnectInfo.keepAliveSeconds = mqttexampleKEEP_ALIVE_INTERVAL_SECONDS;

//     /* Append metrics when connecting to the AWS IoT Core broker. */
//     #ifdef democonfigUSE_AWS_IOT_CORE_BROKER
//         #ifdef democonfigCLIENT_USERNAME
//             xConnectInfo.pUserName = CLIENT_USERNAME_WITH_METRICS;
//             xConnectInfo.userNameLength = ( uint16_t ) strlen(
//             CLIENT_USERNAME_WITH_METRICS ); xConnectInfo.pPassword =
//             democonfigCLIENT_PASSWORD; xConnectInfo.passwordLength = (
//             uint16_t ) strlen( democonfigCLIENT_PASSWORD );
//         #else
//             xConnectInfo.pUserName = AWS_IOT_METRICS_STRING;
//             xConnectInfo.userNameLength = AWS_IOT_METRICS_STRING_LENGTH;
//             /* Password for authentication is not used. */
//             xConnectInfo.pPassword = NULL;
//             xConnectInfo.passwordLength = 0U;
//         #endif
//     #else /* ifdef democonfigUSE_AWS_IOT_CORE_BROKER */
//         #ifdef democonfigCLIENT_USERNAME
//             xConnectInfo.pUserName = democonfigCLIENT_USERNAME;
//             xConnectInfo.userNameLength = ( uint16_t ) strlen(
//             democonfigCLIENT_USERNAME ); xConnectInfo.pPassword =
//             democonfigCLIENT_PASSWORD; xConnectInfo.passwordLength = (
//             uint16_t ) strlen( democonfigCLIENT_PASSWORD );
//         #endif /* ifdef democonfigCLIENT_USERNAME */
//     #endif /* ifdef democonfigUSE_AWS_IOT_CORE_BROKER */

//     /* Send MQTT CONNECT packet to broker. MQTT's Last Will and Testament
//     feature
//      * is not used in this demo, so it is passed as NULL. */
//     xResult = MQTT_Connect( &( xGlobalMqttAgentContext.mqttContext ),
//                             &xConnectInfo,
//                             NULL,
//                             mqttexampleCONNACK_RECV_TIMEOUT_MS,
//                             &xSessionPresent );

//     LogInfo( ( "Session present: %d\n", xSessionPresent ) );

//     /* Resume a session if desired. */
//     if( ( xResult == MQTTSuccess ) && ( xCleanSession == false ) )
//     {
//         xResult = MQTTAgent_ResumeSession( &xGlobalMqttAgentContext,
//         xSessionPresent );

//         /* Resubscribe to all the subscribed topics. */
//         if( ( xResult == MQTTSuccess ) && ( xSessionPresent == false ) )
//         {
//             xResult = prvHandleResubscribe();
//         }
//     }

//     return xResult;
// }

// static void prvConnectToMQTTBroker(void) {
//   BaseType_t xNetworkStatus = pdFAIL;
//   MQTTStatus_t xMQTTStatus;

//   /* Connect a TCP socket to the broker. */
//   xNetworkStatus = prvSocketConnect(&xNetworkContext);
//   configASSERT(xNetworkStatus == pdPASS);

//   /* Initialize the MQTT context with the buffer and transport interface. */
//   xMQTTStatus = prvMQTTInit();
//   configASSERT(xMQTTStatus == MQTTSuccess);

//   /* Form an MQTT connection without a persistent session. */
//   xMQTTStatus = prvMQTTConnect(true);
//   configASSERT(xMQTTStatus == MQTTSuccess);
// }

// static void prvMQTTAgentTask(void* pvParameters) {
//   BaseType_t xNetworkResult = pdFAIL;
//   MQTTStatus_t xMQTTStatus = MQTTSuccess, xConnectStatus = MQTTSuccess;
//   MQTTContext_t* pMqttContext = &(xGlobalMqttAgentContext.mqttContext);

//   (void)pvParameters;

//   do {
//     /* MQTTAgent_CommandLoop() is effectively the agent implementation.  It
//      * will manage the MQTT protocol until such time that an error occurs,
//      * which could be a disconnect.  If an error occurs the MQTT context on
//      * which the error happened is returned so there can be an attempt to
//      * clean up and reconnect however the application writer prefers. */
//     xMQTTStatus = MQTTAgent_CommandLoop(&xGlobalMqttAgentContext);

//     /* Success is returned for disconnect or termination. The socket should
//      * be disconnected. */
//     if (xMQTTStatus == MQTTSuccess) {
//       /* MQTT Disconnect. Disconnect the socket. */
//       xNetworkResult = prvSocketDisconnect(&xNetworkContext);
//     }
//     /* Error. */
//     else {
// #if (democonfigCREATE_CODE_SIGNING_OTA_DEMO == 1)
//       { vSuspendOTACodeSigningDemo(); }
// #endif

//       /* Reconnect TCP. */
//       xNetworkResult = prvSocketDisconnect(&xNetworkContext);
//       configASSERT(xNetworkResult == pdPASS);
//       xNetworkResult = prvSocketConnect(&xNetworkContext);
//       configASSERT(xNetworkResult == pdPASS);
//       pMqttContext->connectStatus = MQTTNotConnected;
//       /* MQTT Connect with a persistent session. */
//       xConnectStatus = prvMQTTConnect(false);
//       configASSERT(xConnectStatus == MQTTSuccess);

// #if (democonfigCREATE_CODE_SIGNING_OTA_DEMO == 1)
//       {
//         if (xMQTTStatus == MQTTSuccess) {
//           vResumeOTACodeSigningDemo();
//         }
//       }
// #endif
//     }
//   } while (xMQTTStatus != MQTTSuccess);
// }

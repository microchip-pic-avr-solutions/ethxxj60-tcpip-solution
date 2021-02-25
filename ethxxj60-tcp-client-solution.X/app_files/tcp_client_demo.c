/*
 *  (c) 2020 Microchip Technology Inc. and its subsidiaries.
 *
 *  Subject to your compliance with these terms,you may use this software and
 *  any derivatives exclusively with Microchip products.It is your responsibility
 *  to comply with third party license terms applicable to your use of third party
 *  software (including open source software) that may accompany Microchip software.
 *
 *  THIS SOFTWARE IS SUPPLIED BY MICROCHIP "AS IS". NO WARRANTIES, WHETHER
 *  EXPRESS, IMPLIED OR STATUTORY, APPLY TO THIS SOFTWARE, INCLUDING ANY IMPLIED
 *  WARRANTIES OF NON-INFRINGEMENT, MERCHANTABILITY, AND FITNESS FOR A
 *  PARTICULAR PURPOSE.
 *
 *  IN NO EVENT WILL MICROCHIP BE LIABLE FOR ANY INDIRECT, SPECIAL, PUNITIVE,
 *  INCIDENTAL OR CONSEQUENTIAL LOSS, DAMAGE, COST OR EXPENSE OF ANY KIND
 *  WHATSOEVER RELATED TO THE SOFTWARE, HOWEVER CAUSED, EVEN IF MICROCHIP HAS
 *  BEEN ADVISED OF THE POSSIBILITY OR THE DAMAGES ARE FORESEEABLE. TO THE
 *  FULLEST EXTENT ALLOWED BY LAW, MICROCHIP'S TOTAL LIABILITY ON ALL CLAIMS IN
 *  ANY WAY RELATED TO THIS SOFTWARE WILL NOT EXCEED THE AMOUNT OF FEES, IF ANY,
 *  THAT YOU HAVE PAID DIRECTLY TO MICROCHIP FOR THIS SOFTWARE.
 */
#include <stdio.h>
#include <string.h>
#include <time.h>
#include "../mcc_generated_files/mcc.h"
#include "../mcc_generated_files/TCPIPLibrary/tcpv4.h"
#include "../mcc_generated_files/TCPIPLibrary/ipv4.h"
#include "../mcc_generated_files/TCPIPLibrary/tcpip_config.h"
#include "../mcc_generated_files/TCPIPLibrary/tcpip_types.h"
#include "tcp_client_demo.h"

#define TX_BUFFER_SIZE (50)
#define RX_BUFFER_SIZE (50)

sockaddr_in4_t remoteSocket;

void TCP_Demo_Client(void)
{
    // create the socket for the TCP Client
    static tcpTCB_t port65534TCB;
    
    // create the TX and RX Client's buffers
    static uint8_t rxdataPort65534[RX_BUFFER_SIZE];
    static uint8_t txdataPort65534[TX_BUFFER_SIZE];
    static time_t t_client;
    static time_t socketTimeout;    
    uint16_t rx_len;
    socketState_t socketState;
    rx_len = 0;    
    
    
    socketState = TCP_SocketPoll(&port65534TCB);

    time(&t_client);

    switch(socketState)
    {
        case NOT_A_SOCKET:
           // Inserting and Initializing the socket
           TCP_SocketInit(&port65534TCB); 
           
            break;
        case SOCKET_CLOSED:
            // if the socket is closed we will try to connect again
            // try to connect once at 2 seconds
            socketTimeout = t_client + 2;
            TCP_InsertRxBuffer(&port65534TCB, rxdataPort65534, sizeof(rxdataPort65534));
            
            //Connect to the Server
            TCP_Connect(&port65534TCB, &remoteSocket);
          
            break;
        case SOCKET_IN_PROGRESS:
            // if the socket is closed we will try to connect again
            if(t_client >= socketTimeout)
            {
                TCP_Close(&port65534TCB);
            }
            break;
        case SOCKET_CONNECTED:
            // implement an echo client over TCP
            // check if the previous buffer was sent
            if (TCP_SendDone(&port65534TCB))
            {
                rx_len = TCP_GetReceivedData(&port65534TCB);
                // handle the incoming data
                if(rx_len > 0)
                {
                    // restricting data input length
                    if (rx_len > RX_BUFFER_SIZE) {
                        rxdataPort65534[RX_BUFFER_SIZE] = 0;
                    } else {
                        rxdataPort65534[rx_len] = 0;
                    }
                    // reuse the RX buffer
                    TCP_InsertRxBuffer(&port65534TCB, rxdataPort65534, sizeof(rxdataPort65534));
                }

                if(t_client >= socketTimeout)
                { 
                    // send board status message only once at 2 seconds
                    socketTimeout = t_client + 2;                    
                    //send data back to the source
                    TCP_Send(&port65534TCB, txdataPort65534, strlen(txdataPort65534));
                }
            }
            break;
        case SOCKET_CLOSING:
            TCP_SocketRemove(&port65534TCB);
            break;
        default:
            break;
            // we should not end up here
    }
}

void TCP_Client_Initialize(){
    
     //  Initialize the server IP address with your PC's IP address
    
    remoteSocket.addr.s_addr = MAKE_IPV4_ADDRESS(192, 168, 0, 38);
    remoteSocket.port = 65534;
}
/**
  TFTP client implementation
	
  Company:
    Microchip Technology Inc.

  File Name:
    tftp.c

  Summary:
     This is the implementation of Trivial File Transfer Protocol (TFTP) (RFC 1350).

  Description:
    This source file provides the implementation of the API for TFTP client.

 */

/*

©  [2015] Microchip Technology Inc. and its subsidiaries.  You may use this software 
and any derivatives exclusively with Microchip products. 
  
THIS SOFTWARE IS SUPPLIED BY MICROCHIP "AS IS".  NO WARRANTIES, WHETHER EXPRESS, 
IMPLIED OR STATUTORY, APPLY TO THIS SOFTWARE, INCLUDING ANY IMPLIED WARRANTIES OF 
NON-INFRINGEMENT, MERCHANTABILITY, AND FITNESS FOR A PARTICULAR PURPOSE, OR ITS 
INTERACTION WITH MICROCHIP PRODUCTS, COMBINATION WITH ANY OTHER PRODUCTS, OR USE 
IN ANY APPLICATION. 

IN NO EVENT WILL MICROCHIP BE LIABLE FOR ANY INDIRECT, SPECIAL, PUNITIVE, INCIDENTAL 
OR CONSEQUENTIAL LOSS, DAMAGE, COST OR EXPENSE OF ANY KIND WHATSOEVER RELATED 
TO THE SOFTWARE, HOWEVER CAUSED, EVEN IF MICROCHIP HAS BEEN ADVISED OF THE POSSIBILITY 
OR THE DAMAGES ARE FORESEEABLE.  TO THE FULLEST EXTENT ALLOWED BY LAW, MICROCHIP'S 
TOTAL LIABILITY ON ALL CLAIMS IN ANY WAY RELATED TO THIS SOFTWARE WILL NOT EXCEED 
THE AMOUNT OF FEES, IF ANY, THAT YOU HAVE PAID DIRECTLY TO MICROCHIP FOR THIS SOFTWARE.

MICROCHIP PROVIDES THIS SOFTWARE CONDITIONALLY UPON YOUR ACCEPTANCE OF THESE TERMS. 

*/

/**
 Section: Included Files
 */

#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include "tcpip_types.h"
#include "tcpip_config.h"
#include "tftp.h"
#include "tftp_handler_table.h"
#include "udpv4.h"




#define TFTP_OPCODE_SIZE 2
#define TFTP_PACKET_SIZE (TFTP_OPCODE_SIZE + 2)
#define TFTP_BLOCK_SIZE  2


const char *tftp_modes[] = {"netascii", "octet", "mail"}; // netascii and mail are ASCII, octet is binary
const char tftp_option[] = "blksize";
 
tftp_t tftp_msg;
volatile bool last_block = false;

static uint16_t current_block = 0x0000, prev_block = 0x0000, next_block = 0x0000;
uint32_t tftp_last_address = 0x00020000;



void TFTP_Configure(uint32_t tftp_server_address, char *tftp_filename, char *tftp_block_size)
{   
    tftp_msg.tftp_server_address = tftp_server_address;    
    strncpy(tftp_msg.tftp_filename,tftp_filename,strlen(tftp_filename));
    strncpy(tftp_msg.tftp_block_size,tftp_block_size,strlen(tftp_block_size));    
}



error_msg TFTP_Read_Write_Request(uint8_t opcode)
{
    uint8_t len_tftp_fn, len_tftp_mode, len_tftp_option, len_tftp_block_size;
    error_msg started = ERROR;
    uint32_t tftp_server_address = tftp_msg.tftp_server_address; 
    
    len_tftp_fn = strlen(tftp_msg.tftp_filename);
    len_tftp_mode = strlen(tftp_modes[1]);    
    len_tftp_option = strlen(tftp_option);
    len_tftp_block_size =  strlen(tftp_msg.tftp_block_size);   
    
    if(opcode == ((tftp_opcode)TFTP_READ_REQUEST || opcode == (tftp_opcode)TFTP_WRITE_REQUEST))
    {
        started = UDP_Start(tftp_server_address,SOURCEPORT_TFTP,DESTPORT_TFTP);
        
        if(started)
        {
            UDP_Write16(opcode);
            UDP_WriteBlock((char *)tftp_msg.tftp_filename,len_tftp_fn);
            UDP_Write8(0x00); //End of filename
            UDP_WriteBlock((char *)tftp_modes[1],len_tftp_mode);
            UDP_Write8(0x00); //End of mode
            UDP_WriteBlock((char *)tftp_option,len_tftp_option);
            UDP_Write8(0x00); //Option for block size
            UDP_WriteBlock((char *)tftp_msg.tftp_block_size,len_tftp_block_size);
            UDP_Write8(0x00); //End of option
            
            UDP_Send();
        }
        else printf("UDP_Start Failed\r\n");
    } 
    else printf("Failed Opcode = %d \r\n",opcode);
    
    return started;
}


void TFTP_Ack(uint8_t opcode)
{
    bool started = false;
    uint16_t dest_port;
    uint32_t tftp_server_address = tftp_msg.tftp_server_address; 
    
    if(opcode == TFTP_ACK)
    {
        dest_port = UDP_GetDestPort();
        started = (bool)UDP_Start(tftp_server_address,SOURCEPORT_TFTP,dest_port);  //jira: CAE_MCU8-5647
        if(started)
        {
            UDP_Write16(opcode);
            UDP_Write16(current_block);
            
            UDP_Send();
        }
    }   
}

void TFTP_Handler(int16_t length)   // jira:M8TS-608
{
    uint8_t opcode;
    char data[100];
    char recv_buffer[100];
    uint16_t block_size;
    int v;
    
    block_size =(uint16_t)atoi((const char *)tftp_msg.tftp_block_size);    //jira: CAE_MCU8-5647
    if(length > TFTP_PACKET_SIZE)
    {
        opcode        =  UDP_Read16();
        current_block =  UDP_Read16();
        if(opcode == tftp_optack)
        {
            current_block = 0; //send ACK with block number 0
            TFTP_Ack(TFTP_ACK);
        }        
        if(current_block == prev_block+1)
        {    
            next_block = current_block + 1;
            length = length - 4;
            if(((uint16_t)length <block_size) && (opcode == TFTP_DATA))   //jira: CAE_MCU8-5647
            {
                last_block = true;    
            }
            switch(opcode)
            {
                case tftp_read: //Read request - send first block of data
                    break;
                case tftp_write: //Write request - send ACK with block number 0
                    current_block = 0;
                    TFTP_Ack(TFTP_ACK);
                    break;
                case tftp_data: //Data Received - send ACK

                        while(length > 0)
                        {    
                            v = (length>100)?100:length; // One block of data is 1344 bytes long (0-1343). Transferred using 100 bytes buffer
                            memset(data,0,sizeof(data));    
                            memset(recv_buffer,0,sizeof(recv_buffer)); 
                            UDP_ReadBlock(data, (uint16_t)v);   //jira: CAE_MCU8-5647
                            Process_TFTP_Data(tftp_last_address, data, (uint16_t)v);  //jira: CAE_MCU8-5647
                            tftp_last_address = tftp_last_address + (uint16_t)v;   //jira: CAE_MCU8-5647
                            length -= v;
                        }
                        TFTP_Ack(TFTP_ACK);                
                    prev_block = current_block;
                    break;
                case tftp_ack: //ACK - send next block of data
                    break;
                case tftp_error: //Error
                    length = 0;
                    break;
               
            }
        }
        else if(current_block < prev_block)
        {
            TFTP_Ack(TFTP_ACK); 
            length=0;
            
        }
        else if(current_block > prev_block+1)
        {
            length=0;
        }
    }    
}


/*********************************************************************************/
/* PROCEDURE:	Process_TFTP_Data                                                */
/*                                                                               */
/* This procedure processes block of specified size .                            */
/* The storage can be  in EEPROM, Flash, etc. depending on TFTP_callBackTable    */
/* Input:                                                                        */
/*		address:		Destination Address 000000H - 3FFFFFH                    */
/*		data:		    data to be programmed                                    */
/*      length:         number of bytes to be programmed                         */
/*                                                                               */
/* Returns:                                                                      */
/*		1                                                                        */
/*                                                                               */
/*********************************************************************************/
uint8_t Process_TFTP_Data(uint32_t address, char *data, uint16_t length)
{
    const store_handler_t *hptr;   
    
    hptr = TFTP_callBackTable;
    for(uint8_t i=0; i<tableSize; i++)
    {
        if(hptr)
        {
            if(hptr->storeNumber == store_type)
            {
                hptr->TFTP_CallBack(address, data, length);
                break;
            }
        }
        hptr++;
    }
    
    return 1;
}
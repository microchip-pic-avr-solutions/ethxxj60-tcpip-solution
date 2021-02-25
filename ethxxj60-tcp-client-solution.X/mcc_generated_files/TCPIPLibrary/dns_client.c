/**
  DNS client implementation
	
  Company:
    Microchip Technology Inc.

  File Name:
    dns_client.c

  Summary:
    This is the implementation of DNS client.

  Description:
    This source file provides the implementation of the DNS client stack.

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

#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include "dns_client.h"
#include "udpv4.h"

#include "log.h"
#include "ip_database.h"
#include "lfsr.h"

#ifdef ENABLE_NETWORK_DEBUG
#define logMsg(msg, msgSeverity, msgLogDest)    logMessage(msg, LOG_DNS, msgSeverity, msgLogDest)
#else
#define logMsg(msg, msgSeverity, msgLogDest)
#endif

typedef struct
{
    const char *dnsName;
    uint32_t ipAddress;
    uint32_t ttl;
    bool isValid;
    time_t birthSecond;
} dns_map_t;

uint16_t dnsXidValue;

#define DNS_HEADER_SIZE 12
#define DNS_TYPE_SIZE   2
#define DNS_CLASS_SIZE  2
#define DNS_QUERY_SIZE (DNS_TYPE_SIZE + DNS_CLASS_SIZE)
#define DNS_PACKET_SIZE (DNS_HEADER_SIZE + DNS_QUERY_SIZE)
#define ARRAYSIZE(a)    (sizeof(a) / sizeof(*(a)))
#define DNS_MAP_SIZE 8

dns_map_t dnsCache[DNS_MAP_SIZE]; // maintain a small database of IP address & Host name
mac48Address_t ethMACdns;

static int generateSeed(void);

void DNS_Init(void)
{
    srand(generateSeed());
	memset(dnsCache,0,sizeof(dnsCache));
}

static int generateSeed(void)
{
    int seed;
    ETH_GetMAC((uint8_t *) &ethMACdns);
    seed = lfsrWithSeed(ethMACdns.mac_array[2]);
    seed <<= 8;
    seed |= lfsrWithSeed(ethMACdns.mac_array[3]);
    seed <<= 8;
    seed |= lfsrWithSeed(ethMACdns.mac_array[4]);
    seed <<= 8;
    seed |= lfsrWithSeed(ethMACdns.mac_array[5]);
    return seed;
}

void DNS_Query(const char *str)
{    
    uint8_t lock = 0 , i;
    uint8_t len_str;
    len_str = strlen((char*)str);
    error_msg started;
    dns_map_t *entryPointer;
    dns_map_t *oldestEntry;
    time_t oldestAge;
        
    logMsg("DNS Query", LOG_INFO, (LOG_DEST_CONSOLE|LOG_DEST_ETHERNET));
    
    entryPointer = dnsCache;
    oldestEntry = entryPointer;
    oldestAge = entryPointer->birthSecond;
    
    // find the oldest cache entry
    for(unsigned x=0;x<ARRAYSIZE(dnsCache);x++)
    {
        if(entryPointer->birthSecond > oldestAge )
        {
            oldestAge = entryPointer->birthSecond;
            oldestEntry = entryPointer;
        }
        entryPointer ++;
    }
    entryPointer = oldestEntry;
    
    started = UDP_Start(ipdb_getDNS(),53,53);
	dnsXidValue = rand()*rand();
    if(started==SUCCESS)
    {
        // DNS Header
        UDP_Write16(dnsXidValue);
        UDP_Write16(0x0100); //QR Opcode AA|TC|RD|RA| Z  RCODE
        UDP_Write16(0x0001); //QDCOUNT Questions
        UDP_Write16(0x0000); //ANCOUNT Answer RRs
        UDP_Write16(0x0000); //NSCOUNT Authority RRs
        UDP_Write16(0x0000); //ARCOUNT Additional RRs

         // DNS Query
        for(i = 0 ; i <= len_str ; i++)
        {
            if(str[i]=='.' || str[i]=='\0')
            {
                //len = i-lock;
                UDP_Write8(i-lock);
                for(;lock<i;lock++)    // jira:M8TS-608
                {
                    //dns_q[lock+1]=str[lock];
                    UDP_Write8(str[lock]);
                }

                lock=i+1;
            }
        }
        UDP_Write8(0x00); //End of String

        UDP_Write16(0x0001); //QTYPE  Type - We use type 'A' for Ipv4 host address
        UDP_Write16(0x0001); //QCLASS Class

        UDP_Send();
        entryPointer->dnsName = str;
        entryPointer->isValid = false;
    }

}

void DNS_Handler(int16_t length)    // jira:M8TS-608
{
    uint16_t  v;
    unsigned char dnsR[255];
    uint16_t answer, authorityRR;
    uint32_t ipAddress;
    uint32_t ttl;
    uint16_t type, dataLength;

    dns_map_t *entryPointer;
    uint8_t i, nameLen,lock =0;

    logMsg("DNS Packet Received", LOG_INFO, (LOG_DEST_CONSOLE|LOG_DEST_ETHERNET));
    
    entryPointer = dnsCache;

    if(length > DNS_HEADER_SIZE)
    {
        v = UDP_Read16();
        if(v == dnsXidValue)
        {
            UDP_Read32(); // Flags, Questions
            answer = UDP_Read16(); //Answer RRs
            authorityRR = UDP_Read16(); //Authority RRs
            UDP_Read16(); // Additional RRs
            length -= 12;

           if(length > 0)
            {
                while((nameLen=UDP_Read8())!= 0x00)
                {
                    while(nameLen--)
                    {
                        dnsR[lock] = UDP_Read8();
                        lock++;
                    }
                    dnsR[lock] = '.';
                    lock++;
                }
                dnsR[lock-1] ='\0';

                UDP_Read32();
               length -= lock + 5;
               //DNS Answers
                while(answer)
                {
                    UDP_Read16(); //Name
                    type = UDP_Read16(); //Type
                    UDP_Read16();//Class
                    ttl = UDP_Read32(); //Time to Live
                    dataLength = UDP_Read16(); //Data length
                    if(type == 0x0001)
                    {
                        ipAddress = UDP_Read32();
                    }
                    else
                    {
                        while(dataLength--) UDP_Read8();
                        length -=14+dataLength;
                    }
//                    if(answer > 1)
//                    {
//                        length = length - 21;
//                        UDP_dump(length);
//                    }
                    answer--;
                }
            }
        }
        // find the oldest entry in the table
        dns_map_t *dnsPtr = dnsCache;
        for(uint8_t x=DNS_MAP_SIZE; x !=0; x--)
        {
            if(entryPointer->birthSecond < dnsPtr->birthSecond)
            {
                entryPointer = dnsPtr;
            }
            /* Increment the pointer to get the next element from the array. */
            dnsPtr++;
        }
        for(i = 0; i < ARRAYSIZE(dnsCache);i++)
        {
            if(strcmp(entryPointer->dnsName, (char *)dnsR) == 0)   // jira:M8TS-608
            {
                // the entry_pointer is now pointing to the oldest entry
                // replace the entry with the received data
                entryPointer->birthSecond = time(0);
                entryPointer->ipAddress = ipAddress;
                entryPointer->ttl = ttl;
                entryPointer->isValid = true;
                break;
            }
            entryPointer ++;
        }
    }
}

void DNS_Update(void) // 
{
}

uint32_t DNS_Lookup(const char *dnsName)
{
    dns_map_t *entryPointer = dnsCache;
    uint8_t x;
    
    logMsg("DNS Lookup Request", LOG_INFO, (LOG_DEST_CONSOLE|LOG_DEST_ETHERNET));
    
    for(x=0;x<DNS_MAP_SIZE;x++)
    {
        if(entryPointer->isValid)
        {
            if(strcmp(entryPointer->dnsName, dnsName)==0)
            {
                char str[80];
                sprintf(str,"DNS Found %s in cache %ul",dnsName, entryPointer->ipAddress);
                logMsg(str, LOG_INFO, (LOG_DEST_CONSOLE|LOG_DEST_ETHERNET));
                return entryPointer->ipAddress;
            }
        }
        entryPointer ++;
    }
    DNS_Query(dnsName);
    return 0;
}
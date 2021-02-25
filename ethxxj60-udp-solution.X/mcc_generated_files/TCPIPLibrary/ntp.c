/**
 NTP client implementation
	
  Company:
    Microchip Technology Inc.

  File Name:
    ntp.c

  Summary:
    This is the implementation of NTP client.

  Description:
    This source file provides the implementation of the NTP client protocol.

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
#include <stdio.h>
#include <string.h>
#include <time.h>
#include "../mcc.h"
#include "ntp.h"
#include "udpv4.h"
#include "rtcc.h"
#include "log.h"
#include "ip_database.h"

#define NTP_PACKET_SIZE 48

// the time offset converts from unix time (seconds since Jan 1 1970) to RFC 868 time (seconds since Jan 1 1900)
// the RFC indicates that this is sufficient until 2036
#define NTP_TIME_OFFSET 2208988800


void NTP_Request(void)
{
    uint32_t ntpAddress;
    error_msg started;
    time_t myTime;

    ntpAddress = ipdb_getNTP();
    myTime = time(0);
    myTime += NTP_TIME_OFFSET; // converting to RFC time
    
    //SYSLOG_log ("NTP Request", true, LOG_NTP, LOG_DEBUG);
    
    if(ntpAddress != 0)
    {
        started = UDP_Start(ntpAddress,123,123);
        if(started == SUCCESS)
        {
            UDP_Write8(0xDB); //Leap Indicator - 11, Version number - 011 (3), Mode - 011 (3) client
            UDP_Write8(0x00); //Stratum level of local clock - 0-unspecified, 1-primary reference, 2 to n - secondary reference
            UDP_Write8(0x0A); //Poll - Max interval between succesive messages - 10 =  1024 secs
            UDP_Write8(0xEC); //Precision - Precision of local clock
            UDP_Write32(0x00003B68); //Synchronizing Distance - Roundtrip delay to primary synchronizing source
            UDP_Write32(0x00042673); //Estimated Drift Rate
            UDP_Write32(ntpAddress);
            
            UDP_Write32((uint32_t)myTime); //Reference Timestamp in seconds 
            UDP_Write32(0x0000);     // - Reference Timestamp Fraction assumed to 0
            UDP_Write32((uint32_t)myTime); //Origin Timestamp in seconds    
            UDP_Write32(0x0000);     // - Origin Timestamp Fraction assumed to 0
            UDP_Write32((uint32_t)myTime); //Receive Timestamp in seconds  
            UDP_Write32(0x0000);     // - Receive Timestamp Fraction assumed to 0
            UDP_Write32((uint32_t)myTime); //Transmit Timestamp in seconds  
            UDP_Write32(0x0000);     // - Transmit Timestamp Fraction assumed to 0            
			UDP_Send();
        }
    }
}

void NTP_Handler(int16_t length)    // jira:M8TS-608
{
    uint32_t timeIntegralPart,timeFractionalPart;
    
    UDP_Read8(); //Leap Indicator - 11, Version number - 011 (3), Mode - 011 (3) client
    UDP_Read8(); //Stratum level of local clock - 0-unspecified, 1-primary reference, 2 to n - secondary reference
    UDP_Read8(); //Poll - Max interval between succesive messages - 10 =  1024 secs
    UDP_Read8(); //Precision - Precision of local clock
    UDP_Read32(); //Synchronizing Distance - Round trip delay to primary synchronizing source
    UDP_Read32(); //Estimated Drift Rate
    UDP_Read32(); // reference clock identifier
    
    // Reference Timestamp
    UDP_Read32();
    UDP_Read32();

    // Originate Timestamp
    UDP_Read32();
    UDP_Read32();

    // Receive Timestamp
    timeIntegralPart = UDP_Read32();
    timeFractionalPart = UDP_Read32();
    
    // Transmit timestamp
    UDP_Read32();
    UDP_Read32();
    
    timeIntegralPart -= NTP_TIME_OFFSET;
            
    rtcc_set((time_t *)timeIntegralPart);   // jira:M8TS-608
}


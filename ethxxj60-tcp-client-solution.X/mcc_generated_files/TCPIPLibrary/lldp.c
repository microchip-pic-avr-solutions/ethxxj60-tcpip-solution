/**
  DHCP v4 client implementation

  Company:
    Microchip Technology Inc.

  File Name:
    lldp.c

  Summary:
     This is the implementation of LLDP.

  Description:
    This source file provides the implementation of the Link Layer Discovery Protocol.

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
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "network.h"
#include "physical_layer_interface.h"
#include "log.h"
#include "lldp_tlv_handler_table.h"
#include "ipv4.h"
#include "ip_database.h"

uint8_t rxState;
bool lldpdu_end;
static const mac48Address_t genericMAC ={0X01,0X80,0XC2,0X00,0X00,0X0E};													  
uint8_t throughTimes;
uint16_t txSizeCount,totalSize;
static orgProcessFlags orgProcFlags;

const char PortDescription[]      ="Vendor LED"; 
const char HardwareRevision[]     ="Rev 1.0";
const char FirmwareRevision[]     ="Rev 1.0";
const char SoftwareRevision[]     ="Rev 1.0";
const char SerialNumber[]         ="US-1234";
const char Manufacturer[]         ="Vendor ID";
const char ModelName[]            ="LED-Dimmable";
const char AssetID[]              ="V1234";
const char MUDExt[]               ="Add the mud info";

char* portdescription=NULL;
char* hardwarerev=NULL;
char* firmwarerev=NULL;
char* softwarerev=NULL;
char* serialnumber=NULL;
char* manufacturer=NULL;
char* modelname=NULL;
char* assetID=NULL;
char* mudext=NULL;

char PortInterfaceName[7]="Gi?/?";//portID
#define ARRAYSIZE(a)    (sizeof(a)/sizeof(*(a)))

  /*This is a per-interface structure.  Part of lldp_port.*/
typedef struct {
    uint8_t  state;             /**< The tx state for this interface */
    bool     localChange;       /**< IEEE 802.1AB var (from where?) */ 	//: Look at 9.2.7.8
    uint16_t txTTL;             /**< IEEE 802.1AB var (from where?) */
    lldp_tx_timers_t timers;    /**< The lldp tx state machine timers for this interface */
    uint8_t txCredit;           /**< IEEE 802.1AB 9.2.5.16*/
    uint8_t txFast;                /**< IEEE 802.1AB 9.2.5.18*/
    bool    txNow;              /**< IEEE 802.1AB 9.2.5.20*/
}lldp_tx_port_t ;

typedef enum {
    disabled=0,
    enabledTxOnly = 1,
    enabledRxOnly = 2,
    enabledRxTx = 3
}lldp_admin_status;
#define ADMIN_STATUS_RX_MASK 0x02
#define ADMIN_STATUS_TX_MASK 0x01

typedef struct {
    uint8_t state;
    bool badFrame;
    bool rcvFrame;
}lldp_rx_port_t ;

typedef struct {
    uint8_t source_mac[6];
    uint8_t source_ipaddr[4];
    bool newNeighbor;
    lldp_tx_port_t tx;
    lldp_rx_port_t rx;
    bool portEnabled;     /**< IEEE 802.1AB 9.2.4.1 */
    uint8_t adminStatus;  /**< IEEE 802.1AB 9.2.5.1 */
    uint16_t allocatedPower;
    uint16_t desiredPower;
}lldp_per_port_t ;
lldp_per_port_t lldp_port;


typedef enum {
    TX_TIMER_INITIALIZE,
    TX_TIMER_IDLE,
    TX_TIMER_EXPIRES,
    TX_TICK,
    SIGNAL_TX,
    TX_FAST_START
}lldp_txTimerStates_t;

uint8_t txTimerState;

typedef enum {
    TX_LLDP_INITIALIZE,
    TX_IDLE,
    TX_INFO_FRAME,
    TX_SHUTDOWN_FRAME
}lldp_txStates_t;


typedef enum {
    LLDP_WAIT_PORT_OPERATIONAL,
    RX_LLDP_INITIALIZE,
    RX_WAIT_FOR_FRAME,
    RX_FRAME,
}lldp_rxStates_t;

#ifdef ENABLE_NETWORK_DEBUG
#define logMsg(msg, msgSeverity, msgLogDest)    logMessage(msg, LOG_LLDP, msgSeverity, msgLogDest) 
#else
#define logMsg(msg, msgSeverity, msgLogDest)
#endif			   	  
#define disableLLDP()       do{lldp_port.adminStatus=(lldp_admin_status)disabled;}while(0)
#define enableLLDP_rx()     do{lldp_port.adminStatus=(lldp_admin_status)enabledRxOnly;}while(0)
#define enableLLDP_tx()     do{lldp_port.adminStatus=(lldp_admin_status)enabledTxOnly;}while(0)
#define enableLLDP_rx_tx()  do{lldp_port.adminStatus=(lldp_admin_status)enabledRxTx;}while(0)

#define LLDP_rxEnabled()    ((lldp_port.adminStatus & ADMIN_STATUS_RX_MASK) != 0)
#define LLDP_txEnabled()    ((lldp_port.adminStatus & ADMIN_STATUS_TX_MASK) != 0)
#define LLDP_isDisabled()   (lldp_port.adminStatus == 0)

void txTimerInitializeLLDP(void);
void txTimerStateMachine(void);
void txTimerInitializeLLDP(void);
void txStateMachine(void);
void rxStateMachine(void);
void txFrame(void);

void constrInfoLLDPDU(void);
uint8_t tl_tlvConstruct(uint8_t, uint16_t ,uint8_t);
bool buffFitCheck(uint16_t);
void chkBoundaryCase(void);
bool createEndTLV(void);

error_msg createOrgTLV(uint8_t,uint16_t,uint32_t);
error_msg createBasicTLV(uint8_t,uint8_t,uint16_t);
error_msg createOptionalTLV(uint8_t,uint8_t,uint16_t);

void rxProcessFrame(void);
void processMandatory(uint8_t, uint16_t);
void processError(void);
bool processEnd(uint16_t);
void processOptional(uint8_t, uint16_t);
void processSpecific(uint16_t);

static void swapEvenBytes(void*, uint8_t);
static error_msg setTheTLV(uint8_t, uint16_t, uint32_t, char*);	   
/****************************************APIs******************************************/

void LLDP_InitRx(void)
{
    enableLLDP_rx();
    lldp_port.portEnabled   =true;
    lldp_port.allocatedPower=0;
    lldp_port.desiredPower  =0;
}

void LLDP_InitTx(void)
{
    enableLLDP_tx();
    lldp_port.portEnabled   =true;
    lldp_port.allocatedPower=0;
    lldp_port.desiredPower  =0;
}

void LLDP_InitRxTx(void)
{
    enableLLDP_rx_tx();
    lldp_port.portEnabled   =true;
    lldp_port.allocatedPower=0;
    lldp_port.desiredPower  =0;
}

void LLDP_DecTTR(void)
{
    if(lldp_tx_timers.txTTR > 0)lldp_tx_timers.txTTR--;
}

MulticastMacAddr LLDP_SetDestAddress(void)
{

    // The Mac address has to be a multicast address and we can only have a valid few
    //namely : 01:80:C2:00:00:00, or 01:80:C2:00:00:03, or 01:80:C2:00:00:0E
    //so we choose one of these here
    //If you provide all Zeroes below then 01:80:C2:00:00:0E will be used automatically
    mac48Address_t m={0x01,0x80,0xC2,0x0,0x0,0x0E};

    return m;
}

void LLDP_SetDesiredPower(uint16_t pwr)
{
    lldp_port.desiredPower=pwr;
}

uint16_t LLDP_GetAllocatedPower(void)
{														 
    return lldp_port.allocatedPower;
}

void LLDP_Run(void)
{
    if(lldp_port.portEnabled)
    {
        // should I transmit?
        if( LLDP_txEnabled() )
        {
            txTimerStateMachine();
            txStateMachine();
        }
        
        if( LLDP_isDisabled() )
        {
            lldp_port.portEnabled=false;
        }
    }
    else
    {
		logMsg("Port Disabled!", LOG_WARNING, (LOG_DEST_CONSOLE|LOG_DEST_ETHERNET));			
    }
}
	
error_msg LLDP_createIeeeMDI(uint8_t sub, uint16_t buffLen, uint32_t orgOUI)
{
    error_msg built=ERROR;  //jira: CAE_MCU8-5647
    built =createOrgTLV(sub,buffLen,orgOUI);
        //IEEE
    if(built== SUCCESS && (ETH_GetByteCount()<=MAX_TXBUFF_SIZE) ) //jira: CAE_MCU8-5647
    {
        ETH_Write8(0x0f);ETH_Write8(0x01);ETH_Write8(0x05);ETH_Write8(0x53);
		
        if(lldp_port.allocatedPower != lldp_port.desiredPower)
        {
            if(orgProcFlags.is4WireSupported==true)
            {
                if(lldp_port.desiredPower != 0 && lldp_port.desiredPower<= 0x01FE)
                {
                    ETH_Write8(lldp_port.desiredPower>>8);
                    ETH_Write8(lldp_port.desiredPower);
                }
                else
                {
                    ETH_Write8(0x01);
                    ETH_Write8(0xFE);
                }
            }
            else if(lldp_port.desiredPower <= 0xFF && lldp_port.desiredPower >= 0x82)
                    {
                        ETH_Write8(lldp_port.desiredPower>>8);
                        ETH_Write8(lldp_port.desiredPower);
                    }
            else if(lldp_port.desiredPower > 0xFF) 
                {
                    ETH_Write8(0x00);
                    ETH_Write8(0xFF);
                }
            else
            {
                ETH_Write8(0x00);
                ETH_Write8(0x82);
            }
        }
        else
        {
            if(lldp_port.desiredPower == 0 ) //13 watts only
            {
                ETH_Write8(0x00);
                ETH_Write8(0x82);
            }
            else
            {
                ETH_Write8(lldp_port.allocatedPower>>8);
                ETH_Write8(lldp_port.allocatedPower);
            }
        }
        ETH_Write8(lldp_port.allocatedPower>>8);
        ETH_Write8(lldp_port.allocatedPower);
		
        built=SUCCESS;
    }
    return built;
}

error_msg LLDP_createCiscoMDI(uint8_t sub, uint16_t buffLen, uint32_t orgOUI)
{
    error_msg built=ERROR; //jira: CAE_MCU8-5647				
	uint8_t options = 0;					
    built =createOrgTLV(sub,buffLen,orgOUI);
    //IEEE
    if(lldp_port.desiredPower>=0xFF)
    {
        orgProcFlags.PD_requestSparePairOn=true;
    }
      else 
    {
      orgProcFlags.PD_requestSparePairOn=false;
    }
    if(built== SUCCESS && (ETH_GetByteCount()<=MAX_TXBUFF_SIZE) ) //jira: CAE_MCU8-5647
    {
        if(orgProcFlags.is4WireSupported==true) options |= 0x01;
        if(independentSparedArchitectur == false) options |= 0x02;
        if(orgProcFlags.is4WireSupported == true && orgProcFlags.PD_requestSparePairOn==true) options |= 0x04; 
        if(orgProcFlags.poeEnabledPair == true)options |= 0x08;						   
        ETH_Write8(options);
        built=SUCCESS;
    }
    return built;
}

error_msg LLDP_createCiscoOUI(uint8_t sub, uint16_t buffLen, uint32_t orgOUI)
{
    error_msg built=ERROR;  //jira: CAE_MCU8-5647
    built =createOrgTLV(sub,buffLen,orgOUI);
        //IEEE
    if(built== SUCCESS && (ETH_GetByteCount()<=MAX_TXBUFF_SIZE) )  //jira: CAE_MCU8-5647
    {
        ETH_Write8(0xb); //IoT device
        built=SUCCESS;
    }
    return built;
}

error_msg LLDP_createCiscoClass(uint8_t sub, uint16_t buffLen, uint32_t orgOUI)
{
    error_msg built=ERROR;  //jira: CAE_MCU8-5647
    built =createOrgTLV(sub,buffLen,orgOUI);
        //IEEE
    if(built== SUCCESS && (ETH_GetByteCount()<=MAX_TXBUFF_SIZE) )  //jira: CAE_MCU8-5647
    {
        ETH_Write8(0x03); //bit0=1 => actuator present), bit1=1 =>sensor Present
        built=SUCCESS;
    }
    return built;
}

error_msg LLDP_createCiscoProtocol(uint8_t sub, uint16_t buffLen, uint32_t orgOUI)
{
    error_msg built=ERROR;  //jira: CAE_MCU8-5647
    built =createOrgTLV(sub,buffLen,orgOUI);
        //IEEE
    if(built== SUCCESS && (ETH_GetByteCount()<=MAX_TXBUFF_SIZE) )  //jira: CAE_MCU8-5647
    {
        ETH_Write8(0); //COAP is the supported Protocol
        built=SUCCESS;
    }
    return built;
}
    
error_msg LLDP_createIeeeConfig(uint8_t sub, uint16_t buffLen, uint32_t orgOUI)
{
    error_msg built=ERROR;  //jira: CAE_MCU8-5647
    built =createOrgTLV(sub,buffLen,orgOUI);    //jira: CAE_MCU8-5647
        //IEEE
    if(built== SUCCESS && (ETH_GetByteCount()<=MAX_TXBUFF_SIZE) )  //jira: CAE_MCU8-5647
    {
        //auto Negotiate   advert capabilities       op MAU type
        ETH_Write8(0x03);  ETH_Write16(0x6c01);      ETH_Write16(0x0010);       
        built=SUCCESS;
    }
    return built;
}


error_msg LLDP_createTiaMED_cap(uint8_t sub, uint16_t buffLen, uint32_t orgOUI)
{
    error_msg built=ERROR;  //jira: CAE_MCU8-5647
    built =createOrgTLV(sub,buffLen,orgOUI);
        //IEEE
    if(built== SUCCESS && (ETH_GetByteCount()<=MAX_TXBUFF_SIZE) )  //jira: CAE_MCU8-5647
    {
        //LLDP-MED_Capabilities
        ETH_Write8(0x00); ETH_Write8(0x11); ETH_Write8(0x01);
        built=SUCCESS;
    }
    return built;
}


error_msg LLDP_createTia_Net_pol(uint8_t sub, uint16_t buffLen, uint32_t orgOUI)
{
    error_msg built=ERROR;  //jira: CAE_MCU8-5647
    built =createOrgTLV(sub,buffLen,orgOUI);
        //IEEE
    if(built== SUCCESS && (ETH_GetByteCount()<=MAX_TXBUFF_SIZE) )  //jira: CAE_MCU8-5647
    {
        //LLDP-MED
        ETH_Write8(0x00); ETH_Write8(0x00); ETH_Write8(0x00);ETH_Write8(0x00);
        built=SUCCESS;
    }
    return built;
}


error_msg LLDP_createTia_PWR_MDI(uint8_t sub, uint16_t buffLen, uint32_t orgOUI)
{
    error_msg built=ERROR;  //jira: CAE_MCU8-5647
    built =createOrgTLV(sub,buffLen,orgOUI);
        //IEEE
    if(built== SUCCESS && (ETH_GetByteCount()<=MAX_TXBUFF_SIZE) )  //jira: CAE_MCU8-5647
    {
        //LLDP-MED
        ETH_Write8(0x51); ETH_Write8(0x00);
        if(lldp_port.desiredPower <= 0xFF) ETH_Write8(lldp_port.desiredPower);
        else
        ETH_Write8(0xFF);
        
        built=SUCCESS;
    }
    return built;
}

error_msg LLDP_createTia_HW_Rev(uint8_t sub, uint16_t buffLen, uint32_t orgOUI)
{
    error_msg built=ERROR;  //jira: CAE_MCU8-5647
    char* ptr = hardwarerev;

    if(hardwarerev != NULL)
    {
        built = setTheTLV(sub, buffLen, orgOUI, ptr);
    }
    else
    {
        buffLen = sizeof(HardwareRevision); 
        built =createOrgTLV(sub,buffLen,orgOUI);
        if(built== SUCCESS && (ETH_GetByteCount()<=MAX_TXBUFF_SIZE) )  //jira: CAE_MCU8-5647
        {
            ETH_WriteBlock(HardwareRevision,buffLen);
            built=SUCCESS;
        }
    }
    
    return built;
}

error_msg LLDP_createTia_FW_Rev(uint8_t sub, uint16_t buffLen, uint32_t orgOUI)
{
    error_msg built=ERROR;  //jira: CAE_MCU8-5647
    char* ptr = firmwarerev;
    
    if(firmwarerev != NULL)
    {
        built = setTheTLV(sub, buffLen, orgOUI, ptr);
    }
    else
    {
        buffLen = sizeof(FirmwareRevision); 
        built =createOrgTLV(sub,buffLen,orgOUI);
        if(built== SUCCESS && (ETH_GetByteCount()<=MAX_TXBUFF_SIZE) )  //jira: CAE_MCU8-5647
        {
            ETH_WriteBlock(FirmwareRevision,buffLen);
            built=SUCCESS;
        }
    }
    return built;
}

error_msg LLDP_createTia_Srl_Num(uint8_t sub, uint16_t buffLen, uint32_t orgOUI)
{
    error_msg built=ERROR;  //jira: CAE_MCU8-5647
    char* ptr = serialnumber;
    
    if(serialnumber != NULL)
    {
        built = setTheTLV(sub, buffLen, orgOUI, ptr);
    }
    if (built != SUCCESS && serialnumber ==NULL)
    {
        buffLen = sizeof(SerialNumber); 
        built =createOrgTLV(sub,buffLen,orgOUI);
        if(built== SUCCESS && (ETH_GetByteCount()<=MAX_TXBUFF_SIZE) )  //jira: CAE_MCU8-5647
        {
            ETH_WriteBlock(SerialNumber,buffLen);
            built=SUCCESS;
        }
    }
    return built;
}

error_msg LLDP_createTia_SW_Rev(uint8_t sub, uint16_t buffLen, uint32_t orgOUI)
{
    error_msg built=ERROR;  //jira: CAE_MCU8-5647
    char* ptr = softwarerev;
    
    if(softwarerev != NULL)
    {
        built = setTheTLV(sub, buffLen, orgOUI, ptr);
    }
    if (built != SUCCESS && softwarerev ==NULL)
    {
        buffLen = sizeof(SoftwareRevision); 
        built =createOrgTLV(sub,buffLen,orgOUI);
        if(built== SUCCESS && (ETH_GetByteCount()<=MAX_TXBUFF_SIZE) )  //jira: CAE_MCU8-5647
        {
            ETH_WriteBlock(SoftwareRevision,buffLen);
            built=SUCCESS;
        }
    }
    return built;
}

error_msg LLDP_createTia_Manufacturer_Name(uint8_t sub, uint16_t buffLen, uint32_t orgOUI)
{
    error_msg built=ERROR;  //jira: CAE_MCU8-5647
    char* ptr = manufacturer;
    
    if(manufacturer != NULL)
    {
        built = setTheTLV(sub, buffLen, orgOUI, ptr);
    }
    if (built != SUCCESS && manufacturer == NULL)
    {
        buffLen = sizeof(Manufacturer); 
        built =createOrgTLV(sub,buffLen,orgOUI);
        if(built== SUCCESS && (ETH_GetByteCount()<=MAX_TXBUFF_SIZE) )  //jira: CAE_MCU8-5647
        {
            ETH_WriteBlock(Manufacturer,buffLen);
            built=SUCCESS;
        }
    }
    return built;
}

error_msg LLDP_createTia_Model_Name(uint8_t sub, uint16_t buffLen, uint32_t orgOUI)
{
    error_msg built=ERROR;  //jira: CAE_MCU8-5647
    char* ptr = modelname;
    
    if(modelname != NULL)
    {
        built = setTheTLV(sub, buffLen, orgOUI, ptr);
    }
    if (built != SUCCESS && modelname == NULL)
    {
        buffLen = sizeof(ModelName); 
        built =createOrgTLV(sub,buffLen,orgOUI);
        if(built== SUCCESS && (ETH_GetByteCount()<=MAX_TXBUFF_SIZE) )  //jira: CAE_MCU8-5647
        {
            ETH_WriteBlock(ModelName,buffLen);
            built=SUCCESS;
        }
    }
    return built;
}

error_msg LLDP_createTia_Asset_ID(uint8_t sub,  uint16_t buffLen, uint32_t orgOUI)
{
    error_msg built=ERROR;  //jira: CAE_MCU8-5647
    char* ptr = assetID;
    
    if(assetID != NULL)
    {
        built = setTheTLV(sub, buffLen, orgOUI, ptr);
    }
    if (built != SUCCESS && assetID == NULL)
    {
        buffLen = sizeof(AssetID); 
        built =createOrgTLV(sub,buffLen,orgOUI);
        if(built== SUCCESS && (ETH_GetByteCount()<=MAX_TXBUFF_SIZE) )  //jira: CAE_MCU8-5647
        {
            ETH_WriteBlock(AssetID,buffLen);
            built=SUCCESS;
        }
    }
    return built;
}

error_msg LLDP_createCisco_MUD_Ext(uint8_t sub,  uint16_t buffLen, uint32_t orgOUI)
{
    error_msg built=ERROR;  //jira: CAE_MCU8-5647
    char* ptr = mudext;
    
    if(mudext != NULL)
    {
        built = setTheTLV(sub, buffLen, orgOUI, ptr);
    }
    if (built != SUCCESS && mudext == NULL)
    {
        buffLen = sizeof(MUDExt); 
        built =createOrgTLV(sub,buffLen,orgOUI);
        if(built== SUCCESS && (ETH_GetByteCount()<=MAX_TXBUFF_SIZE) )  //jira: CAE_MCU8-5647
        {
            ETH_WriteBlock(MUDExt,buffLen);
            built=SUCCESS;
        }
    }
    return built;
}

error_msg createChassisTLV(uint8_t type,uint8_t sub,uint16_t buffLen)
{
    error_msg built=ERROR;  //jira: CAE_MCU8-5647

    if (sub == CHASSIS_MAC_ADDRESS)
    {
        mac48Address_t ChassisMacAddr;
        ChassisMacAddr = *(MAC_getAddress());
        if (sizeof(ChassisMacAddr)< buffLen)
			buffLen = sizeof(ChassisMacAddr);
        built =createBasicTLV(type,sub,buffLen);

        if(built==SUCCESS && (ETH_GetByteCount()<=MAX_TXBUFF_SIZE) )
        {								 
            ETH_WriteBlock((char *)&ChassisMacAddr,buffLen);    // jira:M8TS-608
            built=SUCCESS;
        }
     }
    return built;
}

error_msg createPortTLV(uint8_t type,uint8_t sub,uint16_t buffLen)
{
    error_msg built=ERROR;  //jira: CAE_MCU8-5647

     if (sub == PORT_INTERFACE_NAME)
    {
        if (ARRAYSIZE(PortInterfaceName)< buffLen)
                buffLen = ARRAYSIZE(PortInterfaceName);
        built =createBasicTLV(type,sub,buffLen);
    
        if(built==SUCCESS && (ETH_GetByteCount()<=MAX_TXBUFF_SIZE) )
        {
            ETH_WriteBlock(PortInterfaceName,buffLen);
            built=SUCCESS;
        }
    }
    return built;
}

error_msg createTTLTLV(uint8_t type,uint8_t sub,uint16_t buffLen)
{
    error_msg built=ERROR;  //jira: CAE_MCU8-5647
    built =createBasicTLV(type,sub,buffLen);
    if(built==SUCCESS && (ETH_GetByteCount()<=MAX_TXBUFF_SIZE) )
    {
            ETH_Write16(120);
            built=SUCCESS;
    }

    return built;
}

error_msg createPortDesc(uint8_t type,uint8_t sub,uint16_t buffLen)
{
    error_msg built=ERROR;  //jira: CAE_MCU8-5647
    static bool firstEntry =true;
    static uint8_t count=0;
    char* ptr = portdescription;
   
    if(portdescription != NULL)
    {
        if(firstEntry)
        {
            while (*ptr++ != 0)count++;    // jira:M8TS-608
            ptr = portdescription;
            firstEntry = false;
        }
        built =createBasicTLV(type,sub,count);
        if(built== SUCCESS && (ETH_GetByteCount()<=MAX_TXBUFF_SIZE) )  //jira: CAE_MCU8-5647
        {  
            ETH_WriteBlock(portdescription,count);
            built=SUCCESS;
        }	 
    }
    if (built != SUCCESS && portdescription == NULL)
    {
        buffLen = sizeof(PortDescription); 
        built =createBasicTLV(type,sub,buffLen);
        if(built== SUCCESS && (ETH_GetByteCount()<=MAX_TXBUFF_SIZE) )  //jira: CAE_MCU8-5647
        {
            ETH_WriteBlock(PortDescription,buffLen);
            built=SUCCESS;
        }
    }
    return built;   //jira: CAE_MCU8-5647
}

error_msg createSysCap(uint8_t type,uint8_t sub,uint16_t buffLen)
{
    error_msg built= ERROR;
    built =createBasicTLV(type,sub,buffLen);
    if(built==SUCCESS && (ETH_GetByteCount()<=MAX_TXBUFF_SIZE) )
    {
             ETH_Write16(0);
             ETH_Write16(0);
    }

    return built;
}

error_msg createSystemDesc  (uint8_t type,uint8_t sub,uint16_t buffLen)
{
    error_msg built=ERROR;  //jira: CAE_MCU8-5647
    uint8_t len=0;
    char errorString[64];
    
    len=85+sizeof(PortDescription)+sizeof(HardwareRevision)+sizeof(Manufacturer)+sizeof(FirmwareRevision)+sizeof(SoftwareRevision)+sizeof(ModelName);
       
    sprintf(errorString,"Total len:%u  PortLen:%u and sizeof :%u", len,strlen(PortDescription),sizeof(PortDescription));
	logMsg(errorString, LOG_INFO, (LOG_DEST_CONSOLE|LOG_DEST_ETHERNET));
	
    built =createBasicTLV(type,sub,len);
    
    if(built==SUCCESS && (ETH_GetByteCount()<=MAX_TXBUFF_SIZE) )
    {
        ETH_WriteString("sysDescr.0 = STRING: <<Port_Desc: ");
        ETH_WriteBlock(PortDescription,sizeof(PortDescription));
        ETH_WriteString("; HW_REV: ");
        ETH_WriteBlock(HardwareRevision,sizeof(HardwareRevision));
        ETH_WriteString("; VENDOR: ");
        ETH_WriteBlock(Manufacturer,sizeof(Manufacturer));
        ETH_WriteString("; SW_REV: ");
        ETH_WriteBlock(SoftwareRevision, sizeof(SoftwareRevision));
        ETH_WriteString("; MODEL: ");
        ETH_WriteBlock(ModelName, sizeof(ModelName));
        ETH_WriteString("; FW_REV: ");
        ETH_WriteBlock(FirmwareRevision, sizeof(FirmwareRevision));       
        ETH_WriteString(">>");
    }

    return built;
}

error_msg createMgmtAddrTLV(uint8_t type,uint8_t sub,uint16_t buffLen)
{
    error_msg built=ERROR;  //jira: CAE_MCU8-5647
    uint32_t ipv4add;
    
    ipv4add = ipdb_getAddress();
    swapEvenBytes(&ipv4add,4);
    
    if(AddressType==2)buffLen=24;
    else if (AddressType==1)
    {
        buffLen=12;
    }
    
    built =createBasicTLV(type,sub,buffLen);
    if(built==SUCCESS && (ETH_GetByteCount()<=MAX_TXBUFF_SIZE) )
    {
            if(AddressType==2)ETH_Write8(17);
            else ETH_Write8(5);
            
            ETH_Write8(AddressType);
            
            if(AddressType==1)ETH_WriteBlock((char *)&ipv4add,4);   // jira:M8TS-608
            ETH_Write8(0);
            ETH_Write8(0);
            ETH_Write8(0);
            ETH_Write8(0);
            ETH_Write8(0);
            ETH_Write8(0);
    }

    return built;
}


/*Tx_Timer*/
void txTimerStateMachine(void)
{
    switch(txTimerState)
    {
        case TX_TIMER_INITIALIZE:
            txTimerInitializeLLDP();
            if( LLDP_txEnabled() )
            {
               txTimerState=TX_TIMER_IDLE;
            }
            break;

        case TX_TIMER_IDLE:
            if(lldp_tx_timers.txTick)txTimerState=TX_TICK;
            if (lldp_tx_timers.txTTR==0)txTimerState=TX_TIMER_EXPIRES;
            else if(lldp_port.tx.localChange)txTimerState=SIGNAL_TX;
            else if(lldp_port.newNeighbor)txTimerState=TX_FAST_START;
            break;

        case TX_TIMER_EXPIRES:
            if(lldp_port.tx.txFast>0)lldp_port.tx.txFast--;
            txTimerState=SIGNAL_TX;
            break;

        case TX_TICK:
            clrLLDPTick();
            // Implementation of txAddCredit() as per IEEE spec:
            if(lldp_port.tx.txCredit<txCreditMax)lldp_port.tx.txCredit++;
            txTimerState=TX_TIMER_IDLE;
            break;

        case SIGNAL_TX:
            lldp_port.tx.txNow=true;
            lldp_port.tx.localChange=false;
            lldp_tx_timers.txTTR=(lldp_port.tx.txFast)? msgFastTx : msgTxInterval;
            txTimerState=TX_TIMER_IDLE;
            break;

        case TX_FAST_START:
            lldp_port.newNeighbor=false;
            if(lldp_port.tx.txFast==0)lldp_port.tx.txFast=txFastInit;
            break;

    }
}


void txTimerInitializeLLDP(void)
{
    clrLLDPTick();
    lldp_port.tx.txNow=false;
    lldp_port.tx.localChange=false;
    lldp_tx_timers.txTTR=0;
    lldp_port.tx.txFast=0;
    lldp_tx_timers.txShutdownWhile=0;
    lldp_port.newNeighbor=false;
    lldp_port.tx.txCredit= txCreditMax;
}


//Transmit State Machine

void txStateMachine(void)
{
    static uint8_t txState;

    switch (txState)
    {
        case TX_LLDP_INITIALIZE:
            if( LLDP_txEnabled() ) txState=TX_IDLE;
            break;

        case TX_IDLE:
            lldp_port.tx.txTTL = (65535<(msgTxHold*msgTxInterval)+1)? 65535:((msgTxHold*msgTxInterval)+1);
            if(lldp_port.tx.txNow && lldp_port.tx.txCredit>0)
                txState=TX_INFO_FRAME;
            else if ( LLDP_txEnabled() == 0 )
                txState=TX_SHUTDOWN_FRAME;
            break;

        case TX_INFO_FRAME:
            txFrame();
            lldp_port.tx.txCredit--;
            lldp_port.tx.txNow=false;
            txState=TX_IDLE;
            break;

        case TX_SHUTDOWN_FRAME:
            lldp_tx_timers.txShutdownWhile = reinitDelay;
            if(!lldp_tx_timers.txShutdownWhile) txState=TX_LLDP_INITIALIZE;
            break;

    }
}


void LLDP_Packet(void)
{
    if( LLDP_rxEnabled() )
    {
        rxProcessFrame();
    }
}

void txFrame(void)
{
    error_msg ret;
    mac48Address_t destMac;
    uint8_t i,orVal;
    orVal = 0;
//check to see if a Multicast destination address has been provided by the app else use the destination MAC from the last packet received
    destMac = LLDP_SetDestAddress();
    for (i=0; i<sizeof(destMac); ++i) orVal|=destMac.mac_array[i];
    if(orVal==0)destMac = genericMAC;


   if(throughTimes==0)
   {
        ret = ETH_WriteStart(&destMac,ETHERTYPE_LLDP);
   }
   if(ret==SUCCESS)
   {
        constrInfoLLDPDU();
        ret=ETH_Send();
   }

}

void constrInfoLLDPDU(void)
{
	//: Use built error check while building the packets in this area																 
    volatile uint8_t x,tableSize;
    bool built;

    //create Fixed TLVs from the Table
    const createBasicTLV_t *bhptr;
    const createOrgTLV_t *ohptr;

    throughTimes=txSizeCount=totalSize=0;// throughtimes Max=2 (only 1500 bytes in a packet)

    bhptr=lldpCallFixedTlvTable;
    for(x=0; x<BASIC_TLVS_TABLE_SIZE;x++)
    {
        if(x==(bhptr->tlvOrder)){
            built = (bool)bhptr->callTlvMaker(bhptr->type,bhptr->subtype,bhptr->buffLength);   //jira: CAE_MCU8-5647
        }
        bhptr++;
    }

    ohptr=lldpCallOrgSpecTlvTable;
    tableSize=get_org_tlvs_table_size();
	
    for(x=0; x<tableSize;x++)
    {
        if(x==(ohptr->tlvOrder))
        {
            built = (bool)ohptr->callTlvMaker(ohptr->type,ohptr->buffLength,ohptr->OUI);    //jira: CAE_MCU8-5647
        }
        ohptr++;
    }

    //Last TLV to be created at the very end always
    built=createEndTLV();
}

/***************************TLV CREATION*****************************************/

#define typeField 1
#define lengthField 2
uint8_t tl_tlvConstruct(uint8_t type, uint16_t length,uint8_t whichField)
{
    uint16_t tl;
     tl=type;
    tl=tl<<9;

    tl = tl|length;

    if(whichField==typeField)return(tl>>8);
    else if(whichField==lengthField)return tl;
    else return 0;
}

error_msg createBasicTLV(uint8_t type,uint8_t sub,uint16_t buffLen)
{
    error_msg built=ERROR;
    uint16_t tlvLen;

    if(sub)tlvLen=buffLen+1; 
    else tlvLen=buffLen;
    if (ETH_GetByteCount()<=MAX_TXBUFF_SIZE)
        { //taking into account:sub,tlvLen,oui and the actual buffer
            //Mandatory First TLV in the LLDPDU Packet

            txSizeCount=txSizeCount+tlvLen;

            ETH_Write8(tl_tlvConstruct(type,tlvLen,typeField));
            ETH_Write8(tl_tlvConstruct(type,tlvLen,lengthField));
            if(sub!=0)ETH_Write8(sub);
            built=SUCCESS;
        }
    else built=ERROR;

    return built;
}

error_msg createOrgTLV(uint8_t sub, uint16_t buffLen, uint32_t orgOUI)
{
    error_msg built=ERROR; //jira: CAE_MCU8-5647
    uint16_t tlvLen;

    tlvLen=buffLen+4;//subtype and org specific ID + payload
   if (ETH_GetByteCount()<=MAX_TXBUFF_SIZE){ //taking into account:sub,tlvLen,oui and the actual buffer
        ETH_Write8(tl_tlvConstruct(ORG_SPECIFIC_TLV,tlvLen,typeField));
        ETH_Write8(tl_tlvConstruct(ORG_SPECIFIC_TLV,tlvLen,lengthField));
        //next 3 octets contain IEE OUI
        ETH_Write24(orgOUI);
        ETH_Write8(sub);
        built=SUCCESS;
   }
   else built=ERROR;

    return built;
};


bool createEndTLV(void)
{
    bool built=false;
    txSizeCount=txSizeCount+2;

    if(ETH_GetByteCount()<=MAX_TXBUFF_SIZE)
    {
        ETH_Write8(0);
        ETH_Write8(0);
        built = true;
    }
    return built;
}


/**************************************************PROCESSING TLVs*******************************************/


void rxProcessFrame(void)
{
    uint16_t tl_tlv,tlv_length;
    uint8_t tlv_type;

    lldp_port.rx.rcvFrame=true;

    while(    lldp_port.rx.rcvFrame )
    {
        tl_tlv = ETH_Read16();//get only the first 2 bytes so now you can setup the length of the array coming next

        tlv_type= tl_tlv >>9; //Type only has 7 bits out of the 16
        tlv_length=tl_tlv & 0X01FF ; //Length carries the rest of the 9 bits

        if(tlv_type > 0 && tlv_type <= 3) processMandatory(tlv_type,tlv_length); // This can be implemented if needed
        else if (tlv_type==ORG_SPECIFIC_TLV) processSpecific(tlv_length);
        else if (tlv_type>3 &&tlv_type<9)processOptional(tlv_type,tlv_length); // This can be implemented if needed
        else if (tlv_type==END_LLDPDU_TLV)
        {
            lldpdu_end = processEnd(tlv_length);
        }
        else
        {
            processError();
        }
    }
}

void processMandatory(uint8_t tlvType, uint16_t tlvLength)
{
    static uint8_t lastState;
    mac48Address_t chassisID;
    uint8_t subType; 
    uint16_t ttl;
	
    logMsg("Processing LLDP MANDATORY Packet", LOG_INFO, (LOG_DEST_CONSOLE|LOG_DEST_ETHERNET));  	 
    switch(tlvType)
    {
        case (TLV_TYPES)CHASSIS_ID_TLV:
            lastState=(TLV_TYPES)CHASSIS_ID_TLV;
            ETH_ReadBlock(&chassisID,tlvLength);
            swapEvenBytes((uint8_t*)&chassisID,6);
            break;
        case (TLV_TYPES)PORT_ID_TLV:
            if (lastState==(TLV_TYPES)CHASSIS_ID_TLV)
            {
                lastState=(TLV_TYPES)PORT_ID_TLV;
                subType=ETH_Read8();
                if(subType==5)ETH_ReadBlock(PortInterfaceName,tlvLength-1);
                else ETH_Dump(tlvLength-1);//since we are not handling the other types

                break;
            }
            else{
                processError();
                lastState = 0;
            }
            break;
        case (TLV_TYPES)TIME_TO_LIVE_TLV:
            if (lastState==(TLV_TYPES)PORT_ID_TLV)
            {
                ttl = ETH_Read16();
                break;
            }
            else{
                processError();
                lastState = 0;
            }
            break;
    }
}

void processError(void)
{
    logMsg("Processing LLDP ERROR Packet", LOG_INFO, (LOG_DEST_CONSOLE|LOG_DEST_ETHERNET));
    lldp_port.rx.badFrame=1;
    lldp_port.rx.rcvFrame=false;
    rxState = RX_WAIT_FOR_FRAME;
}

bool processEnd(uint16_t tlvLength)
{
    logMsg("Processing END LLDP Packet", LOG_INFO, (LOG_DEST_CONSOLE|LOG_DEST_ETHERNET));
    if (tlvLength==0){
        lldp_port.rx.rcvFrame = false;
        rxState = RX_WAIT_FOR_FRAME;
        return true;
    }
    else {
        processError();
        return false;
    }
}
void processOptional(uint8_t tlvType, uint16_t tlvLength)
{
    logMsg("Processing Optional", LOG_INFO, (LOG_DEST_CONSOLE|LOG_DEST_ETHERNET));
    // We are not processing any optional TLVs right now. Code should be added for the ones that need to be processed
    // And the rest should be discarded
    ETH_Dump(tlvLength);
}

void processSpecific(uint16_t tlvLength)
{
    bool handled;
    uint32_t orgOui;
    uint8_t orgSubtype, x;

    const orgSpecificTLVs_t *hptr;

    logMsg("Processing LLDP SPECIFIC Packet", LOG_INFO, (LOG_DEST_CONSOLE|LOG_DEST_ETHERNET));
	
    hptr = lldporgProcessingTlvTable;
    handled = false;
    //OUI
    ETH_ReadBlock(&orgOui,3); // 3 bytes long
    orgOui <<=8;
    swapEvenBytes((uint8_t*)&orgOui,4);

    orgSubtype = ETH_Read8();

    for (x=0; x<= PROCESSING_TLVS_TABLE_SIZE; x++)
    {
        if (hptr->oui==orgOui && hptr->subtype == orgSubtype)
        {
            handled = (bool)hptr->processOrgTlvs(tlvLength,orgOui,orgSubtype);    //jira: CAE_MCU8-5647
        }
        hptr++;

    }
    if (!handled)
    {
        ETH_Dump(tlvLength-4);
    }

}


error_msg processCiscoPowerTlv(uint8_t len, uint32_t oui, uint8_t orgSubtype)
{
    struct{
        uint8_t  type;
        uint16_t length;
        uint32_t    OUI;
        uint8_t     subtype;
        union{
            uint8_t     capabilities;
            struct{
                unsigned                                 :4;//Reserved
                unsigned PSE_sparePairPOE_ON             :1;//1=ON, 0=OFF
                unsigned PD_req_sparePairPOE_ON          :1;//1=ON
                unsigned PD_sparePair_Arch_shared        :1;//1=Desired
                unsigned PSE_4wirePOE_supported          :1;//1=supported
            }bits;
        }capability;
        //uint8_t    capabilities;
    }cisco_4wire_poe;

    if(orgSubtype==CISCO_POWER_VIA_MDI)
    {
        logMsg("CISCO Power TLV", LOG_INFO, (LOG_DEST_CONSOLE|LOG_DEST_ETHERNET));
        cisco_4wire_poe.OUI=oui; //its orgspecific
        cisco_4wire_poe.type=127; // We already know it is an org specific TLV
        cisco_4wire_poe.length=len;//string length
        cisco_4wire_poe.subtype = orgSubtype;//Get the Subtype
        cisco_4wire_poe.capability.capabilities=ETH_Read8();
        if(cisco_4wire_poe.capability.capabilities & 0x08) orgProcFlags.poeEnabledPair=1;
        if (cisco_4wire_poe.capability.capabilities &0x01)orgProcFlags.is4WireSupported=1;	  
        return SUCCESS;
    }
    else return ERROR;
}



error_msg processIEEE3PowerTlv(uint8_t len, uint32_t oui, uint8_t orgSubtype)
{
    struct{
        uint8_t  type;
        uint16_t length;
        uint32_t    OUI;
        uint8_t     subtype;
        union{
            uint8_t     capabilities;
            struct{
                unsigned                                :4;//Reserved
                unsigned PSE_pairsControl               :1;//1=can be controlled
                unsigned PSE_MDI_powerState             :1;//1=enabled
                unsigned PSE_MDI_powerSupport           :1;//1=supported
                unsigned portClass                      :1;//1=PSE
                }bits;
            }capability;
        uint8_t     PSE_power_pair;
        uint8_t     power_class;
        union{
            uint8_t     TSPval;
            struct{
                unsigned power_type     :2;//11=Type1 PD, 10=Type1 PSE, 01=Type2 PD,00=Type2 PSE
                unsigned power_source   :2;// if power_type = PD:11=PSE&local, 10=Reserved, 01=PSE, 00=unkown
                                       // if power_type = PSE:11=Reserved, 10=Backup Src, 01=Primary pwr src,00=unkown
                unsigned                :2;
                unsigned power_priority :2;//11=low, 10=high,01=critical,00=unknown
            }bits;
        }type_src_prior;
        //uint8_t     TSPval;
        uint16_t    PD_reqd_power;
        uint16_t    PSE_allocated_power;
    }ieee_mdi_poe;
    if(orgSubtype==IEEE_3_POWER_VIA_MDI)
    {
        logMsg("IEEE Power TLV", LOG_INFO, (LOG_DEST_CONSOLE|LOG_DEST_ETHERNET));
        ieee_mdi_poe.OUI=oui;
        ieee_mdi_poe.type=127;
        ieee_mdi_poe.length=len;
        ieee_mdi_poe.subtype = orgSubtype;
        ieee_mdi_poe.capability.capabilities =ETH_Read8();
        ieee_mdi_poe.PSE_power_pair          =ETH_Read8();
        ieee_mdi_poe.power_class             =ETH_Read8();
        ieee_mdi_poe.type_src_prior.TSPval   =ETH_Read8();
        ieee_mdi_poe.PD_reqd_power=ETH_Read16();
        ieee_mdi_poe.PSE_allocated_power=ETH_Read16();

        //Keep the allocated power in record
        lldp_port.allocatedPower=ieee_mdi_poe.PSE_allocated_power;

        if(ieee_mdi_poe.PSE_allocated_power > 0x0FF)
        {
            orgProcFlags.val &= 0x01; //clear all mutually exclusive flags
            orgProcFlags.uPoeEnabledPower = 1;//full UPOE
            orgProcFlags.poeEnabledMinPower=0;
            orgProcFlags.poePlusEnabledPower=0;
        }
        else if (ieee_mdi_poe.PSE_allocated_power > 0x0082 && ieee_mdi_poe.PSE_allocated_power <= 0xFF)
        {
            orgProcFlags.val &= 0x01; //clear all mutually exclusive flags
            orgProcFlags.poePlusEnabledPower = 1; // upto 35 watts
            orgProcFlags.uPoeEnabledPower = 0;//full UPOE
            orgProcFlags.poeEnabledMinPower=0;
        }
       else if (ieee_mdi_poe.PSE_allocated_power <= 0x0082)
           {
            orgProcFlags.val &= 0x01; //clear all mutually exclusive flags
            orgProcFlags.poeEnabledMinPower = 1; // upto 15 watts
            orgProcFlags.poePlusEnabledPower = 0; // upto 35 watts
            orgProcFlags.uPoeEnabledPower = 0;//full UPOE
        }
        return SUCCESS;
   }
   else return ERROR;

}

static void swapEvenBytes(void *ptr, uint8_t len) //length has to be even
{
    uint8_t spare,x,times;
    uint8_t *buff = (uint8_t*) ptr;
    times=(uint8_t)(len>>1); //divide by 2   //jira: CAE_MCU8-5647
    for (x=0; x<times;x++)
    {
        spare = buff[x];
        buff[x]= buff[len-1-x];
        buff[len-1-x]=spare;
    }
}

void LLDP_setPortDescription(const char* val)
{
    portdescription = (char*)val;   //jira: CAE_MCU8-5647
}

char* LLDP_getPortDescription(void)
{
    return (char*)(PortDescription);  //jira: CAE_MCU8-5647
}

void LLDP_setHardwareRevision(const char* val)
{
    hardwarerev = (char*)val;    //jira: CAE_MCU8-5647
}

char* LLDP_getHardwareRevision(void)
{
    return (char*)(HardwareRevision);   //jira: CAE_MCU8-5647
}

void LLDP_setFirmwareRevision(const char* val)
{
    firmwarerev = (char*)val;  //jira: CAE_MCU8-5647
}

char* LLDP_getFirmwareRevision(void)
{
    return (char*)(FirmwareRevision);   //jira: CAE_MCU8-5647
}

void LLDP_setSoftwareRevision(const char* val)
{
    softwarerev = (char*)val;    //jira: CAE_MCU8-5647
}

char* LLDP_getSoftwareRevision(void)
{
    return (char*)(SoftwareRevision);  //jira: CAE_MCU8-5647
}

void LLDP_setSerialNumber(const char* val)
{
    serialnumber = (char*)val;   //jira: CAE_MCU8-5647
}

char* LLDP_getSerialNumber(void)
{
    return (char*)(SerialNumber);
}

void LLDP_setManufacturer(const char* val)
{
    manufacturer = (char*)val;    //jira: CAE_MCU8-5647
}

char* LLDP_getManufacturer(void)
{
    return (char*)(Manufacturer);  //jira: CAE_MCU8-5647
}

void LLDP_setModelName(const char* val)
{ 
    modelname = (char*)val;   //jira: CAE_MCU8-5647
}

char* LLDP_getModelName(void)
{
    return (char*)(ModelName);   //jira: CAE_MCU8-5647
}

void LLDP_setAssetID(const char* val)
{
    assetID = (char*)val;   //jira: CAE_MCU8-5647
}
char* LLDP_getAssetID(void)
{
    return (char*)(AssetID);   //jira: CAE_MCU8-5647
}

void LLDP_setMUDInfo(const char* val)
{
    mudext = (char*)val;    //jira: CAE_MCU8-5647
}

static error_msg setTheTLV(uint8_t sub, uint16_t buffLen, uint32_t orgOUI,char* val)
{
    error_msg built=ERROR;  //jira: CAE_MCU8-5647
    uint8_t count=0;
    char* ptr = val;

    while (*(ptr++))count++;
    ptr = val;

    built =createOrgTLV(sub,count,orgOUI);
    if(built== SUCCESS && (ETH_GetByteCount()<=MAX_TXBUFF_SIZE) )    //jira: CAE_MCU8-5647
    {
        ETH_WriteBlock(val,count);
        built = SUCCESS;
    }

    return built;  
}
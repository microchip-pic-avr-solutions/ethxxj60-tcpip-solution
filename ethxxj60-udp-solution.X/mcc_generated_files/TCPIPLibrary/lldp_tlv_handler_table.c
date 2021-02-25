/**
  LLDP protocol v4 implementation
	
  Company:
    Microchip Technology Inc.

  File Name:
 lldp_tlv_handler_table.c

  Summary:
     This is the implementation of UDP version 4 protocol

  Description:
    This source file provides the implementation of the API for the UDP v4 protocol.

 */

/*

Â©  [2015] Microchip Technology Inc. and its subsidiaries.  You may use this software 
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
#include "lldp_tlv_handler_table.h"

bool independentSparedArchitectur = true;

error_msg LLDP_createCiscoMDI   (uint8_t,uint16_t,uint32_t);
error_msg LLDP_createIeeeMDI    (uint8_t,uint16_t,uint32_t);
error_msg LLDP_createTiaMED_cap (uint8_t,uint16_t,uint32_t);
error_msg LLDP_createTiaMED     (uint8_t,uint16_t,uint32_t);
error_msg LLDP_createIeeeConfig (uint8_t,uint16_t,uint32_t);
error_msg LLDP_createTia_Net_pol     (uint8_t,uint16_t,uint32_t);
error_msg LLDP_createTia_PWR_MDI     (uint8_t,uint16_t,uint32_t);
error_msg LLDP_createTia_HW_Rev     (uint8_t,uint16_t,uint32_t);
error_msg LLDP_createTia_FW_Rev    (uint8_t,uint16_t,uint32_t);
error_msg LLDP_createTia_SW_Rev   (uint8_t,uint16_t,uint32_t);
error_msg LLDP_createTia_Srl_Num     (uint8_t,uint16_t,uint32_t);
error_msg LLDP_createTia_Manufacturer_Name     (uint8_t,uint16_t,uint32_t);
error_msg LLDP_createTia_Model_Name     (uint8_t,uint16_t,uint32_t);
error_msg LLDP_createTia_Asset_ID     (uint8_t,uint16_t,uint32_t);
error_msg LLDP_createCisco_MUD_Ext     (uint8_t,uint16_t,uint32_t);																   
error_msg LLDP_createCiscoOUI     (uint8_t,uint16_t,uint32_t);
error_msg LLDP_createCiscoClass    (uint8_t,uint16_t,uint32_t);
error_msg LLDP_createCiscoProtocol    (uint8_t,uint16_t,uint32_t);

error_msg createChassisTLV  (uint8_t,uint8_t,uint16_t);
error_msg createPortTLV     (uint8_t,uint8_t,uint16_t);
error_msg createTTLTLV      (uint8_t,uint8_t,uint16_t);
error_msg createPortDesc    (uint8_t,uint8_t,uint16_t);
error_msg createSysCap      (uint8_t,uint8_t,uint16_t);
error_msg createSystemDesc  (uint8_t,uint8_t,uint16_t);
error_msg createSysCap      (uint8_t,uint8_t,uint16_t);
error_msg createMgmtAddrTLV (uint8_t,uint8_t,uint16_t);

error_msg processCiscoPowerTlv(uint8_t, uint32_t, uint8_t);
error_msg processIEEE3PowerTlv(uint8_t, uint32_t, uint8_t);

const createBasicTLV_t lldpCallFixedTlvTable[BASIC_TLVS_TABLE_SIZE] = \
{
   //you cannot change the order of first 3  TLVs and the EndTLV should ALWAYS be the last one
 //{tlvOrder,   type,                   Subtype,                bufflen,                            functionPointer}
   {0,          CHASSIS_ID_TLV,         CHASSIS_MAC_ADDRESS,    255,                                createChassisTLV   },//CHASSIS_ID TLV
   {1,          PORT_ID_TLV,            PORT_INTERFACE_NAME,    255,                                createPortTLV      },
   {2,          TIME_TO_LIVE_TLV,           0,                  2,                                  createTTLTLV       },
   {3,          PORT_DESCRIPTION_TLV,       0,                  255,                                createPortDesc     },
   {4,          SYSTEM_CAPABILITIES_TLV,    0,                  4,                                  createSysCap       },
   {5,          SYSTEM_DESCRIPTION_TLV,     0,                  255,                                createSystemDesc   },
   {6,          MANAGEMENT_ADDRESS_TLV,     0,                  167,                                createMgmtAddrTLV  }
                                 
};


const createOrgTLV_t lldpCallOrgSpecTlvTable[] = \
{
   //you cannot change the order of first 3  TLVs and the EndTLV should ALWAYS be the last one
 //{order,   Subtype,                           bufflen,                   OUI,          ,   functionPointer}
   {0,      IEEE_3_MAC_PHY_CONFIG,                  5,                  IEEE802_3_OUI,         LLDP_createIeeeConfig},
   {1,      CISCO_POWER_VIA_MDI ,                   1,                  CISCO_OUI,             LLDP_createCiscoMDI},
   {2,      IEEE_3_POWER_VIA_MDI,                   8,                  IEEE802_3_OUI,         LLDP_createIeeeMDI},
   {3,      TIA_LLDP_MED,                           3,                  TIA_OUI,               LLDP_createTiaMED_cap},
   {4,      TIA_NETWORK_POLICY,                     4,                  TIA_OUI,               LLDP_createTia_Net_pol},
   {5,      TIA_EXTENDED_POWER_VIA_MDI,             3,                  TIA_OUI,               LLDP_createTia_PWR_MDI},
   {6,      CISCO_OUI_TLV,                          1,                 CISCO_OUI,              LLDP_createCiscoOUI},
   {7,      CISCO_CLASS_TLV,                        1,                 CISCO_OUI,              LLDP_createCiscoClass},
   {8,      CISCO_PROTOCOL_TLV,                     1,                 CISCO_OUI,              LLDP_createCiscoProtocol},
   {9,      TIA_INVENT_HW_REV,                      255,                TIA_OUI,               LLDP_createTia_HW_Rev},
   {10,     TIA_INVENT_FW_REV,                      255,                TIA_OUI,               LLDP_createTia_FW_Rev},
   {11,     TIA_INVENT_SW_REV,                      255,                TIA_OUI,               LLDP_createTia_SW_Rev},
   {12,     TIA_INVENT_SERIAL_NUM,                  255,                TIA_OUI,               LLDP_createTia_Srl_Num},
   {13,     TIA_INVENT_MANUFACT_NAME,               255,                TIA_OUI,               LLDP_createTia_Manufacturer_Name},
   {14,     TIA_INVENT_MODEL_NAME,                  255,                TIA_OUI,               LLDP_createTia_Model_Name},
   {15,     TIA_INVENT_ASSET_ID,                    255,                TIA_OUI,               LLDP_createTia_Asset_ID},
   {16,     CISCO_MUD_TLV,                          255,                CISCO_ALT_OUI,         LLDP_createCisco_MUD_Ext}
//   {13,     CISCO_OUI_TLV,                          1,                  CISCO_OUI,             LLDP_createCiscoOUI},
//   {14,     CISCO_CLASS_TLV,                        1,                  CISCO_OUI,             LLDP_createCiscoClass},
//   {15,     CISCO_PROTOCOL_TLV,                     1,                  CISCO_OUI,             LLDP_createCiscoProtocol}

};

// A table to keep up with any newly added org specific TLVs for processing purposes only
const orgSpecificTLVs_t lldporgProcessingTlvTable[2] = \
{
 //  OUI                        SUBTYPE                           Porcessing Function
  {CISCO_OUI,       (ORG_CISCO_SUBTYPE)CISCO_POWER_VIA_MDI,     processCiscoPowerTlv},
  {IEEE802_3_OUI,   (ORG_IEEE_3_SUBTYPE)IEEE_3_POWER_VIA_MDI,   processIEEE3PowerTlv}
  
};

uint8_t get_org_tlvs_table_size (void)
{
    return (sizeof(lldpCallOrgSpecTlvTable)/sizeof(*(lldpCallOrgSpecTlvTable)));
}
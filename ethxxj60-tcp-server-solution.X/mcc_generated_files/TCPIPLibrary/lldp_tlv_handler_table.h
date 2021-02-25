/* 
 * File:   lldp_tlv_handler_table.h
 * Author: c14210
 *
 * Created on March 4, 2015, 4:35 PM
 */

#ifndef LLDP_TLV_HANDLER_TABLE_H
#define	LLDP_TLV_HANDLER_TABLE_H

#include <stdint.h>
#include <stdbool.h>
#include "lldp.h"

#define CISCO_OUI                   0x000142
#define CISCO_ALT_OUI               0x00005E											
#define IEEE802_3_OUI               0x00120F
#define TIA_OUI                     0x0012BB
#define BASIC_TLVS_TABLE_SIZE        7
#define PROCESSING_TLVS_TABLE_SIZE   2

const uint8_t AddressType         = 1;

extern bool independentSparedArchitectur;										 
	
extern const orgSpecificTLVs_t lldporgProcessingTlvTable[PROCESSING_TLVS_TABLE_SIZE];
extern const createOrgTLV_t lldpCallOrgSpecTlvTable[];
extern const createBasicTLV_t lldpCallFixedTlvTable[BASIC_TLVS_TABLE_SIZE];
uint8_t get_org_tlvs_table_size (void);
//LLDP APIs that user might need to touch

void LLDP_Run(void);
void LLDP_InitRx(void);
void LLDP_InitTx(void);
void LLDP_InitRxTx(void);
void LLDP_DecTTR(void);

void LLDP_SetDesiredPower(uint16_t);
uint16_t LLDP_GetAllocatedPower(void);

void   LLDP_setPortDescription(const char* val);
char * LLDP_getInfo(char*Name);

void LLDP_setAssetID(const char* val);
char* LLDP_getAssetID(void);

void LLDP_setModelName(const char* val);
char* LLDP_getModelName(void);

void LLDP_setManufacturer(const char* val);
char* LLDP_getManufacturer(void);

void LLDP_setSerialNumber(const char* val);
char* LLDP_getSerialNumber(void);

void LLDP_setSoftwareRevision(const char* val);
char* LLDP_getSoftwareRevision(void);

void LLDP_setHardwareRevision(const char* val);
char* LLDP_getHardwareRevision(void);

void LLDP_setFirmwareRevision(const char* val);
char* LLDP_getFirmwareRevision(void);

void LLDP_setMUDInfo(const char* val);

MulticastMacAddr LLDP_SetDestAddress(void);

#endif	/* LLDP_TLV_HANDLER_TABLE_H */


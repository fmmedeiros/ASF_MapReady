/*
Implementation of pup_toNetwork.h

Orion Sky Lawlor, uiuc.edu@acm.org, 2004/3/18
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "pup.h"
#include "pup_toNetwork.h"

/****************** toNetwork ********************
*/

void PUP_toNetwork_sizer::bytes(void *p,int n,size_t itemSize,PUP::dataType t)
{
	switch (t) {
	case PUP::Tchar: //Strings and bytes get copied as-is
	case PUP::Tuchar:
	case PUP::Tbyte:
		nBytes+=n;
		break;
	case PUP::Tulong:  // longs and doubles are 8 bytes
	case PUP::Tlong:
	case PUP::Tulonglong:
	case PUP::Tlonglong: 
	case PUP::Tdouble: 
		nBytes+=n*8;
		break;
	default: //Everything else goes as a network int
		nBytes+=n*4;
		break;
	}
}

#define casesPUP_toNetwork_types \
	casePUP_toNetwork_type(Tfloat,float,float); \
	casePUP_toNetwork_type(Tdouble,double,double); \
	case PUP::Tushort: /* Fallthrough (no special treatment for unsigned) */ \
	casePUP_toNetwork_type(Tshort,short,int); \
	case PUP::Tuint:  \
	casePUP_toNetwork_type(Tint,int,int); \
       	case PUP::Tulong: \
       	casePUP_toNetwork_type(Tlong,long,PUP_NETWORK_INT8); \
       	case PUP::Tulonglong:  \
       	casePUP_toNetwork_type(Tlonglong,PUP_NETWORK_INT8,PUP_NETWORK_INT8); \
       	casePUP_toNetwork_type(Tbool,bool,int); \
       	case PUP::Tsync: break; /* ignore syncs */

void PUP_toNetwork_pack::bytes(void *p,int n,size_t itemSize,PUP::dataType t)
{
	int i;
	switch (t) {
	case PUP::Tchar: //Strings and bytes get copied as-is
	case PUP::Tuchar:
	case PUP::Tbyte:
		memcpy(buf,p,n);
		buf+=n;
		break;
#define casePUP_toNetwork_type(enumName,typeName,writeAs) \
	case PUP::enumName: \
	        for (i=0;i<n;i++) \
		  w((writeAs)( ((typeName *)p)[i] ));\
	        break
	casesPUP_toNetwork_types
#undef casePUP_toNetwork_type
	
       	default: 
       		CmiAbort("Unrecognized type passed to PUP_toNetwork_pack!\n");
       	}
}

void PUP_toNetwork_unpack::bytes(void *p,int n,size_t itemSize,PUP::dataType t)
{
	int i;
	switch (t) {
	case PUP::Tchar: //Strings and bytes get copied as-is
	case PUP::Tuchar:
	case PUP::Tbyte:
		memcpy(p,buf,n);
		buf+=n;
		break;
#define casePUP_toNetwork_type(enumName,typeName,readAs) \
	case PUP::enumName: \
	        for (i=0;i<n;i++) \
		  ((typeName *)p)[i]=(typeName)read_##readAs();\
	        break
	casesPUP_toNetwork_types
#undef casePUP_toNetwork_type
	
       	default: 
       		CmiAbort("Unrecognized type passed to PUP_toNetwork_unpack!\n");
       	}
}




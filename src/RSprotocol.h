/*
 *  RSprotocol.h : constants and macros for Rserve client/server architecture
 *  This file is based on Rserv's Rsrv.h but extracts only constants
 *  and macros
 *
 *  Copyright (C) 2002-12 Simon Urbanek
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU Lesser General Public License as published
 *  by the Free Software Foundation; version 2.1 of the License
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 *  Note: This header file is licensed under LGPL to allow other
 *        programs to use it under LGPL. Rserve itself is licensed under GPL.
 *
 *  $Id: Rsrv.h 379 2012-05-21 22:21:57Z urbanek $
 */

/* external defines:
   MAIN - should be defined in just one file that will contain the fn definitions and variables
 */

#ifndef __RSRV_H__
#define __RSRV_H__

#define default_Rsrv_port 6311

/* Rserve communication is done over any reliable connection-oriented
   protocol (usually TCP/IP or local sockets). After the connection is
   established, the server sends 32 bytes of ID-string defining the
   capabilities of the server. Each attribute of the ID-string is 4 bytes
   long and is meant to be user-readable (i.e. don't use special characters),
   and it's a good idea to make "\r\n\r\n" the last attribute

   the ID string must be of the form:

   [0] "Rsrv" - R-server ID signature
   [4] "0100" - version of the R server
   [8] "QAP1" - protocol used for communication (here Quad Attributes Packets v1)
   [12] any additional attributes follow. \r\n<space> and '-' are ignored.

   optional attributes
   (in any order; it is legitimate to put dummy attributes, like "----" or
    "    " between attributes):

   "R151" - version of R (here 1.5.1)
   "ARpt" - authorization required (here "pt"=plain text, "uc"=unix crypt,
            "m5"=MD5)
            connection will be closed if the first packet is not CMD_login.
	    if more AR.. methods are specified, then client is free to
	    use the one he supports (usually the most secure)
   "K***" - key if encoded authentification is challenged (*** is the key)
            for unix crypt the first two letters of the key are the salt
	    required by the server */

/* QAP1 transport protocol header structure

   all int and double entries throughout the transfer are in
   Intel-endianess format: int=0x12345678 -> char[4]=(0x78,0x56,x34,0x12)
   functions/macros for converting from native to protocol format 
   are available below

   Please note also that all values muse be quad-aligned, i.e. the length
   must be divisible by 4. This is automatically assured for int/double etc.,
   but care must be taken when using strings and byte streams.

 */

struct phdr { /* always 16 bytes */
	int cmd; /* command */
	int len; /* length of the packet minus header (ergo -16) */
	int dof; /* data offset behind header (ergo usually 0) */
	int res; /* high 32-bit of the packet length (since 0103
				and supported on 64-bit platforms only)
				aka "lenhi", but the name was not changed to
				maintain compatibility */
};

/* each entry in the data section (aka parameter list) is preceded by 4 bytes:
   1 byte : parameter type
   3 bytes: length
   parameter list may be terminated by 0/0/0/0 but doesn't have to since "len"
   field specifies the packet length sufficiently (hint: best method for parsing is
   to allocate len+4 bytes, set the last 4 bytes to 0 and trverse list of parameters
   until (int)0 occurs
   
   since 0102:
   if the 7-th bit (0x40) in parameter type is set then the length is encoded
   in 7 bytes enlarging the header by 4 bytes. 
 */

/* macros for handling the first int - split/combine (24-bit version only!) */
#define PAR_TYPE(X) ((X) & 255)
#define PAR_LEN(X) (((unsigned int)(X)) >> 8)
#define PAR_LENGTH PAR_LEN
#define SET_PAR(TY,LEN) ((((unsigned int) (LEN) & 0xffffff) << 8) | ((TY) & 255))

#define CMD_STAT(X) (((X) >> 24)&127) /* returns the stat code of the response */
#define SET_STAT(X,s) ((X) | (((s) & 127) << 24)) /* sets the stat code */
#define CMD_FULL(X) ((X) & 0xfffff)  /* lower 20-bit are used by the command (class + cmd) */

#define CMD_CLASS(X) ((X) & 0xf0000) /* get command class */
#define CMD_CLASS_RSERVE   0x00000   /* regular Rserve commands */
#define CMD_CLASS_RESPONSE 0x10000   /* response to Rserve commands */
#define CMD_CLASS_OOB      0x20000   /* OOB commands */
#define CMD_CLASS_PROXY    0x40000   /* proxy commands (filtered out by proxies) */

#define CMD_RESP CMD_CLASS_RESPONSE  /* all responses have this flag set */

#define RESP_OK (CMD_RESP|0x0001) /* command succeeded; returned parameters depend
				     on the command issued */
#define RESP_ERR (CMD_RESP|0x0002) /* command failed, check stats code
				      attached string may describe the error */

#define CMD_OOB             CMD_CLASS_OOB  /* out-of-band data - i.e. unsolicited messages */
#define OOB_SEND        (CMD_OOB | 0x1000) /* OOB send - unsolicited SEXP sent from the R instance to the client. 12 LSB are reserved for application-specific code */
#define OOB_MSG         (CMD_OOB | 0x2000) /* OOB message - unsolicited message sent from the R instance to the client requiring a response. 12 LSB are reserved for application-specific code */
#define OOB_STREAM_READ (CMD_OOB | 0x4000) /* OOB stream read request - server requests streaming data from the client (typically streaming input for computation) */

#define IS_OOB_SEND(X)          (((X) & 0x0ffff000) == OOB_SEND)
#define IS_OOB_MSG(X)           (((X) & 0x0ffff000) == OOB_MSG)
#define IS_OOB_STREAM_READ(X)   (((X) & 0x0ffff000) == OOB_STREAM_READ)
#define OOB_USR_CODE(X)         ((X) & 0xfff)

#define CMD_PROXY_TARGET    (CMD_CLASS_PROXY | 0x01) /* payload is NUL-terminted string defining the desired target host:port -- IPv6 addresses must be quoted in [] */
#define CMD_PROXY_GET_SLOT  (CMD_CLASS_PROXY | 0x02) /* no payload, lock-in a slot for connection-limited hosts, proxy closes connection if the target slot is unavailable at this point */

/* flag for create_server: Use QAP object-cap mode */
#define SRV_QAP_OC 0x40

/* stat codes; 0-0x3f are reserved for program specific codes - e.g. for R
   connection they correspond to the stat of Parse command.
   the following codes are returned by the Rserv itself

   codes <0 denote Rerror as provided by R_tryEval
 */
#define ERR_auth_failed      0x41 /* auth.failed or auth.requested but no
				     login came. in case of authentification
				     failure due to name/pwd mismatch,
				     server may send CMD_accessDenied instead
				  */
#define ERR_conn_broken      0x42 /* connection closed or broken packet killed it */
#define ERR_inv_cmd          0x43 /* unsupported/invalid command */
#define ERR_inv_par          0x44 /* some parameters are invalid */
#define ERR_Rerror           0x45 /* R-error occured, usually followed by
				     connection shutdown */
#define ERR_IOerror          0x46 /* I/O error */
#define ERR_notOpen          0x47 /* attempt to perform fileRead/Write 
				     on closed file */
#define ERR_accessDenied     0x48 /* this answer is also valid on
				     CMD_login; otherwise it's sent
				     if the server deosn;t allow the user
				     to issue the specified command.
				     (e.g. some server admins may block
				     file I/O operations for some users) */
#define ERR_unsupportedCmd   0x49 /* unsupported command */
#define ERR_unknownCmd       0x4a /* unknown command - the difference
				     between unsupported and unknown is that
				     unsupported commands are known to the
				     server but for some reasons (e.g.
				     platform dependent) it's not supported.
				     unknown commands are simply not recognized
				     by the server at all. */
/* The following ERR_.. exist since 1.23/0.1-6 */
#define ERR_data_overflow    0x4b /* incoming packet is too big.
				     currently there is a limit as of the
				     size of an incoming packet. */
#define ERR_object_too_big   0x4c /* the requested object is too big
				     to be transported in that way.
				     If received after CMD_eval then
				     the evaluation itself was successful.
				     optional parameter is the size of the object
				  */
/* since 1.29/0.1-9 */
#define ERR_out_of_mem       0x4d /* out of memory. the connection is usually
									 closed after this error was sent */

/* since 0.6-0 */
#define ERR_ctrl_closed      0x4e /* control pipe to the master process is closed or broken */

/* since 0.4-0 */
#define ERR_session_busy     0x50 /* session is still busy */
#define ERR_detach_failed    0x51 /* unable to detach seesion (cannot determine
									 peer IP or problems creating a listening
									 socket for resume) */
/* since 1.7 */
#define ERR_disabled         0x61 /* feature is disabled */
#define ERR_unavailable      0x62 /* feature is not present in this build */
#define ERR_cryptError       0x63 /* crypto-system error */
#define ERR_securityClose    0x64 /* server-initiated close due to security
									 violation (too many attempts, excessive
									 timeout etc.) */

/* availiable commands */

#define CMD_login        0x001 /* "name\npwd" : - */
#define CMD_voidEval     0x002 /* string : - */
#define CMD_eval         0x003 /* string : encoded SEXP */
#define CMD_shutdown     0x004 /* [admin-pwd] : - */

/* security/encryption - all since 1.7-0 */
#define CMD_switch       0x005 /* string (protocol)  : - */
#define CMD_keyReq       0x006 /* string (request) : bytestream (key) */ 
#define CMD_secLogin     0x007 /* bytestream (encrypted auth) : - */

#define CMD_OCcall       0x00f /* SEXP : SEXP  -- it is the only command
								  supported in object-capability mode
								  and it requires that the SEXP is a
								  language construct with OC reference
								  in the first position */
#define CMD_OCinit  0x434f7352 /* SEXP -- 'RsOC' - command sent from
								  the server in OC mode with the packet
								  of initial capabilities. */

/* file I/O routines. server may answe */
#define CMD_openFile     0x010 /* fn : - */
#define CMD_createFile   0x011 /* fn : - */
#define CMD_closeFile    0x012 /* - : - */
#define CMD_readFile     0x013 /* [int size] : data... ; if size not present,
				  server is free to choose any value - usually
				  it uses the size of its static buffer */
#define CMD_writeFile    0x014 /* data : - */
#define CMD_removeFile   0x015 /* fn : - */

/* object manipulation */
#define CMD_setSEXP      0x020 /* string(name), REXP : - */
#define CMD_assignSEXP   0x021 /* string(name), REXP : - ; same as setSEXP
								  except that the name is parsed */

/* session management (since 0.4-0) */
#define CMD_detachSession    0x030 /* : session key */
#define CMD_detachedVoidEval 0x031 /* string : session key; doesn't */
#define CMD_attachSession    0x032 /* session key : - */  

/* control commands (since 0.6-0) - passed on to the master process */
/* Note: currently all control commands are asychronous, i.e. RESP_OK
   indicates that the command was enqueued in the master pipe, but there
   is no guarantee that it will be processed. Moreover non-forked
   connections (e.g. the default debug setup) don't process any
   control commands until the current client connection is closed so
   the connection issuing the control command will never see its
   result.
*/
#define CMD_ctrl            0x40  /* -- not a command - just a constant -- */
#define CMD_ctrlEval        0x42  /* string : - */
#define CMD_ctrlSource      0x45  /* string : - */
#define CMD_ctrlShutdown    0x44  /* - : - */

/* 'internal' commands (since 0.1-9) */
#define CMD_setBufferSize 0x081  /* [int sendBufSize] 
				  this commad allow clients to request
				  bigger buffer sizes if large data is to be
				  transported from Rserve to the client.
				  (incoming buffer is resized automatically)
				 */
#define CMD_setEncoding   0x082  /* string (one of "native","latin1","utf8") : -; since 0.5-3 */

/* special commands - the payload of packages with this mask does not contain defined parameters */

#define CMD_SPECIAL_MASK 0xf0

#define CMD_serEval      0xf5 /* serialized eval - the packets are raw serialized data without data header */
#define CMD_serAssign    0xf6 /* serialized assign - serialized list with [[1]]=name, [[2]]=value */
#define CMD_serEEval     0xf7 /* serialized expression eval - like serEval with one additional evaluation round */

/* data types for the transport protocol (QAP1)
   do NOT confuse with XT_.. values. */

#define DT_INT        1  /* int */
#define DT_CHAR       2  /* char */
#define DT_DOUBLE     3  /* double */
#define DT_STRING     4  /* 0 terminted string */
#define DT_BYTESTREAM 5  /* stream of bytes (unlike DT_STRING may contain 0) */
#define DT_SEXP       10 /* encoded SEXP */
#define DT_ARRAY      11 /* array of objects (i.e. first 4 bytes specify how many
			    subsequent objects are part of the array; 0 is legitimate) */
#define DT_LARGE      64 /* new in 0102: if this flag is set then the length of the object
			    is coded as 56-bit integer enlarging the header by 4 bytes */

/* XpressionTypes
   REXP - R expressions are packed in the same way as command parameters
   transport format of the encoded Xpressions:
   [0] int type/len (1 byte type, 3 bytes len - same as SET_PAR)
   [4] REXP attr (if bit 8 in type is set)
   [4/8] data .. */

#define XT_NULL          0  /* P  data: [0] */
#define XT_INT           1  /* -  data: [4]int */
#define XT_DOUBLE        2  /* -  data: [8]double */
#define XT_STR           3  /* P  data: [n]char null-term. strg. */
#define XT_LANG          4  /* -  data: same as XT_LIST */
#define XT_SYM           5  /* -  data: [n]char symbol name */
#define XT_BOOL          6  /* -  data: [1]byte boolean
							     (1=TRUE, 0=FALSE, 2=NA) */
#define XT_S4            7  /* P  data: [0] */

#define XT_VECTOR        16 /* P  data: [?]REXP,REXP,.. */
#define XT_LIST          17 /* -  X head, X vals, X tag (since 0.1-5) */
#define XT_CLOS          18 /* P  X formals, X body  (closure; since 0.1-5) */
#define XT_SYMNAME       19 /* s  same as XT_STR (since 0.5) */
#define XT_LIST_NOTAG    20 /* s  same as XT_VECTOR (since 0.5) */
#define XT_LIST_TAG      21 /* P  X tag, X val, Y tag, Y val, ... (since 0.5) */
#define XT_LANG_NOTAG    22 /* s  same as XT_LIST_NOTAG (since 0.5) */
#define XT_LANG_TAG      23 /* s  same as XT_LIST_TAG (since 0.5) */
#define XT_VECTOR_EXP    26 /* s  same as XT_VECTOR (since 0.5) */
#define XT_VECTOR_STR    27 /* -  same as XT_VECTOR (since 0.5 but unused, use XT_ARRAY_STR instead) */

#define XT_ARRAY_INT     32 /* P  data: [n*4]int,int,.. */
#define XT_ARRAY_DOUBLE  33 /* P  data: [n*8]double,double,.. */
#define XT_ARRAY_STR     34 /* P  data: string,string,.. (string=byte,byte,...,0) padded with '\01' */
#define XT_ARRAY_BOOL_UA 35 /* -  data: [n]byte,byte,..  (unaligned! NOT supported anymore) */
#define XT_ARRAY_BOOL    36 /* P  data: int(n),byte,byte,... */
#define XT_RAW           37 /* P  data: int(n),byte,byte,... */
#define XT_ARRAY_CPLX    38 /* P  data: [n*16]double,double,... (Re,Im,Re,Im,...) */

#define XT_UNKNOWN       48 /* P  data: [4]int - SEXP type (as from TYPEOF(x)) */
/*                             |
                               +--- interesting flags for client implementations:
                                    P = primary type
                                    s = secondary type - its decoding is identical to
									    a primary type and thus the client doesn't need to
										decode it separately.
									- = deprecated/removed. if a client doesn't need to
									    support old Rserve versions, those can be safely
										skipped. 
  Total primary: 4 trivial types (NULL, STR, S4, UNKNOWN) + 6 array types + 3 recursive types
*/

#define XT_LARGE         64 /* new in 0102: if this flag is set then the length of the object
			       is coded as 56-bit integer enlarging the header by 4 bytes */
#define XT_HAS_ATTR      128 /* flag; if set, the following REXP is the
				attribute */
/* the use of attributes and vectors results in recursive storage of REXPs */

#define BOOL_TRUE  1
#define BOOL_FALSE 0
#define BOOL_NA    2

#define GET_XT(X) ((X)&63)
#define GET_DT(X) ((X)&63)
#define HAS_ATTR(X) (((X)&XT_HAS_ATTR)>0)
#define IS_LARGE(X) (((X)&XT_LARGE)>0)

#endif

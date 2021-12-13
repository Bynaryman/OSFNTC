/*
 * Copyright 2014,2017 International Business Machines
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "tlx_interface.h"
#include "utils.h"

#include <arpa/inet.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>



static int establish_protocol(struct AFU_EVENT *event)
{
	int bc, bl, bp, i;
	bp = 0;
	bl = 16;
	fd_set watchset;	/* fds to read from */
	uint8_t byte;
	uint32_t primary, secondary, tertiary;

	// Send protocol ID to other side of socket connection
	event->tbuf[0] = 'T';
	event->tbuf[1] = 'L';
	event->tbuf[2] = 'X';
	event->tbuf[3] = '\0';
	for (i = 0; i < 4; i++) {
		event->tbuf[4 + i] =
		    ((event->proto_primary) >> ((3 - i) * 8)) & 0xFF;
	}
	for (i = 0; i < 4; i++) {
		event->tbuf[8 + i] =
		    ((event->proto_secondary) >> ((3 - i) * 8)) & 0xFF;
	}
	for (i = 0; i < 4; i++) {
		event->tbuf[12 + i] =
		    ((event->proto_tertiary) >> ((3 - i) * 8)) & 0xFF;
	}
	while (bp < bl) {
		bc = send(event->sockfd, event->tbuf + bp, bl - bp, 0);
		if (bc < 0) {
			fprintf(stderr, "ERROR: establish_protocol: send failed: %s\n",
							strerror(errno));
			return TLX_TRANSMISSION_ERROR;
		}
		bp += bc;
	}

	// Get protocol ID from other side of socket connection
	bc = 0;
	FD_ZERO(&watchset);
	FD_SET(event->sockfd, &watchset);
	select(event->sockfd + 1, &watchset, NULL, NULL, NULL);
	while ((event->rbp < 16) && (bc != -1)) {
		if ((bc =
		     recv(event->sockfd, &(event->rbuf[event->rbp]), 1,
			  0)) == -1) {
			if (errno == EWOULDBLOCK) {
				select(event->sockfd + 1, &watchset, NULL, NULL,
				       NULL);
				continue;
			} else {
				return TLX_BAD_SOCKET;
			}
		}
		event->rbp += bc;
	}
	event->rbp = 0;

	if (strcmp((char *)event->rbuf, "TLX") != 0) {
		if (strcmp((char *)event->rbuf, "OCSE") == 0) {
			fprintf(stderr, "ERROR: establish_protocol: OCSE client attempted"
							" to connect directly, instead of relaying through the"
							" ocse server.\n");
		} else {
			fprintf(stderr, "ERROR: establish_protocol: Unrecognized protocol.\n");
		}
		return TLX_BAD_SOCKET;
	}

	primary = 0;
	for (i = 4; i < 8; i++) {
		byte = event->rbuf[i];
		primary <<= 8;
		primary += (uint32_t) byte;
	}

	secondary = 0;
	for (i = 8; i < 12; i++) {
		byte = event->rbuf[i];
		secondary <<= 8;
		secondary += (uint32_t) byte;
	}

	tertiary = 0;
	for (i = 12; i < 16; i++) {
		byte = event->rbuf[i];
		tertiary <<= 8;
		tertiary += (uint32_t) byte;
	}


	// Check for mis-matched primary level and error out if found
	if (primary != event->proto_primary) {
		printf("ERROR: Remote tlx_interface code using different TLX revision level!!\n");
		printf("\tLocal tlx_interface level:%d.%d.%d\n",
		       event->proto_primary, event->proto_secondary,
		       event->proto_tertiary);
		printf("\tRemote tlx_interface level:%d.%d.%d\n",
		       primary, secondary, tertiary);
		printf("Please check your #define setting in common/tlx_interface_t.h!!\n");
		printf("Please recompile libocl, ocse, your AFU and your application before rerunning!!\n");

		return TLX_VERSION_ERROR;
	}
	else
		return TLX_SUCCESS;


}

/* Call this at startup to reset all the event indicators */

void tlx_event_reset(struct AFU_EVENT *event)
{
	memset(event, 0, sizeof(*event));
	event->proto_primary = PROTOCOL_PRIMARY;
	event->proto_secondary = PROTOCOL_SECONDARY;
	event->proto_tertiary = PROTOCOL_TERTIARY;
}

/* Call this once after creation to initialize the AFU_EVENT structure and
 * open a socket conection to an AFU server.  This function initializes the
 * TLX side of the interface which is the client in the socket connection
 * server_host should be the name of the server hosting the simulation of
 * the AFU and port is the active port on that server. */

int tlx_init_afu_event(struct AFU_EVENT *event, char *server_host, int port)
{
	tlx_event_reset(event);
	//DO NOT set initial credit values to anything other than 0
	// AFU & ocse have to set them to valid values.
	// ocse has to WAIT until AFU sets initial value before sending first
	// config_read cmd.
	debug_msg( "tlx_init_afu_event: port=%d", port );
	event->tlx_afu_cmd_initial_credit = 0;
	event->tlx_afu_resp_initial_credit = 0;
	event->tlx_afu_cmd_data_initial_credit = 0;
	event->tlx_afu_resp_data_initial_credit = 0;
	event->afu_tlx_cmd_credits_available = 0;
	event->cfg_tlx_credits_available = 0;
	event->afu_tlx_resp_credits_available = 0;
	event->tlx_afu_credit_valid = 1;//do we really need to do this
	event->rbp = 0;

	debug_msg( "      cfg_tlx_credits_available = %d", event->cfg_tlx_credits_available );
	debug_msg( "      afu_tlx_cmd_credits_available = %d", event->afu_tlx_cmd_credits_available );
	debug_msg( "      afu_tlx_resp_credits_available = %d", event->afu_tlx_resp_credits_available );

	struct hostent *he;
	if ((he = gethostbyname(server_host)) == NULL) {
		herror("gethostbyname");
		return TLX_BAD_SOCKET;
	}
	struct sockaddr_in ssadr;
	memset(&ssadr, 0, sizeof(ssadr));
	memcpy(&ssadr.sin_addr, he->h_addr_list[0], he->h_length);
	ssadr.sin_family = AF_INET;
	ssadr.sin_port = htons(port);
	event->sockfd = socket(PF_INET, SOCK_STREAM, 0);
	if (event->sockfd == 0) {
		perror("socket");
		return TLX_BAD_SOCKET;
	}
	if (connect(event->sockfd, (struct sockaddr *)&ssadr, sizeof(ssadr)) <
	    0) {
		perror("connect");
		return TLX_BAD_SOCKET;
	}
	fcntl(event->sockfd, F_SETFL, O_NONBLOCK);

	int rc = establish_protocol(event);
	info_msg("TLX_SOCKET: Using TLX protocol level : %d.%d.%d",
	       event->proto_primary, event->proto_secondary,
	       event->proto_tertiary);

	return rc;
}

/* Call this to close the socket connection from either side */

int tlx_close_afu_event(struct AFU_EVENT *event)
{
	char buffer[4096];

	// Shutdown socket traffic
	if (shutdown(event->sockfd, SHUT_RDWR))
		return TLX_CLOSE_ERROR;

	// Drain any data in socket
	while (recv(event->sockfd, buffer, sizeof(buffer) - 1, MSG_DONTWAIT) >
	       0) ;

	// Close socket
	if (close(event->sockfd))
		return TLX_CLOSE_ERROR;
	event->sockfd = -1;

	return TLX_SUCCESS;
}

/* Call this once after creation to initialize the AFU_EVENT structure. */
/* This function initializes the AFU side of the interface which is the
 * server in the socket connection. */

int tlx_serv_afu_event(struct AFU_EVENT *event, int port)
{
	int cs = -1;
	tlx_event_reset(event);

	debug_msg("tlx_serv_afu_event: port = %d", port );

	event->rbp = 0;
	event->rdata_head = NULL;
	event->rdata_tail = NULL;
	event->rdata_rd_cnt = 0;

	//DO NOT set initial credit values to anything other than 0
	// AFU & ocse have to set them to valid values.
	// ocse has to WAIT until AFU sets initial value before sending first
	// config_read cmd.
	//event->afu_tlx_resp_initial_credit = 0;
	//event->afu_tlx_cmd_initial_credit = 0;
	//event->cfg_tlx_initial_credit = 0;
	//event->tlx_afu_resp_credits_available = 0;
	//event->tlx_afu_cmd_credits_available = 0;
	//event->tlx_afu_resp_data_credits_available = 0;
	//event->tlx_afu_cmd_credits_available = 0;
	//event->cfg_tlx_credits_available = 0;
	//event->afu_tlx_credit_req_valid = 1;//do we really need to do this

	//debug_msg( "      tlx_afu_cmd_credits_available = %d", event->tlx_afu_cmd_credits_available );
	//debug_msg( "      tlx_afu_resp_credits_available = %d", event->tlx_afu_resp_credits_available );
	//debug_msg( "      tlx_afu_resp_data_credits_available = %d", event->tlx_afu_resp_data_credits_available );

	struct sockaddr_in ssadr, csadr;
	unsigned int csalen = sizeof(csadr);
	memset(&ssadr, 0, sizeof(ssadr));
	ssadr.sin_family = AF_UNSPEC;
	ssadr.sin_addr.s_addr = INADDR_ANY;
	ssadr.sin_port = htons(port);
	event->sockfd = socket(PF_INET, SOCK_STREAM, 0);
	if (event->sockfd < 0) {
		perror("socket");
		return TLX_BAD_SOCKET;
	}
	if (bind(event->sockfd, (struct sockaddr *)&ssadr, sizeof(ssadr)) == -1) {
		perror("bind");
		tlx_close_afu_event(event);
		return TLX_BAD_SOCKET;
	}
	char hostname[1024];
	hostname[1023] = '\0';
	gethostname(hostname, 1023);
	info_msg("AFU Server is waiting for connection on %s:%d", hostname,
	       port);
	fflush(stdout);
	if (listen(event->sockfd, 10) == -1) {
		perror("listen");
		tlx_close_afu_event(event);
		return TLX_BAD_SOCKET;
	}
	while (cs < 0) {
		cs = accept(event->sockfd, (struct sockaddr *)&csadr, &csalen);
		if ((cs < 0) && (errno != EINTR)) {
			perror("accept");
			tlx_close_afu_event(event);
			return TLX_BAD_SOCKET;
		}
	}
	close(event->sockfd);
	event->sockfd = cs;
	fcntl(event->sockfd, F_SETFL, O_NONBLOCK);
	char clientname[1024];
	clientname[1023] = '\0';
	getnameinfo((struct sockaddr *)&csadr, sizeof(csadr), clientname, 1024,
		    NULL, 0, 0);
	info_msg("TLX client connection from %s", clientname);

	int rc = establish_protocol(event);
	info_msg("Using TLX protocol level : %d.%d.%d", event->proto_primary,
	       event->proto_secondary, event->proto_tertiary);

	return rc;
}

/* Call this from ocse to set the initial tlx_afu credit values */

int tlx_afu_send_initial_credits(struct AFU_EVENT *event,
				 uint8_t tlx_afu_cmd_initial_credit,
				 uint8_t tlx_afu_resp_initial_credit,
				 uint8_t tlx_afu_cmd_data_initial_credit,
				 uint8_t tlx_afu_resp_data_initial_credit)

{
        debug_msg( "tlx_afu_send_initial_credits" );

	event->tlx_afu_cmd_initial_credit = tlx_afu_cmd_initial_credit;
	event->tlx_afu_cmd_data_initial_credit = tlx_afu_cmd_data_initial_credit;
	event->tlx_afu_resp_initial_credit = tlx_afu_resp_initial_credit;
	event->tlx_afu_resp_data_initial_credit = tlx_afu_resp_data_initial_credit;
	event->tlx_afu_credit_valid = 1;

	debug_msg("      tlx_afu_cmd_initial_credit = %d", event->tlx_afu_cmd_initial_credit );
	debug_msg("      tlx_afu_resp_initial_credit = %d", event->tlx_afu_resp_initial_credit );
	debug_msg("      tlx_afu_cmd_data_initial_credit = %d", event->tlx_afu_cmd_data_initial_credit );
	debug_msg("      tlx_afu_resp_data_initial_credit = %d", event->tlx_afu_resp_data_initial_credit );
	return TLX_SUCCESS;
}


/* Call this from ocse to read the initial afu_tlx credit values */

int afu_tlx_read_initial_credits(struct AFU_EVENT *event,
		uint8_t * afu_tlx_cmd_initial_credit,
		uint8_t * cfg_tlx_initial_credit,
		uint8_t * afu_tlx_resp_initial_credit)
{
	debug_msg("afu_tlx_read_initial_credits:");

	if (!event->afu_tlx_credit_req_valid) {
	        debug_msg("      initial credits not available");
		return AFU_TLX_NO_CREDITS;
	}

	* cfg_tlx_initial_credit = event->cfg_tlx_initial_credit;
	* afu_tlx_cmd_initial_credit = event->afu_tlx_cmd_initial_credit;
	* afu_tlx_resp_initial_credit = event->afu_tlx_resp_initial_credit;

	event->cfg_tlx_credits_available = event->cfg_tlx_initial_credit;
	event->afu_tlx_cmd_credits_available = event->afu_tlx_cmd_initial_credit;
	event->afu_tlx_resp_credits_available = event->afu_tlx_resp_initial_credit;

	// printf("setting afu_tlx_credit_req_valid = 0 in afu_tlx_read_initial_credits \n");
	event->afu_tlx_credit_req_valid = 0;

	debug_msg( "      cfg_tlx_credits_available = %d", event->cfg_tlx_credits_available );
	debug_msg( "      afu_tlx_cmd_credits_available = %d", event->afu_tlx_cmd_credits_available );
	debug_msg( "      afu_tlx_resp_credits_available = %d", event->afu_tlx_resp_credits_available );

	return TLX_SUCCESS;


}




/* Call this from ocse to send a  response  to tlx/afu*/

int tlx_afu_send_resp(struct AFU_EVENT *event,
		 uint8_t tlx_resp_opcode,
		 uint16_t resp_afutag, uint8_t resp_code,
		 uint8_t resp_pg_size, uint8_t resp_dl,
#ifdef TLX4
		 uint32_t resp_host_tag, uint8_t resp_cache_state,
#endif
		 uint8_t resp_dp, uint32_t resp_addr_tag)

{
	debug_msg( "tlx_afu_send_resp: afu_tlx_resp_credits_available = %d", event->afu_tlx_resp_credits_available );
	if (event->afu_tlx_resp_credits_available == 0)
		return AFU_TLX_NO_CREDITS;
	if (event->tlx_afu_resp_valid) {
		return TLX_AFU_DOUBLE_RESP;
	} else {
		event->tlx_afu_resp_valid = 1;
	        //printf("lgt: tlx_afu_send_resp: decrementing afu_tlx_resp_credits_available fields\n");
		event->afu_tlx_resp_credits_available -= 1;
		event->tlx_afu_resp_opcode = tlx_resp_opcode;
		event->tlx_afu_resp_afutag = resp_afutag;
		event->tlx_afu_resp_code = resp_code;
		event->tlx_afu_resp_pg_size = resp_pg_size;
		event->tlx_afu_resp_dl = resp_dl;
		event->tlx_afu_resp_dp = resp_dp;
		event->tlx_afu_resp_addr_tag = resp_addr_tag;
#ifdef TLX4
		event->tlx_afu_resp_host_tag = resp_host_tag;
		event->tlx_afu_resp_cache_state = resp_cache_state;
#endif
		return TLX_SUCCESS;
	}
}


// TODO If this is still needed/used, update to include support for split responses
// needs to have resp_dp passed in to determine offset into data
int tlx_afu_send_resp_data(struct AFU_EVENT *event,
		 uint16_t resp_byte_cnt,
		 uint8_t resp_data_bdi,uint8_t * resp_data)

{

	if (event->afu_tlx_resp_rd_req == 0)
		return AFU_TLX_NO_CREDITS;
	//afu_tlx_resp_rd_cnt only valid when afu_tlx_resp_rd_req is valid
	if (resp_byte_cnt <= 64) {
		if (event->afu_tlx_resp_rd_cnt != 1)
			return AFU_TLX_RD_CNT_WRONG;
	} else if (event->afu_tlx_resp_rd_cnt != (resp_byte_cnt/64))
			return AFU_TLX_RD_CNT_WRONG;

	event->tlx_afu_resp_data_bdi = resp_data_bdi;
	memcpy(event->tlx_afu_resp_data, resp_data, resp_byte_cnt);
	event->tlx_afu_resp_data_valid = 1;
	event->tlx_afu_resp_data_byte_cnt = resp_byte_cnt;
	event->afu_tlx_resp_rd_cnt = 0;
	event->afu_tlx_resp_rd_req = 0;
	return TLX_SUCCESS;
}


/* Call this from ocse to send both response & response data to tlx/afu  */
/* This is where we create multiple resp/resp data packets w/varying dl ??
 * OR, do we expect ocse to fragment and just send us packets to pass on?
 * AND do we really need rd_req and rd_cnt in our implementation?? */

int tlx_afu_send_resp_and_data(struct AFU_EVENT *event,
		 uint8_t tlx_resp_opcode,
		 uint16_t resp_afutag, uint8_t resp_code,
		 uint8_t resp_pg_size, uint8_t resp_dl,
#ifdef TLX4
		 uint32_t resp_host_tag, uint8_t resp_cache_state,
#endif
		 uint8_t resp_dp, uint32_t resp_addr_tag,
		 uint8_t resp_data_bdi,uint8_t * resp_data)

{

	uint64_t offset;
        uint32_t size;

	debug_msg( "tlx_afu_send_resp_and_data: afu_tlx_resp_credits_available = %d", event->afu_tlx_resp_credits_available );

	if (event->afu_tlx_resp_credits_available == 0) {
	  return AFU_TLX_NO_CREDITS;
	}
	if ((event->tlx_afu_resp_valid ==1) || (event->tlx_afu_resp_data_valid == 1)) {
	  debug_msg("tlx_afu_send_resp_and_data: double resp and data");
	  return TLX_AFU_DOUBLE_RESP_AND_DATA;
	} else {
	        //printf("lgt: tlx_afu_send_resp_and_data: decrementing afu_tlx_resp_credits_available fields\n");
		event->tlx_afu_resp_valid = 1;
		event->tlx_afu_resp_data_valid = 1;
		event->afu_tlx_resp_credits_available -= 1;
		event->tlx_afu_resp_opcode = tlx_resp_opcode;
		event->tlx_afu_resp_afutag = resp_afutag;
		//	printf("lgt: tlx_afu_send_resp_and_data: resp_afutag = 0x%04x\n", event->tlx_afu_resp_afutag);
		event->tlx_afu_resp_code = resp_code;
		event->tlx_afu_resp_pg_size = resp_pg_size;
		event->tlx_afu_resp_dl = resp_dl;
		event->tlx_afu_resp_dp = resp_dp;
		event->tlx_afu_resp_addr_tag = resp_addr_tag;
#ifdef TLX4
		event->tlx_afu_resp_host_tag = resp_host_tag;
		event->tlx_afu_resp_cache_state = resp_cache_state;
#endif
		event->tlx_afu_resp_data_bdi = resp_data_bdi;
		// convert dl to size and send all the data
		// printf("lgt: tlx_afu_send_resp_and_data: including data\n");
		size = dl_to_size( resp_dl );
		//check resp_dp; if !=0 need to offset into data buffer
		offset = 0 + ((resp_dp & 0x3) * 64);
		debug_msg("tlx_afu_send_resp_and_data and offset = 0x%x", offset);
		memcpy(event->tlx_afu_resp_data, (resp_data + offset), size);
		event->tlx_afu_resp_data_byte_cnt = size;
		return TLX_SUCCESS;
	}
}


/* Call this from ocse to send an xlate_done cmd  to tlx/afu*/

int tlx_afu_send_posted_cmd(struct AFU_EVENT *event,
		 uint8_t tlx_resp_opcode,
		 uint16_t resp_afutag, uint8_t resp_code)

{
	debug_msg( "tlx_afu_send_posted_cmd: afu_tlx_resp_credits_available = %d", event->afu_tlx_resp_credits_available );
	if (event->afu_tlx_resp_credits_available == 0)
		return AFU_TLX_NO_CREDITS;
	if (event->tlx_afu_resp_valid) {
		return TLX_AFU_DOUBLE_RESP;
	} else {
		event->tlx_afu_resp_valid = 1;
		event->afu_tlx_resp_credits_available -= 1;
		event->tlx_afu_resp_opcode = tlx_resp_opcode;
		event->tlx_afu_resp_afutag = resp_afutag;
		event->tlx_afu_resp_code = resp_code;
		event->tlx_afu_resp_pg_size = 0;
		event->tlx_afu_resp_dl = 0;
		event->tlx_afu_resp_dp = 0;
		event->tlx_afu_resp_addr_tag = 0;
		return TLX_SUCCESS;
	}
}


/* Call this from ocse to send a command to tlx/afu */
/* DO NOT USE for config_wr or config_rd commands */

int tlx_afu_send_cmd(struct AFU_EVENT *event,
		 uint8_t tlx_cmd_opcode,
		 uint16_t cmd_capptag, uint8_t cmd_dl,
		 uint8_t cmd_pl, uint64_t cmd_be,
		 uint8_t cmd_end,
#ifdef TLX4
		 uint8_t cmd_os, uint8_t cmd_flag,
#endif
		 uint64_t cmd_pa)

{

	debug_msg( "tlx_afu_send_cmd: afu_tlx_cmd_credits_available = %d", event->afu_tlx_cmd_credits_available );

  	if ((tlx_cmd_opcode == TLX_CMD_CONFIG_READ) && (tlx_cmd_opcode == TLX_CMD_CONFIG_WRITE)) {
		//printf(" TRYING TO SEND CONFIG CMD w/send cmd- opcode = 0x%x \n", tlx_cmd_opcode);
		return (CFG_TLX_NOT_CFG_CMD);
	}
	if (event->tlx_afu_cmd_valid ==1)  {
		return TLX_AFU_DOUBLE_COMMAND;
	} else {
		event->afu_tlx_cmd_credits_available -= 1;
		debug_msg("      afu_tlx_cmd_credits_available is now %d", event->afu_tlx_cmd_credits_available);
		event->tlx_afu_cmd_valid = 1;
		event->tlx_afu_cmd_opcode = tlx_cmd_opcode;
		event->tlx_afu_cmd_capptag = cmd_capptag;
		event->tlx_afu_cmd_dl = cmd_dl;
		event->tlx_afu_cmd_pl = cmd_pl;
		event->tlx_afu_cmd_be = cmd_be;
		event->tlx_afu_cmd_end = cmd_end;
		event->tlx_afu_cmd_pa = cmd_pa;
#ifdef TLX4
		event->tlx_afu_cmd_flag = cmd_flag;
		event->tlx_afu_cmd_os = cmd_os;
#endif
		return TLX_SUCCESS;
	}

}

/* DO NOT USE for config_wr commands */
int tlx_afu_send_cmd_data(struct AFU_EVENT *event,
		 uint16_t cmd_byte_cnt,
		 uint8_t cmd_data_bdi,uint8_t * cmd_data)

{
	if (event->afu_tlx_cmd_rd_req == 0)
		return AFU_TLX_NO_CREDITS;
	//TODO read afu_tlx_cmd_rd_cnt eventually
	if (cmd_byte_cnt <= 64) {
		if (event->afu_tlx_cmd_rd_cnt != 1)
			return AFU_TLX_RD_CNT_WRONG;
	} else if (event->afu_tlx_cmd_rd_cnt != (cmd_byte_cnt/64))
			return AFU_TLX_RD_CNT_WRONG;
	event->tlx_afu_cmd_data_bdi = cmd_data_bdi;
	memcpy(event->tlx_afu_cmd_data_bus, cmd_data, cmd_byte_cnt);
	event->tlx_afu_cmd_data_byte_cnt = cmd_byte_cnt;
	//printf("cmd_rd_cnt is 0x%2x \n", event->afu_tlx_cmd_rd_cnt);
	event->tlx_afu_cmd_data_valid = 1;
	event->afu_tlx_cmd_rd_cnt -= 1;
	event->afu_tlx_cmd_rd_req = 0;
	return TLX_SUCCESS;
}


/* Call this from ocse to send a command to tlx/afu */
/* DO NOT USE for config_wr or config_rd commands */

int tlx_afu_send_cmd_and_data( struct AFU_EVENT *event,
			       uint8_t tlx_cmd_opcode,
			       uint16_t cmd_capptag, uint8_t cmd_dl,
			       uint8_t cmd_pl, uint64_t cmd_be,
			       uint8_t cmd_end,
#ifdef TLX4
			       uint8_t cmd_os, uint8_t cmd_flag,
#endif
			       uint64_t cmd_pa,
			       uint8_t cmd_data_bdi, uint8_t * cmd_data)

{

        int size;

	debug_msg( "tlx_afu_send_cmd_and_data: afu_tlx_cmd_credits_available = %d", event->afu_tlx_cmd_credits_available );

  	if ((tlx_cmd_opcode == TLX_CMD_CONFIG_READ) && (tlx_cmd_opcode == TLX_CMD_CONFIG_WRITE)) {
		warn_msg("tlx_afu_send_cmd_and_data: TRYING TO SEND CONFIG CMD w/send cmd- opcode = 0x%x \n", tlx_cmd_opcode);
		return CFG_TLX_NOT_CFG_CMD ;
	}
	if (event->tlx_afu_cmd_valid ==1)  {
		return TLX_AFU_DOUBLE_COMMAND;
	} else {
		event->afu_tlx_cmd_credits_available -= 1;
		debug_msg("      afu_tlx_cmd_credits_available is now %d", event->afu_tlx_cmd_credits_available);
		event->tlx_afu_cmd_valid = 1;
		event->tlx_afu_cmd_data_valid = 1;
		event->tlx_afu_cmd_opcode = tlx_cmd_opcode;
		event->tlx_afu_cmd_capptag = cmd_capptag;
		event->tlx_afu_cmd_dl = cmd_dl;
		event->tlx_afu_cmd_pl = cmd_pl;
		event->tlx_afu_cmd_be = cmd_be;
		event->tlx_afu_cmd_end = cmd_end;
		event->tlx_afu_cmd_pa = cmd_pa;
		event->tlx_afu_cmd_data_bdi = cmd_data_bdi;
		if (cmd_dl != 0) {
		  size = dl_to_size( cmd_dl ); // NO - TODO fix this
		} else {
		  size = 64;
		}
		memcpy(event->tlx_afu_cmd_data_bus, cmd_data, size);
		event->tlx_afu_cmd_data_byte_cnt = size;
#ifdef DEBUG
		printf("tlx_afu_send_cmd_and_data: data = 0x" );
		int i;
		for ( i=0; i<size; i++ ) {
		  printf( "%02x", event->tlx_afu_cmd_data_bus[i] );
		}
		printf( "\n" );
#endif
#ifdef TLX4
		event->tlx_afu_cmd_flag = cmd_flag;
		event->tlx_afu_cmd_os = cmd_os;
#endif
		return TLX_SUCCESS;
	}

}

/* CALL THIS FOR CONFIG RD/CONFIG WR ONLY */
/* This will send config rds and config writes only. It will check and
 * decrement cfg_tlx_credits_available and, for config_wr cmds with data,
 * expects a pointer to a 4B buffer with up to 4 bytes of data located at
 * appropriate address offset in that buffer. This function extracts the write
 * data, and will always send 4B data (from cmd_pl) over socket. If no data.
 * we will send filler 0xdead. If cmd_pl = any other value than 0,1 or 2 there will
 * be NO DATA transferred, just filler. We rely on afu_driver to add the one cycle delay
 * for config data when sent to AFU.
*/
int tlx_afu_send_cfg_cmd_and_data(struct AFU_EVENT *event,
		 uint8_t tlx_cfg_opcode,uint16_t cfg_capptag,
		 uint8_t cfg_pl, uint8_t cfg_t,
#ifdef TLX4
		 uint8_t cfg_os, uint8_t cfg_flag,
#endif
		 uint64_t cfg_pa,
		 uint8_t cfg_data_bdi,uint8_t * cfg_data)
{
	uint16_t cfg_data_byte_cnt;
	char fill[4] = "dead";

	debug_msg( "tlx_afu_send_cfg_cmd_and_data: cfg_tlx_credits_available = %d", event->cfg_tlx_credits_available );

	// Return non zero status if there is a pending cfg cmd (should never happen. TODO should we also check and
	// block cfg cmd if regular cmd is pending? would that ever happen?
	if (event->tlx_cfg_valid ==1)
		return TLX_AFU_DOUBLE_CMD_AND_DATA;
  	if ((tlx_cfg_opcode != TLX_CMD_CONFIG_READ) && (tlx_cfg_opcode != TLX_CMD_CONFIG_WRITE)) {
		printf(" TRYING TO SEND NON CONFIG CMD w/cfg send - opcode = 0x%x \n", tlx_cfg_opcode);
		return (CFG_TLX_NOT_CFG_CMD);
	}

	if (event->cfg_tlx_credits_available == 0) {
	  warn_msg( "      no cfg credits available");
		return CFG_TLX_NO_CREDITS;
	}

	event->cfg_tlx_credits_available -= 1;
	debug_msg("      cfg_tlx_credits available is now %d  \n", event->cfg_tlx_credits_available );
	event->tlx_cfg_valid = 1;
	event->tlx_cfg_opcode = tlx_cfg_opcode;
	event->tlx_cfg_capptag = cfg_capptag;
	event->tlx_cfg_pl = cfg_pl;
	event->tlx_cfg_t = cfg_t;
	event->tlx_cfg_pa = cfg_pa;
	if (tlx_cfg_opcode == TLX_CMD_CONFIG_WRITE) {
		switch (cfg_pl) {
			case 0: cfg_data_byte_cnt = 0;
				break;
			case 1: cfg_data_byte_cnt = 0;
				break;
			case 2: cfg_data_byte_cnt = 4;
				break;
			default: cfg_data_byte_cnt = 0;
				printf("INVALID cfg_pl is 0x%x so NO CONFIG DATA SENT\n", cfg_pl);
				break;
			}
	// TODO decide if we want to do a check and just send valid data or always send something.
	// right now, we always send something
	//	if (cfg_data_byte_cnt != 0) {
	//		event->tlx_cfg_data_valid = 1;
	//debug_msg( "tlx_afu_send_cfg_cmd_and_data: tlx_cfg_data = %d", event->tlx_cfg_data_valid );
	//event->tlx_cfg_data_byte_cnt = cfg_data_byte_cnt;
	event->tlx_cfg_data_bdi = cfg_data_bdi;
	// WE ONLY SEND 4 BYTES AS INDICATED BY CMD_PL or we send fill
	if ((tlx_cfg_opcode == TLX_CMD_CONFIG_READ) || (cfg_data_byte_cnt == 0))
		memcpy(event->tlx_cfg_data_bus, &fill, 4);
	else
		memcpy(event->tlx_cfg_data_bus, cfg_data, cfg_data_byte_cnt);
	//	}
	}
	return TLX_SUCCESS;
}

/* CALL THIS FOR CONFIG_RD/CONFIG_WR ONLY */
/* Call this from ocse to read AFU cfg response. This reads cfg_tlx_response interface (AFU to TLX AP config).
 * It expects the caller to provide a pointer to the resp_capptag to be matched with incoming responses.
 * This will read resps for config_rds and config_wr only. It will send back
 * tlx_cfg_resp_ack and, for config_rd cmds,expects a pointer to a 4B buffer so
 * up to 4 bytes of data can be copied to the appropriate address offset in that buffer.
*/

int afu_tlx_read_cfg_resp_and_data(struct AFU_EVENT *event,
		    uint8_t * cfg_resp_opcode,
		    uint16_t  * cfg_resp_capptag, uint16_t requested_capptag,
		    uint8_t * cfg_resp_data_is_valid,
		    uint8_t * cfg_resp_code, uint8_t * cfg_rdata_bus, uint8_t * cfg_rdata_bdi)

{
	if (!event->cfg_tlx_resp_valid) {

		return CFG_TLX_RESP_NOT_VALID;
	} else  if (event->cfg_tlx_resp_capptag != requested_capptag) {
		debug_msg(" CURRENT resp_capptag= 0x%x NOT A MATCH FOR REQUESTED CFG RESP CAPPTAG=0x%x \n",
			event->cfg_tlx_resp_capptag, requested_capptag);
		return (CFG_TLX_NOT_CFG_CMD);
		} else {
		debug_msg(" CURRENT resp_capptag= 0x%x  A MATCH FOR REQUESTED CFG RESP CAPPTAG=0x%x \n",
			event->cfg_tlx_resp_capptag, requested_capptag);
			event->cfg_tlx_resp_valid = 0;
			*cfg_resp_opcode = event->cfg_tlx_resp_opcode;
			debug_msg("cfg_resp_opcode = 0x%x \n", event->cfg_tlx_resp_opcode);
			*cfg_resp_capptag = event->cfg_tlx_resp_capptag;
			*cfg_resp_code = event->cfg_tlx_resp_code;
			debug_msg("afu_resp_code = 0x%x \n", event->cfg_tlx_resp_code);
			*cfg_resp_data_is_valid = 0;
			//if (event->cfg_tlx_rdata_valid) {
			if (event->cfg_tlx_resp_opcode == AFU_RSP_MEM_RD_RESP ) {
	// should we return some sort of RC other than 0 if there is no data? Should calling function be
	// smart enough to know if data is expected? Or should we set a bit to indicate that there is data
	// on the data bus? Call it data valid, BUT caller still has to check bdi? Or does bdi kill interface
	// at the TLX level??
				//event->cfg_tlx_rdata_valid = 0;
				*cfg_resp_data_is_valid = 1;
				*cfg_rdata_bdi = event->cfg_tlx_rdata_bdi;
				memcpy(cfg_rdata_bus, event->cfg_tlx_rdata_bus, 4);
				//printf("cfg_rdata_bus[0] is 0x%2x \n", cfg_rdata_bus[0]);
				//printf("cfg_rdata_bus[0] is 0x%2x \n", cfg_rdata_bus[1]);
				//printf("cfg_rdata_bus[0] is 0x%2x \n", cfg_rdata_bus[2]);
				//printf("cfg_rdata_bus[0] is 0x%2x \n", cfg_rdata_bus[3]);
				}
			event->tlx_cfg_resp_ack = 1;
			event->tlx_afu_credit_valid = 1;
			return TLX_SUCCESS;
			}
}


/* Call this from ocse to read AFU response. This reads both afu_tlx resp AND resp data interfaces */
/* DO NOT USE THIS FOR CONFIG_RD or CONFIG_WR RESPONSES */

int afu_tlx_read_resp_and_data(struct AFU_EVENT *event,
		    uint8_t * afu_resp_opcode, uint8_t * resp_dl,
		    uint16_t * resp_capptag, uint8_t * resp_dp,
		    uint8_t * resp_data_is_valid, uint8_t * resp_code, uint8_t * rdata_bus, uint8_t * rdata_bdi)

{
	if (!event->afu_tlx_resp_valid) {
		return AFU_TLX_RESP_NOT_VALID;
	} else {
		event->afu_tlx_resp_valid = 0;
		*afu_resp_opcode = event->afu_tlx_resp_opcode;
		*resp_dl = event->afu_tlx_resp_dl;
		*resp_capptag = event->afu_tlx_resp_capptag;
		*resp_dp = event->afu_tlx_resp_dp;
		*resp_code = event->afu_tlx_resp_code;
		*resp_data_is_valid = 0;
		if (event->afu_tlx_rdata_valid) {
	// should we return some sort of RC other than 0 if there is no data? Should calling function be
	// smart enough to know if data is expected? Or should we set a bit to indicate that there is data
	// on the data bus? Call it data valid, BUT caller still has to check bdi? Or does bdi kill interface
	// at the TLX level??
			event->afu_tlx_rdata_valid = 0;
			*resp_data_is_valid = 1;
			*rdata_bdi = event->afu_tlx_rdata_bdi;
			memcpy(rdata_bus, event->afu_tlx_rdata_bus, 64);
	       		event->tlx_afu_resp_data_credit = 1;
		printf("in afu_tlx_read_resp_and_data and setting tlx_afu_resp_data_credit to 1 bc we got resp data\n");
			//return TLX_SUCCESS;
			}
	//	return AFU_TLX_RESP_NO_DATA;
		printf("in afu_tlx_read_resp_and_data and setting tlx_afu_resp_credit  to 1 bc we got resp \n");
		event->tlx_afu_resp_credit = 1;
		event->tlx_afu_credit_valid = 1;
		return TLX_SUCCESS;
		}
}


/* Call this from ocse to read AFU response. This reads only the afu_tlx resp interface */
/* DO NOT USE THIS FOR CONFIG_RD or CONFIG_WR RESPONSES */

int afu_tlx_read_resp(struct AFU_EVENT *event,
		    uint8_t * afu_resp_opcode, uint8_t * resp_dl,
		    uint16_t * resp_capptag, uint8_t * resp_dp,
		    uint8_t * resp_code)

{
	if (!event->afu_tlx_resp_valid) {
		return AFU_TLX_RESP_NOT_VALID;
	} else {
		event->afu_tlx_resp_valid = 0;
		*afu_resp_opcode = event->afu_tlx_resp_opcode;
		*resp_dl = event->afu_tlx_resp_dl;
		*resp_capptag = event->afu_tlx_resp_capptag;
		*resp_dp = event->afu_tlx_resp_dp;
		*resp_code = event->afu_tlx_resp_code;

		event->tlx_afu_resp_credit = 1;
		printf("in afu_tlx_read_resp and setting tlx_afu_resp_credit to 1 bc we got resp \n");
		event->tlx_afu_credit_valid = 1;
		return TLX_SUCCESS;
		}
}


/* Call this from ocse to read AFU response. This reads only the afu_tlx resp data interface */
/* DO NOT USE THIS FOR CONFIG_RD or CONFIG_WR RESPONSES */

int afu_tlx_read_resp_data( struct AFU_EVENT *event,
			    uint8_t * resp_data_is_valid, uint8_t * rdata_bus, uint8_t * rdata_bdi)

{
       if (event->afu_tlx_rdata_valid) {
	       // should we return some sort of RC other than 0 if there is no data? Should calling function be
	       // smart enough to know if data is expected? Or should we set a bit to indicate that there is data
	       // on the data bus? Call it data valid, BUT caller still has to check bdi? Or does bdi kill interface
	       // at the TLX level??
	       event->afu_tlx_rdata_valid = 0;
	       *resp_data_is_valid = 1;
	       *rdata_bdi = event->afu_tlx_rdata_bdi;
	       memcpy(rdata_bus, event->afu_tlx_rdata_bus, 64);
	       event->tlx_afu_resp_data_credit = 1;
		printf("in afu_tlx_read_resp_data and setting tlx_afu_resp_data_credit  to 1 bc we got resp data\n");
	       event->tlx_afu_credit_valid = 1;
	       return TLX_SUCCESS;
       }
	//	return AFU_TLX_RESP_NO_DATA;
       return AFU_TLX_RESP_DATA_NOT_VALID;

}


/* Call this from ocse to read AFU command. This reads both afu_tlx cmd AND cmd data interfaces */

int afu_tlx_read_cmd_and_data(struct AFU_EVENT *event,
  		    uint8_t * afu_cmd_opcode, uint16_t * cmd_actag,
  		    uint8_t * cmd_stream_id, uint8_t * cmd_ea_or_obj,
 		    uint16_t * cmd_afutag, uint8_t * cmd_dl,
  		    uint8_t * cmd_pl,
#ifdef TLX4
		    uint8_t * cmd_os,
#endif
		    uint64_t * cmd_be, uint8_t * cmd_flag,
 		    uint8_t * cmd_endian, uint16_t * cmd_bdf,
  	  	    uint32_t * cmd_pasid, uint8_t * cmd_pg_size, uint8_t * cmd_data_is_valid,
 		    uint8_t * cdata_bus, uint8_t * cdata_bdi)

{
  // in opencapi, it is possible that the afu will send a data only event...
  // in fact, in opencapi, the data we get with a command may not be for this command
  // so,
  //    we should not return an error in that case anymore
  //    we should gather the data with a command that is waiting for it...
  // here, we will just capture the values on the command and data interfaces
        if (!event->afu_tlx_cmd_valid) {
		return AFU_TLX_CMD_NOT_VALID;
	} else {
		event->afu_tlx_cmd_valid = 0;
		*afu_cmd_opcode = event->afu_tlx_cmd_opcode;
		*cmd_actag = event->afu_tlx_cmd_actag;
		*cmd_stream_id = event->afu_tlx_cmd_stream_id;
		memcpy(cmd_ea_or_obj, event->afu_tlx_cmd_ea_or_obj,9);
		*cmd_afutag = event->afu_tlx_cmd_afutag;
		*cmd_dl = event->afu_tlx_cmd_dl;
		*cmd_pl = event->afu_tlx_cmd_pl;
#ifdef TLX4
		*cmd_os = event->afu_tlx_cmd_os;
#endif
		*cmd_be = event->afu_tlx_cmd_be;
		*cmd_flag = event->afu_tlx_cmd_flag;
		*cmd_endian = event->afu_tlx_cmd_endian;
		*cmd_bdf = event->afu_tlx_cmd_bdf;
		*cmd_pasid = event->afu_tlx_cmd_pasid;
		*cmd_pg_size = event->afu_tlx_cmd_pg_size;
		*cmd_data_is_valid = 0;
		if (event->afu_tlx_cdata_valid) {
	// should we return some sort of RC other than 0 if there is no data? Should calling function be
	// smart enough to know if data is expected? Or should we set a bit to indicate that there is data
	// on the data bus? Call it data valid, BUT caller still has to check bdi? Or does bdi kill interface
	// at the TLX level??
			event->afu_tlx_cdata_valid = 0;
			*cmd_data_is_valid = 1;
			*cdata_bdi = event->afu_tlx_cdata_bdi;
			// TODO FOR NOW WE ALWAYS COPY 64 BYTES of DATA - AFU
			// SENDS 64 BYTES
			memcpy(cdata_bus, event->afu_tlx_cdata_bus, 64);
			//return TLX_SUCCESS;
			}
		//return AFU_TLX_CMD_NO_DATA;
		return TLX_SUCCESS;
	}
}


/* Call this from ocse to read AFU command data ONLY.  */
/* DO NOT USE THIS FOR READING CONFIG DATA    */

int afu_tlx_read_cmd_data(struct AFU_EVENT *event,
  	  	    uint8_t * cmd_data_is_valid,
 		    uint8_t * cdata_bus, uint8_t * cdata_bdi)

{
  // in opencapi, it is possible that the afu will send a data only event...
  // in fact, in opencapi, the data we get with a command may not be for this command
  // so,
  //    we should not return an error in that case anymore
  //    we should gather the data with a command that is waiting for it...
  // here, we will just capture the values on the command and data interfaces
        if (event->afu_tlx_cdata_valid) {
		event->afu_tlx_cdata_valid = 0;
		*cmd_data_is_valid = 1;
		*cdata_bdi = event->afu_tlx_cdata_bdi;
		// TODO FOR NOW WE ALWAYS COPY 64 BYTES of DATA - AFU
		// SENDS 64 BYTES
		memcpy(cdata_bus, event->afu_tlx_cdata_bus, 64);
		return TLX_SUCCESS;
	} else
		*cmd_data_is_valid = 0;
		//return AFU_TLX_CMD_NO_DATA;
		return TLX_SUCCESS;

}




/* Call this to send an event to the AFU model after calling one or more of:
 * tlx_send_cmd, tlx_send_resp, tlx_send_cmd_and_data, tlx_send_resp_and_data */

int tlx_signal_afu_model(struct AFU_EVENT *event)
{
	int i, bc, bl;
	int bp = 5;
	if (event->clock != 0)
		return TLX_TRANSMISSION_ERROR;
	event->clock = 1;
	event->tbuf[0] = 0x40;
	event->tbuf[1] = 0; // reserved for tlx_afu_cmd_data_byte_cnt
	event->tbuf[2] = 0; // reserved for tlx_afu_cmd_data_byte_cnt
	event->tbuf[3] = 0; // reserved for tlx_afu_resp_data_byte_cnt
	event->tbuf[4] = 0; // reserved for tlx_afu_resp_data_byte_cnt
	// debug_msg("lgt: tlx_signal_afu_model");
	if (event->tlx_cfg_valid != 0) { //There are 18 bytes to xfer in this group (always xfer data 4B)
		event->tbuf[0] = event->tbuf[0] | 0x20;
		event->tbuf[bp++] = event->tlx_cfg_opcode;
		event->tbuf[bp++] = ((event->tlx_cfg_capptag) >> 8) & 0xFF;
		event->tbuf[bp++] = (event->tlx_cfg_capptag & 0xFF);
		//printf("event->tbuf[%x] is 0x%2x \n", bp-1, event->tbuf[bp-1]);
		for (i = 0; i < 8; i++) {
			event->tbuf[bp++] =
			    ((event->tlx_cfg_pa) >> ((7 - i) * 8)) & 0xFF;
		}
		event->tbuf[bp++] = (event->tlx_cfg_t & 0x01);
		event->tbuf[bp++] = (event->tlx_cfg_pl & 0x07);
		event->tbuf[bp++] = (event->tlx_cfg_data_bdi  & 0x01 );
		//printf("event->tbuf[bp] is 0x%2x and bp is 0x%2x \n", event->tbuf[bp], bp);
		for (i = 0; i < 4; i++) {
			event->tbuf[bp++] = event->tlx_cfg_data_bus[i];
		}
		event->tlx_cfg_valid = 0;
	}
	// debug_msg("lgt: tlx_signal_afu_model");
	if (event->tlx_afu_cmd_valid != 0) { //There are 22 bytes to xfer in this group (24 for TLX4)
		event->tbuf[0] = event->tbuf[0] | 0x10;
		//printf("event->tbuf[0] is 0x%2x \n", event->tbuf[0]);
		event->tbuf[bp++] = event->tlx_afu_cmd_opcode;
		event->tbuf[bp++] = ((event->tlx_afu_cmd_capptag) >> 8) & 0xFF;
		event->tbuf[bp++] = (event->tlx_afu_cmd_capptag & 0xFF);
		//printf("event->tbuf[%x] is 0x%2x \n", bp-1, event->tbuf[bp-1]);
		event->tbuf[bp++] = (event->tlx_afu_cmd_dl & 0x03);
		event->tbuf[bp++] = (event->tlx_afu_cmd_pl & 0x07);
		for (i = 0; i < 8; i++) {
			event->tbuf[bp++] =
			    ((event->tlx_afu_cmd_be) >> ((7 - i) * 8)) & 0xFF;
		}
		event->tbuf[bp++] = (event->tlx_afu_cmd_end & 0x01);
		//event->tbuf[bp++] = (event->tlx_afu_cmd_t & 0x01);
		for (i = 0; i < 8; i++) {
			event->tbuf[bp++] =
			    ((event->tlx_afu_cmd_pa) >> ((7 - i) * 8)) & 0xFF;
		}
#ifdef TLX4
		event->tbuf[bp++] = (event->tlx_afu_cmd_flag & 0x0f);
		event->tbuf[bp++] = (event->tlx_afu_cmd_os & 0x01);
#endif
		event->tlx_afu_cmd_valid = 0;
	}
	if (event->tlx_afu_cmd_data_valid != 0) { //There are 1 + event->tlx_afu_cmd_data_byte_cnt bytes to xfer
		event->tbuf[0] = event->tbuf[0] | 0x08;
		//printf("event->tbuf[0] is 0x%2x \n", event->tbuf[0]);
		event->tbuf[1] = ((event->tlx_afu_cmd_data_byte_cnt) >> 8) & 0x0F;
		event->tbuf[2] = (event->tlx_afu_cmd_data_byte_cnt & 0xFF);
		event->tbuf[bp++] = (event->tlx_afu_cmd_data_bdi  & 0x01 );
		//printf("event->tbuf[bp] is 0x%2x and bp is 0x%2x \n", event->tbuf[bp], bp);
		for (i = 0; i < event->tlx_afu_cmd_data_byte_cnt; i++) {
			event->tbuf[bp++] = event->tlx_afu_cmd_data_bus[i];
		}
		//printf("event->tbuf[3] is 0x%2x and bp-1 is 0x%2x \n", event->tbuf[3], bp-1);
		event->tlx_afu_cmd_data_valid = 0;
	}
	if (event->tlx_afu_resp_valid != 0) { // There are 7 bytes to transfer
		event->tbuf[0] = event->tbuf[0] | 0x04;
		event->tbuf[bp++] = event->tlx_afu_resp_opcode;
		event->tbuf[bp++] = ( event->tlx_afu_resp_afutag >> 8 ) & 0xFF ;
		event->tbuf[bp++] = ( event->tlx_afu_resp_afutag      ) & 0xFF ;
		event->tbuf[bp++] = event->tlx_afu_resp_code;
		event->tbuf[bp++] = event->tlx_afu_resp_pg_size;
		event->tbuf[bp++] = event->tlx_afu_resp_dl;
		event->tbuf[bp++] = event->tlx_afu_resp_dp;
		event->tlx_afu_resp_valid = 0;
	}
	if (event->tlx_afu_resp_data_valid != 0) { // There are 1 + tlx_afu_resp_data_byte_cnt bytes to xfer
		event->tbuf[0] = event->tbuf[0] | 0x02;
		//printf("event->tbuf[0] is 0x%2x \n", event->tbuf[0]);
		event->tbuf[3] = ((event->tlx_afu_resp_data_byte_cnt) >> 8) & 0x0F;
		event->tbuf[4] = (event->tlx_afu_resp_data_byte_cnt & 0xFF);
		event->tbuf[bp++] = (event->tlx_afu_resp_data_bdi  & 0x01 );
		//printf("event->tbuf[bp] is 0x%2x and bp is 0x%2x \n", event->tbuf[bp], bp);
		for (i = 0; i < event->tlx_afu_resp_data_byte_cnt; i++) {
			event->tbuf[bp++] = event->tlx_afu_resp_data[i];
		}
		event->tlx_afu_resp_data_valid = 0;
	}
	//Not sure what qualifies the read requests, rd counts so let's always send these, along with credit signals
	if (event->tlx_afu_credit_valid != 0) { // There are 9 bytes to xfer now with new credit scheme
	   //printf("lgt: tlx_signal_afu_model: credit valid to sent to afu\n");
	   debug_msg("lgt: tlx_signal_afu_model: cmd_initial_credit: %d, cmd_data_initial_credit:%d, resp_initial_credit: %d, resp_data_initial_credit:%d, resp_credit:%d. cmd_credit:%d, resp_data_credit:%d, cmd_data_credit:%d\n",
	          event->tlx_afu_cmd_initial_credit,
	          event->tlx_afu_cmd_data_initial_credit,
	          event->tlx_afu_resp_initial_credit,
	          event->tlx_afu_resp_data_initial_credit,
	          event->tlx_afu_resp_credit,
	          event->tlx_afu_cmd_credit,
	          event->tlx_afu_resp_data_credit,
	          event->tlx_afu_cmd_data_credit);
		event->tbuf[0] = event->tbuf[0] | 0x01;
		event->tbuf[bp++] = event->tlx_afu_cmd_initial_credit;
		event->tbuf[bp++] = event->tlx_afu_cmd_data_initial_credit;
		event->tbuf[bp++] = event->tlx_afu_resp_initial_credit;
		event->tbuf[bp++] = event->tlx_afu_resp_data_initial_credit;
		event->tbuf[bp++] = event->tlx_afu_resp_credit;
		event->tbuf[bp++] = event->tlx_afu_cmd_credit;
		event->tbuf[bp++] = event->tlx_afu_resp_data_credit;
		event->tbuf[bp++] = event->tlx_afu_cmd_data_credit;
		event->tbuf[bp++] = event->tlx_cfg_resp_ack; // this gets set in afu_tlx_read_cfg_resp_and_data
		//printf("n TLX_SIGNAL_AFU_MODEL tlx_afu_resp_credit is 0x%x \n", event->tlx_afu_resp_credit);
		//printf("tlx_afu_resp_data_credit is 0x%x \n", event->tlx_afu_resp_data_credit);
		event->tlx_afu_credit_valid = 0;
		event->tlx_afu_cmd_credit = 0;
		event->tlx_cfg_resp_ack = 0;
		event->tlx_afu_cmd_data_credit = 0;
		event->tlx_afu_resp_credit = 0;
		event->tlx_afu_resp_data_credit = 0;
	}

	// if nothing but a clock event, don't bother sending bytes 1->4
	if ( bp == 5)
		bp = 1;

#ifdef DEBUG
	// dump tbuf
        //if (event->tbuf[0] != 0x40) { // but not for just a clock
	//if (event->tbuf[0] != 0x41) { // but not for just a clock and credit return
	  if ( bp > 1 ) {
	    printf( "DEBUG:tlx_signal_afu_model: tbuf length:0x%02x tbuf: 0x", bp );
	    for ( i = 0; i < bp; i++ ) printf( "%02x", event->tbuf[i] );
	    printf( "\n" );
	  }
	//}
#endif

	bl = bp;
	bp = 0;
	while (bp < bl) {
		bc = send(event->sockfd, event->tbuf + bp, bl - bp, 0);
		if (bc < 0)
			return TLX_TRANSMISSION_ERROR;
		bp += bc;
	}
	return TLX_SUCCESS;
}

/* AFU calls this to send an event to the TLX model */
/* Now static as it's called in tlx_get_tlx_events() */

static int tlx_signal_tlx_model(struct AFU_EVENT *event)
{
	int i, bc, bl;
	int bp = 5;
	if (event->clock != 1) {
	        debug_msg("tlx_signal_tlx_events: called when clock is 0 - not supposed to be transmitting now" );
		return TLX_SUCCESS;
	}
	event->clock = 0;
	event->tbuf[0] = 0x10;
	event->tbuf[1] = 0; // reserved for afu_tlx_cmd_data_byte_cnt
	event->tbuf[2] = 0; // reserved for afu_tlx_cmd_data_byte_cnt
	event->tbuf[3] = 0; // reserved for afu_tlx_resp_data_byte_cnt
	event->tbuf[4] = 0; // reserved for afu_tlx_resp_data_byte_cnt

	debug_msg("txl_signal_tlx_model");
	if (event->afu_tlx_cmd_valid != 0) { //There are 34 bytes to xfer in this group (35 for TLX4 )
		//printf("      adding afu_tlx_cmd\n");
		event->tbuf[0] = event->tbuf[0] | 0x02;
		// printf("event->tbuf[0] is 0x%2x \n", event->tbuf[0]);
		event->tbuf[bp++] = event->afu_tlx_cmd_opcode;
		event->tbuf[bp++] = ((event->afu_tlx_cmd_actag) >> 8) & 0xFF;
		event->tbuf[bp++] = (event->afu_tlx_cmd_actag & 0xFF);
		//printf("event->tbuf[%x] is 0x%2x \n", bp-1, event->tbuf[bp-1]);
		event->tbuf[bp++] = (event->afu_tlx_cmd_stream_id & 0x0f);
		for (i = 0; i < 9; i++) {
			event->tbuf[bp++] = event->afu_tlx_cmd_ea_or_obj[i];
		}
		event->tbuf[bp++] = ((event->afu_tlx_cmd_afutag) >> 8) & 0xFF;
		event->tbuf[bp++] = (event->afu_tlx_cmd_afutag & 0xFF);
		event->tbuf[bp++] = (event->afu_tlx_cmd_dl & 0x03);
		event->tbuf[bp++] = (event->afu_tlx_cmd_pl & 0x07);
		//printf("event->tbuf[%x] is 0x%2x \n", bp-1, event->tbuf[bp-1]);
#ifdef TLX4
		event->tbuf[bp++] = (event->afu_tlx_cmd_os & 0x01);
#endif
		for (i = 0; i < 8; i++) {
			event->tbuf[bp++] =
			    ((event->afu_tlx_cmd_be) >> ((7 - i) * 8)) & 0xFF;
		}
		event->tbuf[bp++] = (event->afu_tlx_cmd_flag & 0x0f);
		event->tbuf[bp++] = (event->afu_tlx_cmd_endian & 0x01);
		event->tbuf[bp++] = ((event->afu_tlx_cmd_bdf) >> 8) & 0xFF;
		event->tbuf[bp++] = (event->afu_tlx_cmd_bdf & 0xFF);
		for (i = 0; i < 4; i++) {
			event->tbuf[bp++] =
			    ((event->afu_tlx_cmd_pasid) >> ((3 - i) * 8)) & 0xFF;
		}
		event->tbuf[bp++] = (event->afu_tlx_cmd_pg_size & 0xFF);
		//printf("event->tbuf[%x] is 0x%2x  \n", bp-1, event->tbuf[bp-1]);
		event->afu_tlx_cmd_valid = 0;
	}
	if (event->afu_tlx_cdata_valid != 0) { //There are 65  bytes to xfer
		//printf("      adding afu_tlx_cmd_data\n");
		event->tbuf[0] = event->tbuf[0] | 0x04;
		event->tbuf[1] = 0;
		event->tbuf[2] = 64;// IF AFU CMD data byte count ever changes, we can use this for real
		//printf("event->tbuf[0] is 0x%2x \n", event->tbuf[0]);
		event->tbuf[bp++] = (event->afu_tlx_cdata_bdi  & 0x01 );
		//printf("event->tbuf[bp] is 0x%2x and bp is 0x%2x \n", event->tbuf[bp], bp);
		for (i = 0; i < 64; i++) {
			event->tbuf[bp++] = event->afu_tlx_cdata_bus[i];
		}
		//printf("event->tbuf[3] is 0x%2x and bp-1 is 0x%2x \n", event->tbuf[3], bp-1);
		event->afu_tlx_cdata_valid = 0;
	}
	if (event->afu_tlx_resp_valid != 0) { //There are 6 bytes to xfer in this group
		//printf("      adding afu_tlx_resp\n");
		event->tbuf[0] = event->tbuf[0] | 0x08;
		event->tbuf[bp++] = event->afu_tlx_resp_opcode;
		event->tbuf[bp++] = (event->afu_tlx_resp_dl & 0x03);
		event->tbuf[bp++] = ((event->afu_tlx_resp_capptag) >> 8) & 0xFF;
		event->tbuf[bp++] = (event->afu_tlx_resp_capptag & 0xFF);
		event->tbuf[bp++] = (event->afu_tlx_resp_dp & 0x03);
		//printf("event->tbuf[%x] is 0x%2x \n", bp-1, event->tbuf[bp-1]);
		event->tbuf[bp++] = (event->afu_tlx_resp_code & 0x0f);
		event->afu_tlx_resp_valid = 0;
	}
	if (event->afu_tlx_rdata_valid != 0) { // There are 65 bytes to xfer
		//printf("      adding afu_tlx_resp_data\n");
		event->tbuf[0] = event->tbuf[0] | 0x20;
		event->tbuf[3] = 0;
		event->tbuf[4] = 64;// IF AFU RESP data byte count ever changes, we can use this for real
		//printf("event->tbuf[0] is 0x%2x \n", event->tbuf[0]);
		event->tbuf[bp++] = (event->afu_tlx_rdata_bdi  & 0x01 );
		//printf("event->tbuf[%x] is 0x%2x \n", bp-1, event->tbuf[bp-1]);
		for (i = 0; i < 64; i++) {
			event->tbuf[bp++] = event->afu_tlx_rdata_bus[i];
		//printf("event->tbuf[%x] is 0x%2x \n", bp-1, event->tbuf[bp-1]);
		}
		event->afu_tlx_rdata_valid = 0;
	}
	if (event->cfg_tlx_resp_valid != 0) { // There are 9 bytes to xfer - always xfer 4B data
		//printf("      adding cfg_tlx_resp_data\n");
		event->tbuf[0] = event->tbuf[0] | 0x40;
		event->tbuf[bp++] = event->cfg_tlx_resp_opcode;
		event->tbuf[bp++] = ((event->cfg_tlx_resp_capptag) >> 8) & 0xFF;
		event->tbuf[bp++] = (event->cfg_tlx_resp_capptag & 0xFF);
		event->tbuf[bp++] = (event->cfg_tlx_resp_code & 0x0f);
		//printf("event->tbuf[0] is 0x%2x \n", event->tbuf[0]);
		event->tbuf[bp++] = (event->cfg_tlx_rdata_bdi  & 0x01 );
		//printf("event->tbuf[%x] is 0x%2x \n", bp-1, event->tbuf[bp-1]);
		for (i = 0; i < 4; i++) {
			event->tbuf[bp++] = event->cfg_tlx_rdata_bus[i];
		//printf("event->tbuf[%x] is 0x%2x \n", bp-1, event->tbuf[bp-1]);
		}
		event->cfg_tlx_resp_valid = 0;
	}
	//Not sure what qualifies the read requests, rd counts so let's always send these, along with credit signals
	if (event->afu_tlx_credit_req_valid != 0) { // There are 10 bytes to xfer
		event->tbuf[0] = event->tbuf[0] | 0x01;
		event->tbuf[bp++] = event->afu_tlx_resp_initial_credit;
		event->tbuf[bp++] = event->afu_tlx_cmd_initial_credit;
		event->tbuf[bp++] = event->cfg_tlx_initial_credit;
		event->tbuf[bp++] = event->afu_tlx_resp_credit;
		event->tbuf[bp++] = event->afu_tlx_cmd_credit;
		event->tbuf[bp++] = event->cfg_tlx_credit_return;
		event->tbuf[bp++] = event->afu_tlx_resp_rd_req;
		event->tbuf[bp++] = event->afu_tlx_resp_rd_cnt;
		event->tbuf[bp++] = event->afu_tlx_cmd_rd_req;
		event->tbuf[bp++] = event->afu_tlx_cmd_rd_cnt;
		if (event->afu_tlx_cmd_credit == 1)
			debug_msg("TLX_SIGNAL_TLX_MODEL SETTING afu_tlx_cmd_credit = 1");
		if (event->afu_tlx_resp_credit == 1)
			debug_msg("TLX_SIGNAL_TLX_MODEL SETTING afu_tlx_resp_credit = 1");
		debug_msg("tlx_signal_tlx_model: setting afu_tlx_credit_req_valid=0 after putting in tbuf");
		event->afu_tlx_credit_req_valid = 0;
		event->afu_tlx_cmd_credit = 0;
		event->cfg_tlx_credit_return= 0;
		event->afu_tlx_resp_credit = 0;
	} else {
	        debug_msg("tlx_signal_tlx_model: no (initial) credits to send");
	}

	// if nothing but a clock event, don't bother sending bytes 1->4
	if ( bp == 5)
		bp = 1;

#ifdef DEBUG
        // dump tbuf
        // if (event->tbuf[0] != 0x10) { // but not for just a clock
	// if (event->tbuf[0] != 0x11) { // but not for just a clock and credit return
	  if ( bp > 1 ) {
	    printf( "DEBUG:tlx_signal_tlx_model: tbuf length:0x%02x tbuf: 0x", bp );
	    for ( i = 0; i < bp; i++ ) printf( "%02x", event->tbuf[i] );
	    printf( "\n" );
	  }
        // }
#endif

	bl = bp;
	bp = 0;
	while (bp < bl) {
		bc = send(event->sockfd, event->tbuf + bp, bl - bp, 0);
		if (bc < 0) {
		  warn_msg("tlx_signal_tlx_model: transmissson error");
		  return TLX_TRANSMISSION_ERROR; }
		bp += bc;
	}
	return TLX_SUCCESS;
}

/* This function checks the socket connection for data from the external AFU
 * simulator. It needs to be called periodically to poll the socket connection.
 * It will update the AFU_EVENT structure.
 * It returns a 1 if there are new events to process, 0 if not, -1 on error or
 * close.  On a 1 return, the following functions should be called to retrieve
 * the individual event:
 * afu_tlx_read_cmd
 * afu_tlx_read_resp
 */

int tlx_get_afu_events(struct AFU_EVENT *event)
{
	int i = 0;
	int bc = 0;
	uint32_t rbc = 1;
	uint16_t cmd_data_byte_cnt, resp_data_byte_cnt;
	cmd_data_byte_cnt = 0;
	resp_data_byte_cnt = 0;
	fd_set watchset;
	FD_ZERO(&watchset);
	FD_SET(event->sockfd, &watchset);
	select(event->sockfd + 1, &watchset, NULL, NULL, NULL);
        debug_msg("tlx_get_afu_events:");
	if (event->rbp == 0) {
		if ((bc = recv(event->sockfd, event->rbuf, 1, 0)) == -1) {
			if (errno == EWOULDBLOCK) {
			        debug_msg("tlx_get_afu_events: nothing in socket?");
				return 0;
			} else {
				return -1;
			}
		}
	if (bc == 0)
		return -1;
		event->rbp += bc;
	}
	if (event->rbp != 0) {
		if ((event->rbuf[0] & 0x10) != 0) {
			event->clock = 0;
			if (event->rbuf[0] == 0x10) {
				debug_msg("tlx_get_afu_events: Just a clock event");
				event->rbp = 0;
				return 1;
			}
		}

		// read bytes 1->4; 1&2 are cmd_data_byte_cnt...3&4 are resp_data_byte_cnt
		if ( ( bc = recv( event->sockfd, event->rbuf + event->rbp, 4, 0 ) ) == -1 ) {
			if (errno == EWOULDBLOCK) {
			  warn_msg("tlx_get_afu_events: not enough byte count data on socket");
   			        // there is not enough on the socket
				return 0;
			} else {
 			  warn_msg("tlx_get_afu_events: bad socket");
   			        // something bad happened to the socket
				return -1;
			}
		}
		// printf("tlx_get_afu_events: read bc = 0x%04x: more bytes from rbuf\n", bc );
		if ( bc == 0 ) {
		        warn_msg("tlx_get_afu_events: bc = 0 after trying to reading data sizes from rbuf" );
			return -1;
		}
		// printf("tlx_get_afu_events: bc = 0x%04x: more bytes in rbuf\n", bc );

		event->rbp += bc;
		rbc += 4;  // account for those extra bytes



//printf("TLX_GET_AFU_EVENT-1 - rbuf[0] is 0x%02x and e->rbp = %2d  \n", event->rbuf[0], event->rbp);
		if ((event->rbuf[0] & 0x20) != 0) {
		        debug_msg("      extract resp data byte count");
			resp_data_byte_cnt = event->rbuf[3];
			resp_data_byte_cnt = ((resp_data_byte_cnt << 8) | event->rbuf[4]);
			rbc += resp_data_byte_cnt;
			rbc += 1; // add bdi byte
			//printf("TLX_GET_AFU_EVENT-0x20 - rbuf[0] is 0x%02x and rbc = %2d  \n", event->rbuf[0], rbc);
			//rbc += 65; //TODO for now, resp data always 65B total
			}
		if ((event->rbuf[0] & 0x08) != 0) {
		        debug_msg("      extract resp count");
			rbc += 6; // resp always 6B
			//printf("TLX_GET_AFU_EVENT-0x08 - rbuf[0] is 0x%02x and rbc = %2d  \n", event->rbuf[0], rbc);
		}
	 	if ((event->rbuf[0] & 0x04) != 0) {
		        debug_msg("      extract cmd data byte count");
			cmd_data_byte_cnt = event->rbuf[1];
			cmd_data_byte_cnt = ((cmd_data_byte_cnt << 8) | event->rbuf[2]);
			rbc += cmd_data_byte_cnt;
			rbc += 1; //add bdi byte
			//rbc += 65; //TODO for now, cmd data always 65 total
			}
		if ((event->rbuf[0] & 0x02) != 0) {
		        debug_msg("      extract cmd byte count");
			rbc += 34; // for TLX4 cmds, value will increase by 1
		}
		if ((event->rbuf[0] & 0x01) != 0) {
		  // debug_msg("      extract credit byte count");
			rbc += 10; // for now, always copy over everything
		}
		if ((event->rbuf[0] & 0x40) != 0) {
		  // debug_msg("      extract cfg byte count");
			rbc += 9; // for now, always copy over everything
		}

		//printf("TLX_GET_AFU_EVENT-2 - rbuf[0] is 0x%02x and rbc is %2d \n", event->rbuf[0], rbc);
		if ( ( bc = recv( event->sockfd, event->rbuf + event->rbp, rbc - event->rbp, 0 ) ) == -1) {
		  if (errno == EWOULDBLOCK) {
		    warn_msg("tlx_get_afu_events: not enough data on socket");
		    return 0;
		  } else {
		    warn_msg("tlx_get_afu_events: bad socket");
		    return -1;
		  }
		}

		if (bc == 0) return -1;

		event->rbp += bc;
	} //should be here but count is off

	if (event->rbp < rbc) {
		//printf("exiting bc event->rbp = 0x%x and rbc= 0x%x \n",
		//	event->rbp, rbc);
		return 0;
	}

#ifdef DEBUG
	// dump rbuf
	// if (event->rbuf[0]  != 0x10) { // except when just a clock
	// if (event->rbuf[0]  != 0x11) { // except when just a clock and credit return
		printf( "DEBUG:tlx_get_afu_events: rbuf length:0x%02x rbuf: 0x", rbc );
		for ( i = 0; i < rbc; i++ ) printf( "%02x", event->rbuf[i] );
		printf( "\n" );
	// }
#endif

	//rbc = 1;
	rbc = 5;
	if ((event->rbuf[0] & 0x02) != 0) {
	        debug_msg("      parsing afu tlx cmd");
		event->afu_tlx_cmd_valid = 1;
		//printf("event->afu_tlx_cmd_valid is 1  and rbc is 0x%2x \n", rbc);
		event->tlx_afu_cmd_credit = 1;
		event->tlx_afu_credit_valid = 1;
		debug_msg("TLX_GET_AFU_EVENTS setting tlx_afu_cmd_credit = 1");

		//printf("event->rbuf[%x] is 0x%2x \n", rbc, event->rbuf[rbc]);
		event->afu_tlx_cmd_opcode = event->rbuf[rbc++];
		event->afu_tlx_cmd_actag = event->rbuf[rbc++];
		//printf("event->rbuf[%x] is 0x%2x \n", rbc-1, event->rbuf[rbc-1]);
		event->afu_tlx_cmd_actag = ((event->afu_tlx_cmd_actag << 8) | event->rbuf[rbc++]);
		event->afu_tlx_cmd_stream_id = event->rbuf[rbc++];
		for (i = 0; i < 9; i++) {
			event->afu_tlx_cmd_ea_or_obj[i] = event->rbuf[rbc++];
		}
		event->afu_tlx_cmd_afutag = event->rbuf[rbc++];
		event->afu_tlx_cmd_afutag = ((event->afu_tlx_cmd_afutag << 8) | event->rbuf[rbc++]);;
		//printf("event->rbuf[%x] is 0x%2x \n", rbc-1, event->rbuf[rbc-1]);
		event->afu_tlx_cmd_dl = event->rbuf[rbc++];
		event->afu_tlx_cmd_pl = event->rbuf[rbc++];
		//printf("event->rbuf[%x] is 0x%2x \n", rbc-1, event->rbuf[rbc-1]);
#ifdef TLX4
		event->afu_tlx_cmd_os = event->rbuf[rbc++];;
#endif
		event->afu_tlx_cmd_be = 0;
		for (bc = 0; bc < 8; bc++) {
			event->afu_tlx_cmd_be  =
			    ((event->afu_tlx_cmd_be) << 8) |
			    event->rbuf[rbc++];
		}
		event->afu_tlx_cmd_flag = event->rbuf[rbc++];
		event->afu_tlx_cmd_endian = event->rbuf[rbc++];
		event->afu_tlx_cmd_bdf = event->rbuf[rbc++];
		event->afu_tlx_cmd_bdf = ((event->afu_tlx_cmd_bdf << 8) | event->rbuf[rbc++]);;
		event->afu_tlx_cmd_pasid = 0;
		for (bc = 0; bc < 4; bc++) {
			event->afu_tlx_cmd_pasid  =
			    ((event->afu_tlx_cmd_pasid) << 8) |
			    event->rbuf[rbc++];
		}
		event->afu_tlx_cmd_pg_size = event->rbuf[rbc++];

	} else {
		event->afu_tlx_cmd_valid = 0;
		event->tlx_afu_cmd_credit = 0;
	}

	if ((event->rbuf[0] & 0x04) != 0) {
	        debug_msg("      parsing afu tlx cmd data");
		event->afu_tlx_cdata_valid = 1;
	        debug_msg("TLX_GET_AFU_EVENTS SETTING afu tlx cdata valid");
		event->tlx_afu_cmd_data_credit = 1;
		event->tlx_afu_credit_valid = 1;
		event->afu_tlx_cdata_bdi = event->rbuf[rbc++] ;
		//printf("event->rbuf[%x] is 0x%2x \n", rbc-1, event->rbuf[rbc-1]);
		for (i = 0; i < cmd_data_byte_cnt; i++) {
			event->afu_tlx_cdata_bus[i] = event->rbuf[rbc++] ;
		}
		// printf( "tlx_get_afu_events:event->afu_tlx_cdata_bus=0x" ); for ( i = 0; i < 64; i++ ) printf("%02x",event->afu_tlx_cdata_bus[i]); printf( "\n" );
	} else {
		event->afu_tlx_cdata_valid = 0;
		event->tlx_afu_cmd_data_credit = 0;
	}
	if ((event->rbuf[0] & 0x08) != 0) {
	        debug_msg("      parsing afu tlx resp");
		event->afu_tlx_resp_valid = 1;
		// event->tlx_afu_resp_credit = 1;  // allow ocse to process the response and return the credit
		// event->tlx_afu_credit_valid = 1;
		printf("in tlx_get_afu_events and NOT setting tlx_afu_credit & resp credits to 1 bc we got resp \n");
		event->afu_tlx_resp_opcode = event->rbuf[rbc++];
		event->afu_tlx_resp_dl = event->rbuf[rbc++];
		event->afu_tlx_resp_capptag = event->rbuf[rbc++];
		event->afu_tlx_resp_capptag = ((event->afu_tlx_resp_capptag << 8) | event->rbuf[rbc++]);;
		//printf("event->rbuf[%x] is 0x%2x \n", rbc-1, event->rbuf[rbc-1]);
		event->afu_tlx_resp_dp = event->rbuf[rbc++];
		event->afu_tlx_resp_code = event->rbuf[rbc++];
	} else {
		event->afu_tlx_resp_valid = 0;
		event->tlx_afu_resp_credit = 0;  
	}
	if ((event->rbuf[0] & 0x20) != 0) {
	        debug_msg("            cmd resp data");
		event->afu_tlx_rdata_valid = 1;
		//event->tlx_afu_resp_data_credit = 1;
		//event->tlx_afu_credit_valid = 1;
		printf("in tlx_get_afu_events and NOT setting tlx_afu_credit & resp_data credits to 1 bc we got resp data\n");
		event->afu_tlx_rdata_bdi= event->rbuf[rbc++];
		//printf("event->rbuf[%x] is 0x%2x \n", rbc-1, event->rbuf[rbc-1]);
		for (i = 0; i < 64; i++) {
			event->afu_tlx_rdata_bus[i]= event->rbuf[rbc++];
		//printf("event->rbuf[%x] is 0x%2x \n", rbc-1, event->rbuf[rbc-1]);
		//printf("event->afu_tlx_rdata_bus[%x] is 0x%2x \n", i, event->afu_tlx_rdata_bus[i]);
		}
	//	}

	} else {
		event->afu_tlx_rdata_valid = 0;
		//printf("in tlx_get_afu_events and setting tlx_afu_resp & resp_data credits to 0\n");
		event->tlx_afu_resp_data_credit = 0;
		}

	if ((event->rbuf[0] & 0x40) != 0) {
	        debug_msg("      parsing cfg tlx resp and data");
		event->cfg_tlx_resp_valid = 1;
		event->cfg_tlx_resp_opcode = event->rbuf[rbc++];
		event->cfg_tlx_resp_capptag = event->rbuf[rbc++];
		event->cfg_tlx_resp_capptag = ((event->cfg_tlx_resp_capptag << 8) | event->rbuf[rbc++]);;
		//printf("event->rbuf[%x] is 0x%2x \n", rbc-1, event->rbuf[rbc-1]);
		event->cfg_tlx_resp_code = event->rbuf[rbc++];
		event->cfg_tlx_rdata_bdi = event->rbuf[rbc++];
		for (i = 0; i < 4; i++) {
			event->cfg_tlx_rdata_bus[i]= event->rbuf[rbc++];
		//printf("event->rbuf[%x] is 0x%2x \n", rbc-1, event->rbuf[rbc-1]);
		//printf("event->tlx_cfg_rdata_bus[%x] is 0x%2x \n", i, event->cfg_tlx_rdata_bus[i]);
		 }

	} else
		event->cfg_tlx_resp_valid = 0;

	if ((event->rbuf[0] & 0x01) != 0) {
	  // debug_msg("      credit processing:");
	        event->afu_tlx_credit_req_valid = 1;
		//printf("JUST SET afu_tlx_credit_req_valid = 1 in tlx_get_afu_model \n");
		event->afu_tlx_resp_initial_credit = event->rbuf[rbc++];
		event->afu_tlx_cmd_initial_credit = event->rbuf[rbc++];
		event->cfg_tlx_initial_credit = event->rbuf[rbc++];
		event->afu_tlx_resp_credit = event->rbuf[rbc++];
		event->afu_tlx_cmd_credit = event->rbuf[rbc++];
		event->cfg_tlx_credit_return = event->rbuf[rbc++];
		event->afu_tlx_resp_rd_req = event->rbuf[rbc++];
		event->afu_tlx_resp_rd_cnt = event->rbuf[rbc++];
		event->afu_tlx_cmd_rd_req = event->rbuf[rbc++];
		if (event->afu_tlx_cmd_rd_req != 0)
			debug_msg("TLX_GET_AFU_EVENTS afu_tlx_cmd_rd_req = 0x%x", event->afu_tlx_cmd_rd_req);
		event->afu_tlx_cmd_rd_cnt = event->rbuf[rbc++];
		if (event->afu_tlx_cmd_rd_cnt != 0)
			debug_msg("TLX_GET_AFU_EVENTS afu_tlx_cmd_rd_cnt = 0x%x", event->afu_tlx_cmd_rd_cnt);;

		if (event->afu_tlx_cmd_credit == 1) {
			event->afu_tlx_cmd_credits_available += 1;
			event->afu_tlx_cmd_credit = 0;
			debug_msg("      incremented afu_tlx_cmd_credits_available to %d", event->afu_tlx_cmd_credits_available);
		}
		if (event->cfg_tlx_credit_return == 1) {
			event->cfg_tlx_credits_available += 1;
			event->cfg_tlx_credit_return = 0;
			debug_msg("      incremented cfg_tlx_credits_available to %d", event->cfg_tlx_credits_available);
		}
		if (event->afu_tlx_resp_credit == 1) {
			event->afu_tlx_resp_credits_available += 1;
			event->afu_tlx_resp_credit = 0;
			debug_msg("      incremented afu_tlx_resp_credits_available to %d", event->afu_tlx_resp_credits_available);
		}
	} else {
		event->afu_tlx_credit_req_valid = 0;
		debug_msg("tlx_get_afu_events: setting afu_tlx_credit_req_valid=0 after processing");
		}

	event->rbp = 0;
	return 1;
}

/* This function checks the socket connection for data from the external TLX
 * simulator. It needs to be called periodically to poll the socket connection.
 * (every clock cycle)  It will update the AFU_EVENT structure and returns a 1
 * if there are new events to process. */

int tlx_get_tlx_events(struct AFU_EVENT *event)
{
        int bc, i;
	uint32_t rbc = 1;
	uint16_t cmd_data_byte_cnt, resp_data_byte_cnt;
	cmd_data_byte_cnt = 0;
	resp_data_byte_cnt = 0;
	debug_msg("tlx_get_tlx_events: entered" );
	if (event->rbp == 0) {
		if ((bc = recv(event->sockfd, event->rbuf, 1, 0)) == -1) {
			if (errno == EWOULDBLOCK) {
    			        // there is nothing on the socket
			  warn_msg("tlx_get_tlx_events: not enough byte count data on socket");
 				return 0;
			} else {
 			  warn_msg("tlx_get_tlx_events: bad socket");
    			        // something bad happened to the socket
				return -1;
			}
		}
		if (bc == 0) {
		         debug_msg("tlx_get_tlx_events: bc = 0, leaving with -1" );
			return -1;
		}
		event->rbp += bc;
	}
	if (event->rbp != 0) {
	        debug_msg("tlx_get_tlx_events: rbp = %d: decoding rbuf[0]= 0x%x", event->rbp, event->rbuf[0] );
		if ((event->rbuf[0] & 0x40) != 0) {
		        // printf("tlx_get_tlx_events: clock\n" );
			event->clock = 1;
		        debug_msg("tlx_get_tlx_events: sending events to tlx" );
			tlx_signal_tlx_model(event);
		        //printf("tlx_get_tlx_events: sent\n" );
			if (event->rbuf[0] == 0x40) {
				//printf("tlx_get_tlx_events: only a clock, nothing else to decode\n" );
				event->rbp = 0;
				return 1;
			}
		}
		// read bytes 1->4; 1&2 are cmd_data_byte_cnt...3&4 are resp_data_byte_cnt
		if ( ( bc = recv( event->sockfd, event->rbuf + event->rbp, 4, 0 ) ) == -1 ) {
			if (errno == EWOULDBLOCK) {
    			        // there is not enough on the socket
				return 0;
			} else {
    			        // something bad happened to the socket
				return -1;
			}
		}
		 //printf("tlx_get_tlx_events: read bc = 0x%04x: more bytes from rbuf\n", bc );
		if ( bc == 0 ) {
		         printf("tlx_get_tlx_events: bc = 0 after trying to reading data sizes from rbuf\n" );
			return -1;
		}
		 //printf("tlx_get_tlx_events: bc = 0x%04x: more bytes in rbuf\n", bc );

		event->rbp += bc;
		rbc += 4;  // account for those extra bytes
		 //printf("tlx_get_tlx_events: updated rbp = 0x%04x, rbc = 0x%04x\n", event->rbp, rbc );

		if ((event->rbuf[0] & 0x20) != 0) {
		        // printf("tlx_get_tlx_events: tlx_cfg\n" );
			rbc += 18; // always receiving 4B of data
			// printf("tlx_get_tlx_events: tlx_cfg: rbc is 0x%x \n", rbc);
		}
		if ((event->rbuf[0] & 0x10) != 0) {
		        // printf("tlx_get_tlx_events: tlx_afu_cmd\n" );
			rbc += 22; // for TLX4 cmds, value will increase by 2
			// printf("tlx_get_tlx_events: tlx_afu_cmd: rbc is 0x%x \n", rbc);
		}
		if ((event->rbuf[0] & 0x08) != 0) {
		        // to look at bytes 1 & 2 in buffer to see what rbc will really be
		        // printf("tlx_get_tlx_events: tlx_afu_cmd_data\n" );
			cmd_data_byte_cnt = event->rbuf[1];
			cmd_data_byte_cnt = ((cmd_data_byte_cnt << 8) | event->rbuf[2]);
			rbc += cmd_data_byte_cnt;
			rbc += 1;  // add bdi byte
			// printf("tlx_get_tlx_events: tlx_afu_cmd_data: rbc is 0x%x \n", rbc);
			//rbc += 5; //TODO for now, cmd data always 5B total
		}
		if ((event->rbuf[0] & 0x04) != 0) {
		        // printf("tlx_get_tlx_events: tlx_afu_resp\n" );
			rbc += 7; // for TLX4 resp, will increase by 5B
			// printf("tlx_get_tlx_events: tlx_afu_resp: rbc is 0x%x \n", rbc);
		}
		if ((event->rbuf[0] & 0x02) != 0) {
		        // look at bytes 3 & 4 in buffer to see what rbc will really be
		        // printf("tlx_get_tlx_events: tlx_afu_resp_data\n" );
			resp_data_byte_cnt = event->rbuf[3];
			resp_data_byte_cnt = ((resp_data_byte_cnt << 8) | event->rbuf[4]);
			rbc += resp_data_byte_cnt;
			rbc += 1;  // add bdi byte
	        	 //printf("tlx_get_tlx_events: tlx_afu_resp_data: size = 0x%x\n", resp_data_byte_cnt );
			// printf("tlx_get_tlx_events: tlx_afu_resp_dat: rbc is 0x%x \n", rbc);
			//rbc += 9; //TODO for now, resp data always 9B total
		}
		if ((event->rbuf[0] & 0x01) != 0) {
		        // printf("tlx_get_tlx_events: tlx_afu_credit\n" );
			rbc += 9; //TODO for now, send all credits which are now 9 bytes
			// printf("tlx_get_tlx_events: tlx_afu_credit: rbc is 0x%x \n", rbc);
		}
		//printf("rbc is 0x%x \n", rbc);
		if ( ( bc = recv( event->sockfd, event->rbuf + event->rbp, rbc - event->rbp, 0 ) ) == -1 ) {
			if (errno == EWOULDBLOCK) {
			        printf("tlx_get_tlx_events: not %0x04x data remaining in socket\n", rbc - event->rbp );
				return 0;
			} else {
			        // something bad happened on the socket
			        return -1;
			}
		}
		 //printf("tlx_get_tlx_events: read bc = 0x%04x: more bytes from rbuf\n", bc );
		if (bc == 0) {
		        printf( "tlx_get_tlx_events: bc = 0 after read the remainder of rbuf from socket???\n" );
			return -1;
		}
		event->rbp += bc;
	}
	if (event->rbp < rbc) {
	        printf( "tlx_get_tlx_events: rbp < rbc for some reason...  rbp = 0x%04x, rbc = 0x%04x  leaving with rc = 0 \n",
			event->rbp, rbc );
		return 0;
	}

#ifdef DEBUG
	// dump rbuf
        // if (event->tbuf[0] != 0x40) { // but not for just a clock
	// if (event->tbuf[0] != 0x41) { // but not for just a clock and credit return
	      printf( "DEBUG:tlx_get_tlx_events: rbuf length:0x%02x rbuf: 0x", rbc );
	      for ( i = 0; i < rbc; i++ ) printf( "%02x", event->rbuf[i] );
	      printf( "\n" );
	// }
#endif

	//rbc = 1;
	rbc = 5;
	//printf("TLX_GET_TLX_EVENTS event->rbuf[0] is 0x%2x and event->rbuf[1] is 0x%2x \n", event->rbuf[0], event->rbuf[1]);
		if (event->rbuf[0] & 0x20) {
		event->tlx_cfg_valid = 1;
		//printf("tlx_get_tlx_events: just set tlx_cfg_valid = %d \n", event->tlx_cfg_valid);
		event->tlx_cfg_opcode = event->rbuf[rbc++];
		//printf("event->rbuf[%x] is 0x%2x \n", rbc-1, event->rbuf[rbc-1]);
		event->tlx_cfg_capptag = event->rbuf[rbc++];
		//printf("event->rbuf[%x] is 0x%2x \n", rbc-1, event->rbuf[rbc-1]);
		event->tlx_cfg_capptag = ((event->tlx_cfg_capptag << 8) | event->rbuf[rbc++]);
		//printf("event->rbuf[%x] is 0x%2x \n", rbc-1, event->rbuf[rbc-1]);
		event->tlx_cfg_pa = 0;
		for (bc = 0; bc < 8; bc++) {
			event->tlx_cfg_pa  =
			    ((event->tlx_cfg_pa) << 8) |
			    event->rbuf[rbc++];
		}
		event->tlx_cfg_t = event->rbuf[rbc++];
		event->tlx_cfg_pl = event->rbuf[rbc++];
		event->tlx_cfg_data_bdi = event->rbuf[rbc++];
		//printf("event->rbuf[%x] is 0x%2x \n", rbc-1, event->rbuf[rbc-1]);
		for (i = 0; i < 4; i++) {
			event->tlx_cfg_data_bus[i] = event->rbuf[rbc++] ;
		  }
	} else {
		event->tlx_cfg_valid = 0;
		event->cfg_tlx_credit_return = 0; // TODO do we want to always xmit this as 0?
	}

	if (event->rbuf[0] & 0x10) {
		event->tlx_afu_cmd_valid = 1;
		//printf("tlx_get_tlx_events: just set tlx_afu_cmd_valid = %d \n", event->tlx_afu_cmd_valid);
		event->tlx_afu_cmd_opcode = event->rbuf[rbc++];
		//printf("event->rbuf[%x] is 0x%2x \n", rbc-1, event->rbuf[rbc-1]);
		event->tlx_afu_cmd_capptag = event->rbuf[rbc++];
		//printf("event->rbuf[%x] is 0x%2x \n", rbc-1, event->rbuf[rbc-1]);
		event->tlx_afu_cmd_capptag = ((event->tlx_afu_cmd_capptag << 8) | event->rbuf[rbc++]);
		//printf("event->rbuf[%x] is 0x%2x \n", rbc-1, event->rbuf[rbc-1]);
		event->tlx_afu_cmd_dl = event->rbuf[rbc++];
		event->tlx_afu_cmd_pl = event->rbuf[rbc++];
		event->tlx_afu_cmd_be = 0;
		for (bc = 0; bc < 8; bc++) {
			event->tlx_afu_cmd_be  =
			    ((event->tlx_afu_cmd_be) << 8) |
			    event->rbuf[rbc++];
		}
		event->tlx_afu_cmd_end = event->rbuf[rbc++];
		event->tlx_afu_cmd_pa = 0;
		for (bc = 0; bc < 8; bc++) {
			event->tlx_afu_cmd_pa  =
			    ((event->tlx_afu_cmd_pa) << 8) |
			    event->rbuf[rbc++];
		}
#ifdef TLX4
		event->tlx_afu_cmd_flag  = event->rbuf[rbc++];
		event->tlx_afu_cmd_os  = event->rbuf[rbc++];
#endif
	} else {
		event->tlx_afu_cmd_valid = 0;
		event->afu_tlx_cmd_credit = 0; // TODO do we want to always xmit this as 0?
	}
	//	printf("rbc is 0x%x \n", rbc);
	if (event->rbuf[0] & 0x08) {
		event->tlx_afu_cmd_data_valid = 1;
		// printf("tlx_get_tlx_events: just set tlx_afu_cmd_data_valid = %d \n", event->tlx_afu_cmd_data_valid);
		event->tlx_afu_cmd_data_bdi = event->rbuf[rbc++];
		//printf("event->rbuf[%x] is 0x%2x \n", rbc-1, event->rbuf[rbc-1]);
		for (i = 0; i < cmd_data_byte_cnt; i++) {
			event->tlx_afu_cmd_data_bus[i] = event->rbuf[rbc++] ;
		  }
	} else {
		event->tlx_afu_cmd_data_valid = 0;
	}
	//	printf("rbc is 0x%x \n", rbc);
	if (event->rbuf[0] & 0x04) {
		event->tlx_afu_resp_valid = 1;
		// printf("tlx_get_tlx_events: just set tlx_afu_resp_valid = %d \n", event->tlx_afu_resp_valid);
		event->tlx_afu_resp_opcode = event->rbuf[rbc++];
		// printf("tlx_get_tlx_events: event->rbuf[0x%02x] = 0x%02x\n", rbc, event->rbuf[rbc] );
		event->tlx_afu_resp_afutag = event->rbuf[rbc++];
		// printf("tlx_get_tlx_events: tlx_afu_resp_afutag = 0x%08x\n", event->tlx_afu_resp_afutag );
		// printf("tlx_get_tlx_events: event->rbuf[0x%02x] = 0x%02x\n", rbc, event->rbuf[rbc] );
		event->tlx_afu_resp_afutag = ((event->tlx_afu_resp_afutag << 8) | event->rbuf[rbc++]);
		// printf("tlx_get_tlx_events: tlx_afu_resp_afutag = 0x%08x\n", event->tlx_afu_resp_afutag );
		event->tlx_afu_resp_code = event->rbuf[rbc++];
		event->tlx_afu_resp_pg_size = event->rbuf[rbc++];
		event->tlx_afu_resp_dl = event->rbuf[rbc++];
		event->tlx_afu_resp_dp = event->rbuf[rbc++];
		// printf( "tlx_get_tlx_events: decoded resp opcode=0x%02x, afutag=0x%08x, code=0x%02x, pg_size=0x%02x, dl=0x%02x, dp=0x%02x\n",
		// 	event->tlx_afu_resp_opcode,
		//	event->tlx_afu_resp_afutag,
		//	event->tlx_afu_resp_code,
		//	event->tlx_afu_resp_pg_size,
		//	event->tlx_afu_resp_dl,
		//	event->tlx_afu_resp_dp );
	} else {
		event->tlx_afu_resp_valid = 0;
		event->afu_tlx_resp_credit = 0;
	}
	//	printf("rbc is 0x%x \n", rbc);
	if (event->rbuf[0] & 0x02) {
		event->tlx_afu_resp_data_valid = 1;
		// printf("tlx_get_tlx_events: just set tlx_afu_resp__data_valid = %d \n", event->tlx_afu_resp_data_valid);
		event->tlx_afu_resp_data_bdi = event->rbuf[rbc++] ;
		//printf("event->rbuf[%x] is 0x%2x \n", rbc-1, event->rbuf[rbc-1]);
		for (i = 0; i < resp_data_byte_cnt ; i++) {
			event->tlx_afu_resp_data[i] = event->rbuf[rbc++] ;
		}
	} else {
		event->tlx_afu_resp_data_valid = 0;
	}
	//	printf("rbc is 0x%x \n", rbc);
	if (event->rbuf[0] & 0x01) {
		event->tlx_afu_credit_valid = 1;
		//printf("tlx_get_tlx_events: just set tlx_afu_credit_valid = %d \n", event->tlx_afu_credit_valid);
		event->tlx_afu_cmd_initial_credit = event->rbuf[rbc++];
		event->tlx_afu_cmd_data_initial_credit = event->rbuf[rbc++];
		event->tlx_afu_resp_initial_credit = event->rbuf[rbc++];
		event->tlx_afu_resp_data_initial_credit = event->rbuf[rbc++];
		event->tlx_afu_resp_credit = event->rbuf[rbc++];
		event->tlx_afu_cmd_credit = event->rbuf[rbc++];
		event->tlx_afu_resp_data_credit = event->rbuf[rbc++];
		event->tlx_afu_cmd_data_credit = event->rbuf[rbc++];
		event->tlx_cfg_resp_ack = event->rbuf[rbc++];
		// printf("lgt: tlx_afu_resp_credit is 0x%x \n", event->tlx_afu_resp_credit);
		// printf("lgt: tlx_afu_resp_data_credit is 0x%x \n", event->tlx_afu_resp_data_credit);
		if (event->tlx_afu_cmd_credit == 1) {
			event->tlx_afu_cmd_credits_available += 1;
		// printf("lgt: tlx_get_tlx_events: incremented tlx_afu_cmd_credits_available is 0x%x \n", event->tlx_afu_cmd_credits_available);
		  }
		if (event->tlx_afu_resp_credit == 1)
			event->tlx_afu_resp_credits_available += 1;
		if (event->tlx_afu_cmd_data_credit == 1)
			event->tlx_afu_cmd_data_credits_available += 1;
		if (event->tlx_afu_resp_data_credit == 1)
			event->tlx_afu_resp_data_credits_available += 1;
		// printf("lgt: tlx_get_tlx_events: incremented tlx_afu_resp_credits_available is 0x%x \n", event->tlx_afu_resp_credits_available);
		// printf("lgt: tlx_get_tlx_events: incremented tlx_afu_resp_data_credits_available is 0x%x \n", event->tlx_afu_resp_data_credits_available);
	} else {
		event->tlx_afu_credit_valid = 0;
		event->tlx_afu_cmd_credit = 0;
		event->tlx_afu_resp_credit = 0;
		event->tlx_afu_cmd_data_credit = 0;
		event->tlx_afu_resp_data_credit = 0;
	}
	//	printf("rbc is 0x%x \n", rbc);
	event->rbp = 0;
	return 1;
}


/* Call this from AFU to set the initial afu tlx_credit values */

int afu_tlx_send_initial_credits(struct AFU_EVENT *event,
		uint8_t afu_tlx_cmd_initial_credit,
		uint8_t cfg_tlx_initial_credit,
		uint8_t afu_tlx_resp_initial_credit)

{
	event->afu_tlx_cmd_initial_credit = afu_tlx_cmd_initial_credit;
	event->cfg_tlx_initial_credit = cfg_tlx_initial_credit;
	event->afu_tlx_resp_initial_credit = afu_tlx_resp_initial_credit;
	event->afu_tlx_credit_req_valid = 1;
        debug_msg( "afu_tlx_send_initial_credits: cmd_initial=%d, cfg_initial=%d, resp_initial=%d, req_valid=%d", 
		   event->afu_tlx_cmd_initial_credit, 
		   event->cfg_tlx_initial_credit, 
		   event->afu_tlx_resp_initial_credit,
		   event->afu_tlx_credit_req_valid );
	return TLX_SUCCESS;
}

/* Call this from AFU to read the initial tlx_afu credit values */

int tlx_afu_read_initial_credits(struct AFU_EVENT *event,
				 uint8_t * tlx_afu_cmd_initial_credit,
				 uint8_t * tlx_afu_resp_initial_credit,
				 uint8_t * tlx_afu_cmd_data_initial_credit,
				 uint8_t * tlx_afu_resp_data_initial_credit)

{
        debug_msg( "tlx_afu_read_initial_credits" );

	// why do I care if credit valid is on during initial credit exchange???
	// because I could get 0 initial credits if I don't
	if (!event->tlx_afu_credit_valid) {
	        debug_msg("      no tlx_afu_credit_valid, not even initial_credits");
		return TLX_AFU_NO_CREDITS;
	}

	*tlx_afu_cmd_initial_credit = event->tlx_afu_cmd_initial_credit;
	*tlx_afu_resp_initial_credit = event->tlx_afu_resp_initial_credit;
	*tlx_afu_cmd_data_initial_credit = event->tlx_afu_cmd_data_initial_credit;
	*tlx_afu_resp_data_initial_credit = event->tlx_afu_resp_data_initial_credit;

	event->tlx_afu_cmd_credits_available = event->tlx_afu_cmd_initial_credit;
	event->tlx_afu_resp_credits_available = event->tlx_afu_resp_initial_credit;
	event->tlx_afu_cmd_data_credits_available = event->tlx_afu_cmd_data_initial_credit;
	event->tlx_afu_resp_data_credits_available = event->tlx_afu_resp_data_initial_credit;
	event->tlx_afu_credit_valid = 0;

        debug_msg( "      tlx_afu_cmd_credits_available = %d", event->tlx_afu_cmd_credits_available );
        debug_msg( "      tlx_afu_resp_credits_available = %d", event->tlx_afu_resp_credits_available );
        debug_msg( "      tlx_afu_cmd_data_credits_available = %d", event->tlx_afu_cmd_data_credits_available );
        debug_msg( "      tlx_afu_resp_data_credits_available = %d", event->tlx_afu_resp_data_credits_available );

	return TLX_SUCCESS;
}


///
/* Call this on the AFU side to send a response to ocse.  */

int afu_tlx_send_resp(struct AFU_EVENT *event,
 		 uint8_t afu_resp_opcode,
 		 uint8_t resp_dl, uint16_t resp_capptag,
 		 uint8_t resp_dp, uint8_t resp_code)
{
        debug_msg ( "afu_tlx_send_resp: available credits = %d", event->tlx_afu_resp_credits_available );
        if (event->tlx_afu_resp_credits_available == 0)
		return TLX_AFU_NO_CREDITS;
	if (event->afu_tlx_resp_valid) {
		return AFU_TLX_DOUBLE_RESP;
	} else {
		event->afu_tlx_resp_valid = 1;
		event->tlx_afu_resp_credits_available -= 1;
		event->afu_tlx_resp_opcode = afu_resp_opcode;
		event->afu_tlx_resp_capptag = resp_capptag;
		event->afu_tlx_resp_code = resp_code;
		event->afu_tlx_resp_dl = resp_dl;
		event->afu_tlx_resp_dp = resp_dp;
		debug_msg( "afu_tlx_send_resp: afu consumed resp credit, now %d", event->tlx_afu_resp_credits_available);
		return TLX_SUCCESS;
	}
}


/* Call this from afu to send response data to ocse   assume can only send 64B
 * @ time to FIFO ?*/

int afu_tlx_send_resp_data(struct AFU_EVENT *event,
		 uint8_t DATA_RESP_CONTINUATION,
		 uint8_t rdata_bdi,uint8_t resp_dp,
		 uint8_t resp_dl,uint8_t * rdata_bus)

{
        debug_msg ( "afu_tlx_send_resp_data: available resp data credits = %d", event->tlx_afu_resp_data_credits_available );
        if  (event->tlx_afu_resp_data_credits_available == 0)
		return TLX_AFU_NO_CREDITS;
	if (event->afu_tlx_rdata_valid == 1) {
		return AFU_TLX_DOUBLE_RESP_DATA;
	} else {
		event->afu_tlx_rdata_valid = 1;
		event->tlx_afu_resp_data_credits_available -= 1;
		// printf("tlx_afu_resp_data_credits available is %d  \n", event->tlx_afu_resp_data_credits_available);
		event->afu_tlx_resp_dp = resp_dp;
		event->afu_tlx_rdata_bdi = rdata_bdi;
		// LGT - we will always get 64 Bytes of data from the afu
		// LGT - it will be up to "ocse" to extract the interesting data from the response
		// LGT - for partial reads, the data will be address aligned in the vector
		// LGT - go look for byte counts!
		memcpy(event->afu_tlx_rdata_bus, rdata_bus, 64);
	//	int i;
	//	for (i = 0; i <5 ; i++) {
	//		printf("Send data is %02x"  , rdata_bus[i]);
	//	}
	//	printf("\n");
		debug_msg ( "afu_tlx_send_resp_data: afu consumed resp data credits, now resp data credits = %d",  event->tlx_afu_resp_data_credits_available );
		return TLX_SUCCESS;
	}


}




/* Call this on the AFU side to send a response and response data to ocse.  */

int afu_tlx_send_resp_and_data(struct AFU_EVENT *event,
 		 uint8_t afu_resp_opcode,
 		 uint8_t resp_dl, uint16_t resp_capptag,
 		 uint8_t resp_dp, uint8_t resp_code,
  		 uint8_t rdata_valid, uint8_t * rdata_bus,
 		 uint8_t rdata_bdi)

{
  // rdata_bus must be at least a 64 Byte array.
        debug_msg ( "afu_tlx_send_resp_and_data: available resp credits = %d, available resp data credits = %d", event->tlx_afu_resp_credits_available, event->tlx_afu_resp_data_credits_available );
        if ((event->tlx_afu_resp_credits_available == 0) ||
		(event->tlx_afu_resp_data_credits_available == 0))
		return TLX_AFU_NO_CREDITS;
	if ((event->afu_tlx_resp_valid ==1) || (event->afu_tlx_rdata_valid == 1)) {
		return AFU_TLX_DOUBLE_RESP_AND_DATA;
	} else {
		event->afu_tlx_resp_valid = 1;
		event->tlx_afu_resp_credits_available -= 1;
		// printf("tlx_afu_resp_credits available is %d  \n", event->tlx_afu_resp_credits_available);
		event->afu_tlx_rdata_valid = 1;
		event->tlx_afu_resp_data_credits_available -= 1;
		// printf("tlx_afu_resp_data_credits available is %d  \n", event->tlx_afu_resp_data_credits_available);
		event->afu_tlx_resp_opcode = afu_resp_opcode;
		event->afu_tlx_resp_capptag = resp_capptag;
		event->afu_tlx_resp_code = resp_code;
		event->afu_tlx_resp_dl = resp_dl;
		event->afu_tlx_resp_dp = resp_dp;
		event->afu_tlx_rdata_bdi = rdata_bdi;
		// LGT - we will always get 64 Bytes of data from the afu
		// LGT - it will be up to "ocse" to extract the interesting data from the response
		// LGT - for partial reads, the data will be address aligned in the vector
		// LGT - go look for byte counts!
		memcpy(event->afu_tlx_rdata_bus, rdata_bus, 64);
	//	int i;
	//	for (i = 0; i <5 ; i++) {
	//		printf("Send data is %02x"  , rdata_bus[i]);
	//	}
	//	printf("\n");
		debug_msg ( "afu_tlx_send_resp_and_data: afu consumed resp and data credits, now resp credits = %d, resp data credits = %d", event->tlx_afu_resp_credits_available, event->tlx_afu_resp_data_credits_available );
		return TLX_SUCCESS;
	}
}


/* CALL THIS FOR CONFIG_RD/CONFIG_WR ONLY */
/* Call this from AFU to send config_wr response and config_rd response and data.
 * This updates cfg_tlx_rdata bus and interface signals. It expects caller to provide
 * cfg_resp_opcode, cfg_capptag, cfg_resp_code,  cfg_rdata_bdi & up to 4B data.
 * It always sends 4B of data over socket.  If no data, we send 0xdead. No more need to send byte cnt.
*/

int afu_cfg_send_resp_and_data(struct AFU_EVENT *event,
 		 uint8_t cfg_resp_opcode,
 		 uint16_t cfg_resp_capptag, uint8_t cfg_resp_code,
		 uint16_t afu_cfg_resp_data_byte_cnt,
		 //uint8_t cfg_rdata_offset, // DO WE NEED THIS?
  		 uint8_t cfg_rdata_valid, uint8_t * cfg_rdata_bus,
 		 uint8_t cfg_rdata_bdi)

{
// Not sure if there is a way to check to be sure AFU is just using this for config_wr or
// config_rd responses, as there isn't a cmd_opcode to check and resp is generic.
// Also, can't seem to find a credit for this?
	char fill[4] = "dead";
	if (event->cfg_tlx_resp_valid ==1)  {
		return AFU_TLX_DOUBLE_RESP_AND_DATA;
	} else {
		event->cfg_tlx_resp_valid = 1;
		event->cfg_tlx_resp_valid = 1;
		event->cfg_tlx_rdata_bdi = cfg_rdata_bdi;
		if (cfg_resp_opcode != AFU_RSP_MEM_RD_RESP)   //no data so send 0xdead for filler
			memcpy(event->cfg_tlx_rdata_bus, &fill, 4); // hope this works
		else
			memcpy(event->cfg_tlx_rdata_bus, cfg_rdata_bus, 4);
		debug_msg("afu_cfg_send_resp_and_data and rdata_bus[0] = 0x%x \n",
			cfg_rdata_bus[0]);
		debug_msg("afu_cfg_send_resp_and_data and rdata_bus[1] = 0x%x \n",
			cfg_rdata_bus[1]);
		debug_msg("afu_cfg_send_resp_and_data and rdata_bus[2] = 0x%x \n",
			cfg_rdata_bus[2]);
		debug_msg("afu_cfg_send_resp_and_data and rdata_bus[3] = 0x%x \n",
			cfg_rdata_bus[3]);
		//}
		event->cfg_tlx_resp_opcode = cfg_resp_opcode;
		event->cfg_tlx_resp_capptag = cfg_resp_capptag;
		event->cfg_tlx_resp_code = cfg_resp_code;
		return TLX_SUCCESS;
	}
}


/* Call this on the AFU side to send a command to ocse */

int afu_tlx_send_cmd(struct AFU_EVENT *event,
		 uint8_t afu_cmd_opcode, uint16_t cmd_actag,
  	 	 uint8_t cmd_stream_id, uint8_t * cmd_ea_or_obj,
  		 uint16_t cmd_afutag, uint8_t cmd_dl,
  		 uint8_t cmd_pl,
#ifdef TLX4
  		 uint8_t cmd_os,     /* 1 bit ordered segment CAPI 4 */
#endif
  	 	 uint64_t cmd_be,uint8_t cmd_flag,
		 uint8_t cmd_endian, uint16_t cmd_bdf,
 		 uint32_t cmd_pasid, uint8_t cmd_pg_size)

{
        debug_msg("afu_tlx_send_cmd: tlx_afu_cmd_credits available is %d", event->tlx_afu_cmd_credits_available );

	if (event->tlx_afu_cmd_credits_available == 0) {
		warn_msg("afu_tlx_send_cmd: no credits available", event->tlx_afu_cmd_credits_available);
		return TLX_AFU_NO_CREDITS;
	}
	if (event->afu_tlx_cmd_valid) {
		warn_msg("afu_tlx_send_cmd: double command", event->tlx_afu_cmd_credits_available);
		return AFU_TLX_DOUBLE_COMMAND;
	}

        debug_msg( "afu_tlx_send_cmd: opcode=0x%02x actag=0x%04x bdf=0x%04x pasid=0x%08x",
		   afu_cmd_opcode, cmd_actag, cmd_bdf, cmd_pasid );
	event->afu_tlx_cmd_valid = 1;
	event->tlx_afu_cmd_credits_available -= 1;
	event->afu_tlx_cmd_opcode = afu_cmd_opcode;
	event->afu_tlx_cmd_actag = cmd_actag;
	event->afu_tlx_cmd_stream_id = cmd_stream_id;
	memcpy(event->afu_tlx_cmd_ea_or_obj,cmd_ea_or_obj,0x9);
	event->afu_tlx_cmd_afutag = cmd_afutag;
	event->afu_tlx_cmd_dl = cmd_dl;
	event->afu_tlx_cmd_pl = cmd_pl;
#ifdef TLX4
	event->afu_tlx_cmd_os = cmd_os;
#endif
	event->afu_tlx_cmd_be = cmd_be;
	event->afu_tlx_cmd_flag = cmd_flag;
	event->afu_tlx_cmd_endian = cmd_endian;
	event->afu_tlx_cmd_bdf = cmd_bdf;
	event->afu_tlx_cmd_pasid = cmd_pasid;
	event->afu_tlx_cmd_pg_size = cmd_pg_size;
	return TLX_SUCCESS;

}


/* Call this from afu to send command data to ocse   assume can only send 64B
 * @ time to FIFO */

int afu_tlx_send_cmd_data( struct AFU_EVENT *event,
			   uint8_t cdata_bdi,
			   uint8_t * cdata_bus )
{
  debug_msg("afu_tlx_send_cmd_data: tlx_afu_cmd_data_credits available is %d", event->tlx_afu_cmd_data_credits_available );

  if (event->tlx_afu_cmd_data_credits_available == 0) {
    warn_msg("afu_tlx_send_cmd_data: no credits available", event->tlx_afu_cmd_data_credits_available);
    return TLX_AFU_NO_CREDITS;
   }

  event->afu_tlx_cdata_valid = 1;
  event->tlx_afu_cmd_data_credits_available -= 1;
  event->afu_tlx_cdata_bdi = cdata_bdi;

  // AFU always sends the full content of the cmd_data_bus
  memcpy(event->afu_tlx_cdata_bus, cdata_bus, 64);

  // int i;
  // printf( "afu_tlx_send_cmd_data:event->afu_tlx_cdata_bus=0x" ); for ( i = 0; i < 64; i++ ) printf( "%02x", event->afu_tlx_cdata_bus[i] ); printf( "\n" );

  return TLX_SUCCESS;
}


/* Call this on the AFU side to send a command and cmd data to ocse */

int afu_tlx_send_cmd_and_data(struct AFU_EVENT *event,
		 uint8_t afu_cmd_opcode, uint16_t cmd_actag,
  	 	 uint8_t cmd_stream_id, uint8_t * cmd_ea_or_obj,
  		 uint16_t cmd_afutag, uint8_t cmd_dl,  /* combine dl and pl ??? */
  		 uint8_t cmd_pl,
#ifdef TLX4
  		 uint8_t cmd_os,     /* 1 bit ordered segment CAPI 4 */
#endif
  	 	 uint64_t cmd_be,uint8_t cmd_flag,
		 uint8_t cmd_endian, uint16_t cmd_bdf,
 		 uint32_t cmd_pasid, uint8_t cmd_pg_size,
  		 uint8_t * cdata_bus, uint8_t cdata_bdi)

{
	if ((event->tlx_afu_cmd_credits_available == 0) ||
	    (event->tlx_afu_cmd_data_credits_available == 0)) {
	  warn_msg("afu_tlx_send_cmd_and_data: no credits cmd or cmd_data available: cmd_credits = %d, cmd_data_credits = %d", event->tlx_afu_cmd_credits_available, event->tlx_afu_cmd_data_credits_available);
	  return TLX_AFU_NO_CREDITS;
	}
	if ((event->afu_tlx_cmd_valid == 1) || (event->afu_tlx_cdata_valid == 1)) {
		return AFU_TLX_DOUBLE_CMD_AND_DATA;
	} else {
		event->afu_tlx_cmd_valid = 1;
		event->tlx_afu_cmd_credits_available -= 1;
		debug_msg("afu_tlx_send_cmd_and_data: tlx_afu_cmd_credits available is %d", event->tlx_afu_cmd_credits_available);
		event->afu_tlx_cdata_valid = 1;
		event->tlx_afu_cmd_data_credits_available -= 1;
		debug_msg("afu_tlx_send_cmd_and_data: tlx_afu_cmd_data_credits available is %d", event->tlx_afu_cmd_data_credits_available);
		event->afu_tlx_cmd_opcode = afu_cmd_opcode;
		event->afu_tlx_cmd_actag = cmd_actag;
		event->afu_tlx_cmd_stream_id = cmd_stream_id;
		memcpy(event->afu_tlx_cmd_ea_or_obj,cmd_ea_or_obj,9);
		event->afu_tlx_cmd_afutag = cmd_afutag;
		event->afu_tlx_cmd_dl = cmd_dl;
		event->afu_tlx_cmd_pl = cmd_pl;
#ifdef TLX4
		event->afu_tlx_cmd_os = cmd_os;
#endif
		event->afu_tlx_cmd_be = cmd_be;
		event->afu_tlx_cmd_flag = cmd_flag;
		event->afu_tlx_cmd_endian = cmd_endian;
		event->afu_tlx_cmd_bdf = cmd_bdf;
		event->afu_tlx_cmd_pasid = cmd_pasid;
		event->afu_tlx_cmd_pg_size = cmd_pg_size;
		event->afu_tlx_cdata_bdi = cdata_bdi;
		// TODO FOR NOW WE ALWAYS COPY 64 BYTES of DATA - AFU ALWAYS
		// SENDS 64 BYTES
		memcpy(event->afu_tlx_cdata_bus, cdata_bus, 64);
		return TLX_SUCCESS;
	}
}


/* Call this from AFU to read ocse (CAPP/TL) response. This reads just tlx_afu resp interface */

int tlx_afu_read_resp(struct AFU_EVENT *event,
		 uint8_t * tlx_resp_opcode,
		 uint16_t * resp_afutag, uint8_t * resp_code,
		 uint8_t * resp_pg_size, uint8_t * resp_dl,
#ifdef TLX4
		 uint32_t * resp_host_tag, uint8_t * resp_cache_state,
#endif
		 uint8_t * resp_dp, uint32_t * resp_addr_tag)

{
	if (!event->tlx_afu_resp_valid) {
		return TLX_AFU_RESP_NOT_VALID;
	} else {
		event->tlx_afu_resp_valid = 0;
		* tlx_resp_opcode = event->tlx_afu_resp_opcode;
		* resp_afutag = event->tlx_afu_resp_afutag;
		* resp_code = event->tlx_afu_resp_code;
		* resp_pg_size = event->tlx_afu_resp_pg_size;
		* resp_dl = event->tlx_afu_resp_dl;
#ifdef TLX4
		* resp_host_tag = event->tlx_afu_host_tag;
		* resp_cache_state = event->tlx_afu_resp_cache_state;
#endif
		* resp_dp = event->tlx_afu_resp_dp;
		* resp_addr_tag = event->tlx_afu_resp_addr_tag;
		}
		return TLX_SUCCESS;
}


/* Call this from AFU to request data on the response data interface  ALSO, AFU calls w/0 values to reset*/
int afu_tlx_resp_data_read_req(struct AFU_EVENT *event,
		 uint8_t  afu_tlx_resp_rd_req, uint8_t  afu_tlx_resp_rd_cnt)
{
	event->afu_tlx_resp_rd_req = afu_tlx_resp_rd_req;
	event->afu_tlx_resp_rd_cnt = afu_tlx_resp_rd_cnt;
	event->afu_tlx_credit_req_valid = 1;
// WE rely on AFU to reset these values when data has all be read, ie, call this
// again with 0 values
		return TLX_SUCCESS;
}


/* Call this from AFU to read ocse (CAPP/TL) response data. This reads just tlx_afu resp data interface */

int tlx_afu_read_resp_data(struct AFU_EVENT *event,
		  uint8_t * resp_data_bdi, uint8_t * resp_data)
{
	if (!event->tlx_afu_resp_data_valid) {
		return TLX_AFU_RESP_DATA_NOT_VALID;
	} else {
		event->tlx_afu_resp_data_valid = 0;
		* resp_data_bdi = event->tlx_afu_resp_data_bdi;
		// host side uses tlx_afu_send_resp_data or tlx_afu_send_resp_and_data
		// to put upto 256 B in event->tlx_afu_resp_data
		// we don't specify a size in the api, so we have to pull the full buffer
		// and allow the reader to use the prior tlx_afu_read_resp to know how much
		// of the buffer to use
		memcpy(resp_data, event->tlx_afu_resp_data, 256);
		return TLX_SUCCESS;
		}
}



/* Call this from AFU to read ocse (CAPP/TL) command. This reads just tlx_afu cmd interfaces  */

int tlx_afu_read_cmd(struct AFU_EVENT *event,
		  uint8_t * tlx_cmd_opcode,
		 uint16_t * cmd_capptag, uint8_t * cmd_dl,
		 uint8_t * cmd_pl, uint64_t * cmd_be,
		 uint8_t * cmd_end, // uint8_t * cmd_t,
#ifdef TLX4
		 uint8_t * cmd_flag,    /* used for atomics from host CAPI 4 */
  		 uint8_t * cmd_os,     /* 1 bit ordered segment CAPI 4 */
#endif
		 uint64_t * cmd_pa)

{
	if (!event->tlx_afu_cmd_valid) {
		return TLX_AFU_CMD_NOT_VALID;
	} else {
		event->tlx_afu_cmd_valid = 0;
		// printf("in tlx_afu_read_cmd and just set tlx_afu_cmd_valid = %d \n", event->tlx_afu_cmd_valid);
		* tlx_cmd_opcode = event->tlx_afu_cmd_opcode;
		* cmd_capptag = event->tlx_afu_cmd_capptag;
		* cmd_dl = event->tlx_afu_cmd_dl;
		* cmd_pl = event->tlx_afu_cmd_pl;
		* cmd_be = event->tlx_afu_cmd_be;
		* cmd_end = event->tlx_afu_cmd_end;
		* cmd_pa = event->tlx_afu_cmd_pa;
#ifdef TLX4
		* cmd_flag = event->tlx_afu_cmd_flag;
		* cmd_os = event->tlx_afu_cmd_os;
#endif
		return TLX_SUCCESS;
	}
}


/* Call this from AFU to request data on the command interface  ALSO, AFU calls w/0 values to reset*/
int afu_tlx_cmd_data_read_req(struct AFU_EVENT *event,
		 uint8_t afu_tlx_cmd_rd_req, uint8_t afu_tlx_cmd_rd_cnt)
{
	event->afu_tlx_cmd_rd_req = afu_tlx_cmd_rd_req;
	event->afu_tlx_cmd_rd_cnt = afu_tlx_cmd_rd_cnt;
	event->afu_tlx_credit_req_valid = 1;
// WE rely on AFU to reset these values when data has all be read, ie, call this
// again with 0 values
		return TLX_SUCCESS;
}


/* Call this from AFU to read ocse (CAPP/TL) command data. This reads just tlx_afu cmd data interface */

int tlx_afu_read_cmd_data(struct AFU_EVENT *event,
		  uint8_t * cmd_data_bdi, uint8_t * cmd_data_bus)
{
	if (!event->tlx_afu_cmd_data_valid) {
		return TLX_AFU_CMD_DATA_NOT_VALID;
	} else {

		event->tlx_afu_cmd_data_valid = 0;
		* cmd_data_bdi = event->tlx_afu_cmd_data_bdi;
		// TODO FOR NOW WE ALWAYS COPY 4 BYTES of DATA - OCSE
		// SENDS 4 BYTES
		memcpy(cmd_data_bus, event->tlx_afu_cmd_data_bus, 4);
		return TLX_SUCCESS;
	}
}
/* CALL THIS FOR CONFIG_RD/CONFIG_WR ONLY */
/* Call this from AFU to read config_rd commands and config_wr commands and data.
 * This updates tlx_cfg_data_bus. It expects caller to provide
 * pointers for everything expected, including pointer to data buffer for up to 4B data.
 * This function checks to see if cmd in afu_event is a config cmd and returns a nonzero
 * value if no config_cmd is present. If this is a config_wr cmd, data will be present
 * and will be provided back to caller, along with config cmd paramters. cmd_pl will
 * contain the number of data bytes, encoded per spec (0= 1 byte, 1= 2, 2= 4).
 * We rely on afu_driver to add the one cycle delay for config data when sent to AFU.
*/

int tlx_cfg_read_cmd_and_data(struct AFU_EVENT *event,
		 uint8_t * cfg_data_bdi, uint8_t * cfg_data_bus,
		 uint8_t * tlx_cfg_opcode,uint16_t * cfg_capptag,
		 uint8_t * cfg_pl, uint8_t * cfg_t,
		 uint64_t * cfg_pa)


{
	uint8_t cfg_data_byte_cnt;

	if ((event->tlx_cfg_opcode != TLX_CMD_CONFIG_READ) &&
		 (event->tlx_cfg_opcode != TLX_CMD_CONFIG_WRITE)) {
		return (CFG_TLX_NOT_CFG_CMD);
	}
	//printf("tlx_cfg_read_cmd_and_data entered \n");
	event->tlx_cfg_valid = 0;
	* tlx_cfg_opcode = event->tlx_cfg_opcode;
	* cfg_capptag = event->tlx_cfg_capptag;
	* cfg_pl = event->tlx_cfg_pl;
	* cfg_t = event->tlx_cfg_t;
	* cfg_pa = event->tlx_cfg_pa;
	switch (event->tlx_cfg_pl) {
		case 0: cfg_data_byte_cnt = 1;
			break;
		case 1: cfg_data_byte_cnt = 2;
			break;
		case 2: cfg_data_byte_cnt = 4;
			break;
		default: cfg_data_byte_cnt = 0;
			break;
		}

	//if (event->tlx_cfg_cmd_data_valid) {
	if (event->tlx_cfg_opcode == TLX_CMD_CONFIG_WRITE) { // there is data to read
		//event->tlx_cfg_cmd_data_valid = 0;
		* cfg_data_bdi = event->tlx_cfg_data_bdi;
		memcpy(cfg_data_bus, event->tlx_cfg_data_bus, cfg_data_byte_cnt);
			debug_msg("tlx_cfg_read_cmd_and_data and cfg_data_byte_cnt = 0x%x \n",
				cfg_data_byte_cnt);
			debug_msg("tlx_cfg_read_cmd_and_data and tlx_cfg_data_bus[0] = 0x%x \n",
				cfg_data_bus[0]);
			debug_msg("tlx_cfg_read_cmd_and_data and tlx_cfg_data_bus[1] = 0x%x \n",
				cfg_data_bus[1]);
			debug_msg("tlx_cfg_read_cmd_and_data and tlx_cfg_data_bus[2] = 0x%x \n",
				cfg_data_bus[2]);
			debug_msg("tlx_cfg_read_cmd_and_data and tlx_cfg_data_bus[3] = 0x%x \n",
				cfg_data_bus[3]);

	}
	return TLX_SUCCESS;
}

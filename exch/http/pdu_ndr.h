#pragma once
#include <cstdint>
#include <gromox/ndr.hpp>
#include "pdu_ndr_ids.hpp"

struct DCERPC_CTX_LIST {
	uint16_t context_id;
	uint8_t num_transfer_syntaxes;
	SYNTAX_ID abstract_syntax;
	SYNTAX_ID *transfer_syntaxes;
};

union DCERPC_OBJECT {
	char empty;
	GUID object;
};

struct DCERPC_ACK_CTX {
	uint16_t result;
	uint16_t reason;
	SYNTAX_ID syntax;
};

struct dcerpc_payload {};

struct dcerpc_request final : public dcerpc_payload {
	~dcerpc_request();

	uint32_t alloc_hint = 0;
	uint16_t context_id = 0, opnum = 0;
	DCERPC_OBJECT object{};
	DATA_BLOB pad{}, stub_and_verifier{};
};
using DCERPC_REQUEST = dcerpc_request;

struct dcerpc_response final : public dcerpc_payload {
	~dcerpc_response();

	uint32_t alloc_hint = 0;
	uint16_t context_id = 0;
	uint8_t cancel_count = 0;
	DATA_BLOB pad{}, stub_and_verifier{};
};
using DCERPC_RESPONSE = dcerpc_response;

struct dcerpc_fault final : public dcerpc_payload {
	~dcerpc_fault();

	uint32_t alloc_hint = 0;
	uint16_t context_id = 0;
	uint8_t cancel_count = 0;
	int status = 0; /* dcerpc ncacn status */
	DATA_BLOB pad{};
};
using DCERPC_FAULT = dcerpc_fault;

struct dcerpc_fack final : public dcerpc_payload {
	~dcerpc_fack();

	uint32_t version = 0;
	uint8_t pad = 0;
	uint16_t window_size = 0;
	uint32_t max_tdsu = 0, max_frag_size = 0;
	uint16_t serial_no = 0, selack_size = 0;
	uint32_t *selack = nullptr;
};
using DCERPC_FACK = dcerpc_fack;

struct dcerpc_cancel_ack final : public dcerpc_payload {
	uint32_t version = 0, id = 0, server_is_accepting = 0;
};
using DCERPC_CANCEL_ACK = dcerpc_cancel_ack;

struct dcerpc_bind final : public dcerpc_payload {
	~dcerpc_bind();

	uint16_t max_xmit_frag = 0, max_recv_frag = 0;
	uint32_t assoc_group_id = 0;
	uint8_t num_contexts = 0;
	DCERPC_CTX_LIST *ctx_list = nullptr;
	DATA_BLOB auth_info{};
};
using DCERPC_BIND = dcerpc_bind;

struct dcerpc_bind_ack final : public dcerpc_payload {
	~dcerpc_bind_ack();

	uint16_t max_xmit_frag = 0, max_recv_frag = 0;
	uint32_t assoc_group_id = 0;
	uint16_t secondary_address_size = 0;
	char secondary_address[64]{};
	DATA_BLOB pad{};
	uint8_t num_contexts = 0;
	DCERPC_ACK_CTX *ctx_list = nullptr;
	DATA_BLOB auth_info{};
};
using DCERPC_BIND_ACK = dcerpc_bind_ack;

struct dcerpc_bind_nak final : public dcerpc_payload {
	~dcerpc_bind_nak();

	uint16_t reject_reason = 0;
	uint32_t num_versions = 0;
	uint32_t *versions = nullptr;
};
using DCERPC_BIND_NAK = dcerpc_bind_nak;

struct dcerpc_co_cancel final : public dcerpc_payload {
	~dcerpc_co_cancel();
	DATA_BLOB auth_info{};
};
using DCERPC_CO_CANCEL = dcerpc_co_cancel;

struct DCERPC_AUTH {
	~DCERPC_AUTH() { clear(); }
	void clear();

	uint8_t auth_type = 0, auth_level = 0, auth_pad_length = 0;
	uint8_t auth_reserved = 0;
	uint32_t auth_context_id = 0;
	DATA_BLOB credentials{};
};

struct dcerpc_auth3 final : public dcerpc_payload {
	~dcerpc_auth3();
	uint32_t pad = 0;
	DATA_BLOB auth_info{};
};
using DCERPC_AUTH3 = dcerpc_auth3;

struct dcerpc_orphaned final : public dcerpc_payload {
	~dcerpc_orphaned();
	DATA_BLOB auth_info{};
};
using DCERPC_ORPHANED = dcerpc_orphaned;

struct RTS_FLOWCONTROLACK {
	uint32_t bytes_received;
	uint32_t available_window;
	GUID channel_cookie;
};

struct RTS_CLIENTADDRESS {
	uint32_t address_type;
	char client_address[64];
};

union RTS_CMDS {
	uint32_t receivewindowsize;
	RTS_FLOWCONTROLACK flowcontrolack;
	uint32_t connectiontimeout;
	GUID cookie;
	uint32_t channellifetime;
	uint32_t clientkeepalive;
	uint32_t version;
	char empty;
	uint32_t padding;
	char negative_ance;
	char ance;
	RTS_CLIENTADDRESS clientaddress;
	GUID associationgroupid;
	uint32_t destination;
	uint32_t pingtrafficsentnotify;
};

struct RTS_CMD {
	uint32_t command_type;
	RTS_CMDS command;
};

struct dcerpc_rts final : public dcerpc_payload {
	~dcerpc_rts();
	uint16_t flags = 0, num = 0;
	RTS_CMD *commands = nullptr;
};
using DCERPC_RTS = dcerpc_rts;

/*
 * RTS PDU Header
 *
 * NCA = Network Connection Architecture
 * CN = Connection (ncacn)
 * DG = Datagram / Connectionless (ncadg)
 *
 * C706 §12.6.1 / RPCH v19 §2.2.3.6.1
 */
struct dcerpc_ncacn_packet {
	~dcerpc_ncacn_packet();

	constexpr dcerpc_ncacn_packet(bool be)
	{
		drep[0] = be ? 0 : DCERPC_DREP_LE;
	}
	uint8_t rpc_vers = 5;
	uint8_t rpc_vers_minor = 0;
	uint8_t pfc_flags = 0;
	uint8_t drep[4]{};

	/*
	 * Concerning NDR_PUSH: frag_length is 0 in the class, and so
	 * serialized with pdu_ndr_push_ncacnpkt. The produced blob is later
	 * updated with pdu_processor_set_frag_length.
	 */
	uint16_t frag_length = 0;
	uint16_t auth_length = 0;
	uint32_t call_id = 0;
	uint8_t pkt_type = DCERPC_PKT_INVALID;
	dcerpc_payload *payload = nullptr;
};
using DCERPC_NCACN_PACKET = dcerpc_ncacn_packet;

extern pack_result pdu_ndr_pull_dcerpc_auth(NDR_PULL *, DCERPC_AUTH *);
extern pack_result pdu_ndr_push_dcerpc_auth(NDR_PUSH *, const DCERPC_AUTH *);
extern pack_result pdu_ndr_pull_ncacnpkt(NDR_PULL *, DCERPC_NCACN_PACKET *);
extern pack_result pdu_ndr_push_ncacnpkt(NDR_PUSH *, DCERPC_NCACN_PACKET *);

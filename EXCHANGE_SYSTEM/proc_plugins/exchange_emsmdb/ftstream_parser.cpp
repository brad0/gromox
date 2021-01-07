// SPDX-License-Identifier: GPL-2.0-only WITH linking exception
#include "ftstream_parser.h"
#include "rop_processor.h"
#include "endian_macro.h"
#include <gromox/defs.h>
#include <gromox/mapidefs.h>
#include <gromox/proc_common.h>
#include "common_util.h"
#include "util.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <cstring>
#include <cstdlib>
#include <unistd.h>
#include <fcntl.h>
#include <cstdio>
#define FSTREAM_SVAL(pdata)						SVAL(pdata,0)
#define FSTREAM_IVAL(pdata)						IVAL(pdata,0)

enum {
	FTSTREAM_PARSER_READ_FAIL = -1,
	FTSTREAM_PARSER_READ_OK,
	FTSTREAM_PARSER_READ_CONTINUE
};


static BOOL ftstream_parser_read_uint16(
	FTSTREAM_PARSER *pstream, uint16_t *pv)
{
	uint16_t tmp_val;
	
	if (sizeof(uint16_t) != read(pstream->fd,
		&tmp_val, sizeof(uint16_t))) {
		return FALSE;
	}
	*pv = FSTREAM_SVAL(&tmp_val);
	pstream->offset += sizeof(uint16_t);
	return TRUE;
}

static BOOL ftstream_parser_read_uint32(
	FTSTREAM_PARSER *pstream, uint32_t *pv)
{
	uint32_t tmp_val;
	
	if (sizeof(uint32_t) != read(pstream->fd,
		&tmp_val, sizeof(uint32_t))) {
		return FALSE;
	}
	*pv = FSTREAM_IVAL(&tmp_val);
	pstream->offset += sizeof(uint32_t);
	return TRUE;
}

static BOOL ftstream_parser_read_uint64(
	FTSTREAM_PARSER *pstream, uint64_t *pv)
{
	uint64_t tmp_val;
	
	if (sizeof(uint64_t) != read(pstream->fd,
		&tmp_val, sizeof(uint64_t))) {
		return FALSE;
	}
	*pv = FSTREAM_IVAL(&tmp_val);
	*pv |= static_cast<uint64_t>(FSTREAM_IVAL(reinterpret_cast<char *>(&tmp_val) + 4)) << 32;
	pstream->offset += sizeof(uint64_t);
	return TRUE;
}

static char* ftstream_parser_read_wstring(
	FTSTREAM_PARSER *pstream, BOOL *pb_continue)
{
	char *pbuff;
	char *pbuff1;
	uint32_t len;
	uint32_t tmp_len;
	uint32_t origin_offset;
	
	*pb_continue = FALSE;
	origin_offset = pstream->offset;
	if (FALSE == ftstream_parser_read_uint32(
		pstream, &len)) {
		return NULL;
	}
	if (len >= common_util_get_param(
		COMMON_UTIL_MAX_MAIL_LENGTH)) {
		return NULL;	
	}
	if (origin_offset + sizeof(uint32_t) + len >
		pstream->st_size) {
		*pb_continue = TRUE;
		return NULL;
	}
	tmp_len = 2*len;
	pbuff = static_cast<char *>(malloc(len + 2));
	if (NULL == pbuff) {
		return NULL;
	}
	if (len != read(pstream->fd, pbuff, len)) {
		free(pbuff);
		return NULL;
	}
	pstream->offset += len;
	/* if trail nulls not found, append them */
	if (0 != pbuff[len - 2] && 0 != pbuff[len - 1]) {
		pbuff[len] = 0;
		pbuff[len + 1] = 0;
		len += 2;
	}
	pbuff1 = static_cast<char *>(common_util_alloc(tmp_len));
	if (NULL == pbuff1) {
		free(pbuff);
		return NULL;
	}
	if (FALSE == utf16le_to_utf8(
		pbuff, len, pbuff1, tmp_len)) {
		free(pbuff);
		return NULL;
	}
	free(pbuff);
	return pbuff1;
}

static char* ftstream_parser_read_string(
	FTSTREAM_PARSER *pstream, BOOL *pb_continue)
{
	char *pbuff;
	uint32_t len;
	uint32_t origin_offset;
	
	*pb_continue = FALSE;
	origin_offset = pstream->offset;
	if (FALSE == ftstream_parser_read_uint32(
		pstream, &len)) {
		return NULL;
	}
	if (len >= common_util_get_param(
		COMMON_UTIL_MAX_MAIL_LENGTH)) {
		return FALSE;	
	}
	if (origin_offset + sizeof(uint32_t) + len >
		pstream->st_size) {
		*pb_continue = TRUE;
		return NULL;
	}
	pbuff = static_cast<char *>(common_util_alloc(len + 1));
	if (NULL == pbuff) {
		return NULL;
	}
	if (len != read(pstream->fd, pbuff, len)) {
		return NULL;
	}
	pstream->offset += len;
	/* if trail null not found, append it */
	if ('\0' != pbuff[len - 1]) {
		pbuff[len] = '\0';
	}
	return pbuff;
}

static char* ftstream_parser_read_naked_wstring(
	FTSTREAM_PARSER *pstream)
{
	char *pbuff;
	uint32_t len;
	char buff[1024];
	uint32_t offset;
	
	offset = 0;
	while (TRUE) {
		if (2 != read(pstream->fd, buff + offset, 2)) {
			return NULL;
		}
		if (0 == buff[offset] && 0 == buff[offset + 1]) {
			break;
		}
		offset += 2;
		if (offset == sizeof(buff)) {
			return NULL;
		}
	}
	len = offset + 2;
	pstream->offset += len;
	pbuff = static_cast<char *>(common_util_alloc(2 * len));
	if (NULL == pbuff) {
		return NULL;
	}
	if (FALSE == utf16le_to_utf8(
		buff, len, pbuff, 2*len)) {
		return NULL;
	}
	return pbuff;
}

static BOOL ftstream_parser_read_guid(
	FTSTREAM_PARSER *pstream, GUID *pguid)
{
	if (FALSE == ftstream_parser_read_uint32(
		pstream, &pguid->time_low)) {
		return FALSE;
	}
	if (FALSE == ftstream_parser_read_uint16(
		pstream, &pguid->time_mid)) {
		return FALSE;
	}
	if (FALSE == ftstream_parser_read_uint16(
		pstream, &pguid->time_hi_and_version)) {
		return FALSE;
	}
	if (2 != read(pstream->fd, pguid->clock_seq, 2)) {
		return FALSE;
	}
	pstream->offset += 2;
	if (6 != read(pstream->fd, pguid->node, 6)) {
		return FALSE;
	}
	pstream->offset += 6;
	return TRUE;
}

static BOOL ftstream_parser_read_svreid(
	FTSTREAM_PARSER *pstream,
	SVREID *psvreid, BOOL *pb_continue)
{
	uint32_t len;
	uint8_t ours;
	uint32_t origin_offset;
	
	*pb_continue = FALSE;
	origin_offset = pstream->offset;
	if (FALSE == ftstream_parser_read_uint32(
		pstream, &len)) {
		return FALSE;
	}
	if (origin_offset + sizeof(uint32_t) + len >
		pstream->st_size) {
		*pb_continue = TRUE;
		return FALSE;
	}
	if (sizeof(uint8_t) != read(pstream->fd,
		&ours, sizeof(uint8_t))) {
		return FALSE;
	}
	pstream->offset += sizeof(uint8_t);
	if (0 == ours) {
		psvreid->pbin = static_cast<BINARY *>(common_util_alloc(sizeof(BINARY)));
		if (NULL == psvreid->pbin) {
			return FALSE;
		}
		psvreid->pbin->cb = len - 1;
		if (0 == psvreid->pbin->cb) {
			psvreid->pbin->pb = NULL;
		} else {
			psvreid->pbin->pv = common_util_alloc(psvreid->pbin->cb);
			if (psvreid->pbin->pv == nullptr)
				return FALSE;
			if (read(pstream->fd, psvreid->pbin->pv, psvreid->pbin->cb) != psvreid->pbin->cb)
				return FALSE;
			pstream->offset += psvreid->pbin->cb;
		}
	}
	if (21 != len) {
		return FALSE;
	}
	psvreid->pbin = NULL;
	if (FALSE == ftstream_parser_read_uint64(
		pstream, &psvreid->folder_id)) {
		return FALSE;
	}
	if (FALSE == ftstream_parser_read_uint64(
		pstream, &psvreid->message_id)) {
		return FALSE;
	}
	if (FALSE == ftstream_parser_read_uint32(
		pstream, &psvreid->instance)) {
		return FALSE;
	}
	return TRUE;
}

static BOOL ftstream_parser_read_binary(
	FTSTREAM_PARSER *pstream, BINARY *pbin,
	BOOL *pb_continue)
{
	uint32_t origin_offset;
	
	*pb_continue = FALSE;
	origin_offset = pstream->offset;
	if (FALSE == ftstream_parser_read_uint32(
		pstream, &pbin->cb)) {
		return FALSE;
	}
	if (pbin->cb >= common_util_get_param(
		COMMON_UTIL_MAX_MAIL_LENGTH)) {
		return FALSE;	
	}
	if (origin_offset + sizeof(uint32_t) +
		pbin->cb > pstream->st_size) {
		*pb_continue = TRUE;
		return FALSE;
	}
	if (0 == pbin->cb) {
		pbin->pb = NULL;
		return TRUE;
	}
	pbin->pv = common_util_alloc(pbin->cb);
	if (pbin->pv == nullptr)
		return FALSE;
	if (read(pstream->fd, pbin->pv, pbin->cb) != pbin->cb)
		return FALSE;
	pstream->offset += pbin->cb;
	return TRUE;
}

static PROPERTY_NAME* ftstream_parser_read_property_name(
	FTSTREAM_PARSER *pstream)
{
	auto pname = static_cast<PROPERTY_NAME *>(common_util_alloc(sizeof(PROPERTY_NAME)));
	if (NULL == pname) {
		return NULL;
	}
	if (FALSE == ftstream_parser_read_guid(
		pstream, &pname->guid)) {
		return NULL;	
	}
	if (sizeof(uint8_t) != read(pstream->fd,
		&pname->kind, sizeof(uint8_t))) {
		return NULL;
	}
	pstream->offset += sizeof(uint8_t);
	pname->plid = NULL;
	pname->pname = NULL;
	switch (pname->kind) {
	case MNID_ID:
		pname->plid = static_cast<uint32_t *>(common_util_alloc(sizeof(uint32_t)));
		if (NULL == pname->plid) {
			return NULL;
		}
		if (FALSE == ftstream_parser_read_uint32(
			pstream, pname->plid)) {
			return FALSE;
		}
		return pname;
	case MNID_STRING:
		pname->pname = ftstream_parser_read_naked_wstring(pstream);
		if (NULL == pname->pname) {
			return NULL;
		}
		return pname;
	}
	return NULL;
}

static int ftstream_parser_read_element(
	FTSTREAM_PARSER *pstream, uint32_t *pmarker,
	TAGGED_PROPVAL *ppropval)
{
	int i;
	uint32_t count;
	BOOL b_continue;
	uint16_t propid;
	uint16_t proptype;
	uint16_t codepage;
	uint16_t fake_byte;
	uint32_t atom_element;
	uint32_t origin_offset;
	PROPERTY_NAME *ppropname;
	
	origin_offset = pstream->offset;
	if (origin_offset == pstream->st_size) {
		return FTSTREAM_PARSER_READ_CONTINUE;
	}
	if (FALSE == ftstream_parser_read_uint32(
		pstream, &atom_element)) {
		return FTSTREAM_PARSER_READ_FAIL;
	}
	switch (atom_element) {
	case STARTTOPFLD:
	case STARTSUBFLD:
	case ENDFOLDER:
	case STARTMESSAGE:
	case ENDMESSAGE:
	case STARTFAIMSG:
	case STARTEMBED:
	case ENDEMBED:
	case STARTRECIP:
	case ENDTORECIP:
	case NEWATTACH:
	case ENDATTACH:
	case INCRSYNCCHG:
	case INCRSYNCCHGPARTIAL:
	case INCRSYNCDEL:
	case INCRSYNCEND:
	case INCRSYNCREAD:
	case INCRSYNCSTATEBEGIN:
	case INCRSYNCSTATEEND:
	case INCRSYNCPROGRESSMODE:
	case INCRSYNCPROGRESSPERMSG:
	case INCRSYNCMESSAGE:
	case INCRSYNCGROUPINFO:
	case FXERRORINFO:
		*pmarker = atom_element;
		return FTSTREAM_PARSER_READ_OK;
	}
	*pmarker = 0;
	proptype = PROP_TYPE(atom_element);
	propid = PROP_ID(atom_element);
	/* META_TAG_IDSETGIVEN, MS-OXCFXICS 3.2.5.2.1 */
	if (META_TAG_IDSETGIVEN == atom_element) {
		proptype = PT_BINARY;
	}
	if (propid & 0x8000) {
		ppropname = ftstream_parser_read_property_name(pstream);
		if (NULL == ppropname) {
			return FTSTREAM_PARSER_READ_FAIL;
		}
		if (FALSE == logon_object_get_named_propid(
			pstream->plogon, TRUE, ppropname, &propid)) {
			return FTSTREAM_PARSER_READ_FAIL;
		}
	}
	if (pstream->st_size == pstream->offset) {
		goto CONTINUE_WAITING;
	}
	ppropval->proptag = PROP_TAG(proptype, propid);
	if (proptype & 0x8000) {
		/* codepage string */
		codepage = proptype & 0x7FFF;
		if (1200 == codepage) {
			ppropval->proptag = CHANGE_PROP_TYPE(ppropval->proptag, PT_UNICODE);
			ppropval->pvalue = ftstream_parser_read_wstring(
										pstream, &b_continue);
		} else {
			ppropval->pvalue = ftstream_parser_read_string(
										pstream, &b_continue);
		}
		if (NULL == ppropval->pvalue) {
			if (TRUE == b_continue) {
				goto CONTINUE_WAITING;
			} else {
				return FTSTREAM_PARSER_READ_FAIL;
			}
		}
		return FTSTREAM_PARSER_READ_OK;
	}
	switch (proptype) {
	case PT_SHORT:
		ppropval->pvalue = common_util_alloc(sizeof(uint16_t));
		if (NULL == ppropval->pvalue) {
			return FTSTREAM_PARSER_READ_FAIL;
		}
		if (!ftstream_parser_read_uint16(pstream, static_cast<uint16_t *>(ppropval->pvalue)))
			return FTSTREAM_PARSER_READ_FAIL;
		return FTSTREAM_PARSER_READ_OK;
	case PT_ERROR:
	case PT_LONG:
		ppropval->pvalue = common_util_alloc(sizeof(uint32_t));
		if (NULL == ppropval->pvalue) {
			return FTSTREAM_PARSER_READ_FAIL;
		}
		if (!ftstream_parser_read_uint32(pstream, static_cast<uint32_t *>(ppropval->pvalue)))
			return FTSTREAM_PARSER_READ_FAIL;	
		return FTSTREAM_PARSER_READ_OK;
	case PT_FLOAT:
		ppropval->pvalue = common_util_alloc(sizeof(float));
		if (NULL == ppropval->pvalue) {
			return FTSTREAM_PARSER_READ_FAIL;
		}
		if (sizeof(float) != read(pstream->fd,
			ppropval->pvalue, sizeof(float))) {
			return FTSTREAM_PARSER_READ_FAIL;	
		}
		pstream->offset += sizeof(float);
		return FTSTREAM_PARSER_READ_OK;
	case PT_DOUBLE:
	case PT_APPTIME:
		ppropval->pvalue = common_util_alloc(sizeof(double));
		if (NULL == ppropval->pvalue) {
			return FTSTREAM_PARSER_READ_FAIL;
		}
		if (sizeof(double) != read(pstream->fd,
			ppropval->pvalue, sizeof(double))) {
			return FTSTREAM_PARSER_READ_FAIL;	
		}
		pstream->offset += sizeof(double);
		return FTSTREAM_PARSER_READ_OK;
	case PT_BOOLEAN:
		ppropval->pvalue = common_util_alloc(sizeof(uint8_t));
		if (NULL == ppropval->pvalue) {
			return FTSTREAM_PARSER_READ_FAIL;
		}
		if (FALSE == ftstream_parser_read_uint16(
			pstream, &fake_byte)) {
			return FTSTREAM_PARSER_READ_FAIL;	
		}
		*(uint8_t*)ppropval->pvalue = fake_byte;
		return FTSTREAM_PARSER_READ_OK;
	case PT_CURRENCY:
	case PT_I8:
	case PT_SYSTIME:
		ppropval->pvalue = common_util_alloc(sizeof(uint64_t));
		if (NULL == ppropval->pvalue) {
			return FTSTREAM_PARSER_READ_FAIL;
		}
		if (!ftstream_parser_read_uint64(pstream, static_cast<uint64_t *>(ppropval->pvalue)))
			return FTSTREAM_PARSER_READ_FAIL;	
		return FTSTREAM_PARSER_READ_OK;
	case PT_STRING8:
		ppropval->pvalue = ftstream_parser_read_string(
								pstream, &b_continue);
		if (NULL == ppropval->pvalue) {
			if (TRUE == b_continue) {
				goto CONTINUE_WAITING;
			} else {
				return FTSTREAM_PARSER_READ_FAIL;
			}
		}
		return FTSTREAM_PARSER_READ_OK;
	case PT_UNICODE:
		ppropval->pvalue = ftstream_parser_read_wstring(
								pstream, &b_continue);
		if (NULL == ppropval->pvalue) {
			if (TRUE == b_continue) {
				goto CONTINUE_WAITING;
			} else {
				return FTSTREAM_PARSER_READ_FAIL;
			}
		}
		return FTSTREAM_PARSER_READ_OK;
	case PT_CLSID:
		ppropval->pvalue = common_util_alloc(sizeof(GUID));
		if (NULL == ppropval->pvalue) {
			return FTSTREAM_PARSER_READ_FAIL;
		}
		if (!ftstream_parser_read_guid(pstream, static_cast<GUID *>(ppropval->pvalue)))
			return FTSTREAM_PARSER_READ_FAIL;
		return FTSTREAM_PARSER_READ_OK;
	case PT_SVREID:
		ppropval->pvalue = common_util_alloc(sizeof(SVREID));
		if (NULL == ppropval->pvalue) {
			return FTSTREAM_PARSER_READ_FAIL;
		}
		if (!ftstream_parser_read_svreid(pstream,
		    static_cast<SVREID *>(ppropval->pvalue), &b_continue)) {
			if (TRUE == b_continue) {
				goto CONTINUE_WAITING;
			} else {
				return FTSTREAM_PARSER_READ_FAIL;
			}
		}
		return FTSTREAM_PARSER_READ_OK;
	case PT_OBJECT:
	case PT_BINARY:
		ppropval->pvalue = common_util_alloc(sizeof(BINARY));
		if (NULL == ppropval->pvalue) {
			return FTSTREAM_PARSER_READ_FAIL;
		}
		if (!ftstream_parser_read_binary(pstream,
		    static_cast<BINARY *>(ppropval->pvalue), &b_continue)) {
			if (TRUE == b_continue) {
				goto CONTINUE_WAITING;
			} else {
				return FTSTREAM_PARSER_READ_FAIL;
			}
		}
		return FTSTREAM_PARSER_READ_OK;
	case PT_MV_SHORT:
		ppropval->pvalue =
			common_util_alloc(sizeof(SHORT_ARRAY));
		if (NULL == ppropval->pvalue) {
			return FTSTREAM_PARSER_READ_FAIL;
		}
		if (FALSE == ftstream_parser_read_uint32(
			pstream, &count)) {
			return FTSTREAM_PARSER_READ_FAIL;
		}
		if (count*sizeof(uint16_t) > 0x10000) {
			return FTSTREAM_PARSER_READ_FAIL;
		}
		if (pstream->st_size < count*sizeof(uint16_t) +
			pstream->offset) {
			goto CONTINUE_WAITING;
		}
		((SHORT_ARRAY*)ppropval->pvalue)->count = count;
		if (0 == count) {
			((SHORT_ARRAY*)ppropval->pvalue)->ps = NULL;
		} else {
			((SHORT_ARRAY*)ppropval->pvalue)->ps =
				static_cast<uint16_t *>(common_util_alloc(sizeof(uint16_t) * count));
			if (NULL == ((SHORT_ARRAY*)ppropval->pvalue)->ps) {
				return FTSTREAM_PARSER_READ_FAIL;
			}
		}
		for (i=0; i<count; i++) {
			if (FALSE == ftstream_parser_read_uint16(pstream,
				((SHORT_ARRAY*)ppropval->pvalue)->ps + i)) {
				return FTSTREAM_PARSER_READ_FAIL;	
			}
		}
		return FTSTREAM_PARSER_READ_OK;
	case PT_MV_LONG:
		ppropval->pvalue =
			common_util_alloc(sizeof(LONG_ARRAY));
		if (NULL == ppropval->pvalue) {
			return FTSTREAM_PARSER_READ_FAIL;
		}
		if (FALSE == ftstream_parser_read_uint32(
			pstream, &count)) {
			return FTSTREAM_PARSER_READ_FAIL;
		}
		if (count*sizeof(uint32_t) > 0x10000) {
			return FTSTREAM_PARSER_READ_FAIL;
		}
		if (pstream->st_size < count*sizeof(uint32_t) +
			pstream->offset) {
			goto CONTINUE_WAITING;
		}
		((LONG_ARRAY*)ppropval->pvalue)->count = count;
		if (0 == count) {
			((LONG_ARRAY*)ppropval->pvalue)->pl = NULL;
		} else {
			((LONG_ARRAY*)ppropval->pvalue)->pl =
				static_cast<uint32_t *>(common_util_alloc(sizeof(uint32_t) * count));
			if (NULL == ((LONG_ARRAY*)ppropval->pvalue)->pl) {
				return FTSTREAM_PARSER_READ_FAIL;
			}
		}
		for (i=0; i<count; i++) {
			if (FALSE == ftstream_parser_read_uint32(pstream,
				((LONG_ARRAY*)ppropval->pvalue)->pl + i)) {
				return FTSTREAM_PARSER_READ_FAIL;	
			}
		}
		return FTSTREAM_PARSER_READ_OK;
	case PT_MV_I8:
		ppropval->pvalue =
			common_util_alloc(sizeof(LONGLONG_ARRAY));
		if (NULL == ppropval->pvalue) {
			return FTSTREAM_PARSER_READ_FAIL;
		}
		if (FALSE == ftstream_parser_read_uint32(
			pstream, &count)) {
			return FTSTREAM_PARSER_READ_FAIL;
		}
		if (count*sizeof(uint64_t) > 0x10000) {
			return FTSTREAM_PARSER_READ_FAIL;
		}
		if (pstream->st_size < count*sizeof(uint64_t) +
			pstream->offset) {
			goto CONTINUE_WAITING;
		}
		((LONGLONG_ARRAY*)ppropval->pvalue)->count = count;
		if (0 == count) {
			((LONGLONG_ARRAY*)ppropval->pvalue)->pll = NULL;
		} else {
			((LONGLONG_ARRAY*)ppropval->pvalue)->pll =
				static_cast<uint64_t *>(common_util_alloc(sizeof(uint64_t) * count));
			if (NULL == ((LONGLONG_ARRAY*)ppropval->pvalue)->pll) {
				return FTSTREAM_PARSER_READ_FAIL;
			}
		}
		for (i=0; i<count; i++) {
			if (FALSE == ftstream_parser_read_uint64(pstream,
				((LONGLONG_ARRAY*)ppropval->pvalue)->pll + i)) {
				return FTSTREAM_PARSER_READ_FAIL;	
			}
		}
		return FTSTREAM_PARSER_READ_OK;
	case PT_MV_STRING8:
		ppropval->pvalue =
			common_util_alloc(sizeof(STRING_ARRAY));
		if (NULL == ppropval->pvalue) {
			return FTSTREAM_PARSER_READ_FAIL;
		}
		if (FALSE == ftstream_parser_read_uint32(
			pstream, &count)) {
			return FTSTREAM_PARSER_READ_FAIL;
		}
		if (pstream->st_size == pstream->offset) {
			goto CONTINUE_WAITING;
		}
		((STRING_ARRAY*)ppropval->pvalue)->count = count;
		if (0 == count) {
			((STRING_ARRAY*)ppropval->pvalue)->ppstr = NULL;
		} else {
			((STRING_ARRAY*)ppropval->pvalue)->ppstr =
				static_cast<char **>(common_util_alloc(sizeof(char *) * count));
			if (NULL == ((STRING_ARRAY*)
				ppropval->pvalue)->ppstr) {
				return FTSTREAM_PARSER_READ_FAIL;
			}
		}
		for (i=0; i<count; i++) {
			((STRING_ARRAY*)ppropval->pvalue)->ppstr[i] =
				ftstream_parser_read_string(pstream, &b_continue);
			if (NULL == ((STRING_ARRAY*)
				ppropval->pvalue)->ppstr[i]) {
				if (TRUE == b_continue) {
					if (pstream->offset - origin_offset > 0x10000) {
						return FTSTREAM_PARSER_READ_FAIL;
					}
					goto CONTINUE_WAITING;
				} else {
					return FTSTREAM_PARSER_READ_FAIL;
				}
			}
			if (pstream->st_size == pstream->offset) {
				if (pstream->offset - origin_offset > 0x10000) {
					return FTSTREAM_PARSER_READ_FAIL;
				}
				goto CONTINUE_WAITING;
			}
		}
		return FTSTREAM_PARSER_READ_OK;
	case PT_MV_UNICODE:
		ppropval->pvalue =
			common_util_alloc(sizeof(STRING_ARRAY));
		if (NULL == ppropval->pvalue) {
			return FTSTREAM_PARSER_READ_FAIL;
		}
		if (FALSE == ftstream_parser_read_uint32(
			pstream, &count)) {
			return FTSTREAM_PARSER_READ_FAIL;
		}
		if (pstream->st_size == pstream->offset) {
			goto CONTINUE_WAITING;
		}
		((STRING_ARRAY*)ppropval->pvalue)->count = count;
		if (0 == count) {
			((STRING_ARRAY*)ppropval->pvalue)->ppstr = NULL;
		} else {
			((STRING_ARRAY*)ppropval->pvalue)->ppstr =
				static_cast<char **>(common_util_alloc(sizeof(char *) * count));
			if (NULL == ((STRING_ARRAY*)
				ppropval->pvalue)->ppstr) {
				return FTSTREAM_PARSER_READ_FAIL;
			}
		}
		for (i=0; i<count; i++) {
			((STRING_ARRAY*)ppropval->pvalue)->ppstr[i] =
				ftstream_parser_read_wstring(pstream, &b_continue);
			if (NULL == ((STRING_ARRAY*)
				ppropval->pvalue)->ppstr[i]) {
				if (TRUE == b_continue) {
					if (pstream->offset - origin_offset > 0x10000) {
						return FTSTREAM_PARSER_READ_FAIL;
					}
					goto CONTINUE_WAITING;
				} else {
					return FTSTREAM_PARSER_READ_FAIL;
				}
			}
			if (pstream->st_size == pstream->offset) {
				if (pstream->offset - origin_offset > 0x10000) {
					return FTSTREAM_PARSER_READ_FAIL;
				}
				goto CONTINUE_WAITING;
			}
		}
		return FTSTREAM_PARSER_READ_OK;
	case PT_MV_CLSID:
		ppropval->pvalue =
			common_util_alloc(sizeof(GUID_ARRAY));
		if (NULL == ppropval->pvalue) {
			return FTSTREAM_PARSER_READ_FAIL;
		}
		if (FALSE == ftstream_parser_read_uint32(
			pstream, &count)) {
			return FTSTREAM_PARSER_READ_FAIL;
		}
		if (16*count > 0x10000) {
			return FTSTREAM_PARSER_READ_FAIL;
		}
		if (pstream->st_size < 16*count + pstream->offset) {
			goto CONTINUE_WAITING;
		}
		((GUID_ARRAY*)ppropval->pvalue)->count = count;
		if (0 == count) {
			((GUID_ARRAY*)ppropval->pvalue)->pguid = NULL;
		} else {
			((GUID_ARRAY*)ppropval->pvalue)->pguid =
				static_cast<GUID *>(common_util_alloc(sizeof(GUID *) * count));
			if (NULL == ((GUID_ARRAY*)ppropval->pvalue)->pguid) {
				return FTSTREAM_PARSER_READ_FAIL;
			}
		}
		for (i=0; i<count; i++) {
			if (FALSE == ftstream_parser_read_guid(pstream,
				((GUID_ARRAY*)ppropval->pvalue)->pguid + i)) {
				return FTSTREAM_PARSER_READ_FAIL;	
			}
		}
		return FTSTREAM_PARSER_READ_OK;
	case PT_MV_BINARY: {
		ppropval->pvalue =
			common_util_alloc(sizeof(BINARY_ARRAY));
		auto ba = static_cast<BINARY_ARRAY *>(ppropval->pvalue);
		if (NULL == ppropval->pvalue) {
			return FTSTREAM_PARSER_READ_FAIL;
		}
		if (FALSE == ftstream_parser_read_uint32(
			pstream, &count)) {
			return FTSTREAM_PARSER_READ_FAIL;
		}
		if (pstream->st_size == pstream->offset) {
			goto CONTINUE_WAITING;
		}
		ba->count = count;
		if (0 == count) {
			ba->pbin = nullptr;
		} else {
			ba->pbin = static_cast<BINARY *>(common_util_alloc(sizeof(BINARY) * count));
			if (ba->pbin == nullptr)
				return FTSTREAM_PARSER_READ_FAIL;
		}
		for (i=0; i<count; i++) {
			if (!ftstream_parser_read_binary(pstream,
			    ba->pbin + i, &b_continue)) {
				if (TRUE == b_continue) {
					if (pstream->offset - origin_offset > 0x10000) {
						return FTSTREAM_PARSER_READ_FAIL;
					}
					goto CONTINUE_WAITING;
				} else {
					return FTSTREAM_PARSER_READ_FAIL;
				}
			}
			if (pstream->st_size == pstream->offset) {
				if (pstream->offset - origin_offset > 0x10000) {
					return FTSTREAM_PARSER_READ_FAIL;
				}
				goto CONTINUE_WAITING;
			}
		}
		return FTSTREAM_PARSER_READ_OK;
	}
	}
	return FTSTREAM_PARSER_READ_FAIL;
	
CONTINUE_WAITING:
	pstream->offset = origin_offset;
	return FTSTREAM_PARSER_READ_CONTINUE;
}

BOOL ftstream_parser_write_buffer(
	FTSTREAM_PARSER *pstream,
	const BINARY *ptransfer_data)
{
	lseek(pstream->fd, 0, SEEK_END);
	if (ptransfer_data->cb != write(pstream->fd,
		ptransfer_data->pb, ptransfer_data->cb)) {
		return FALSE;	
	}
	pstream->st_size += ptransfer_data->cb;
	return TRUE;
}

static BOOL ftstream_parser_truncate_fd(
	FTSTREAM_PARSER *pstream)
{
	uint32_t len;
	char buff[0x10000];
	
	if (0 == pstream->offset) {
		return TRUE;
	}
	if (pstream->st_size == pstream->offset) {
		ftruncate(pstream->fd, 0);
		lseek(pstream->fd, 0, SEEK_SET);
		pstream->st_size = 0;
		pstream->offset = 0;
		return TRUE;
	}
	lseek(pstream->fd, pstream->offset, SEEK_SET);
	len = read(pstream->fd, buff, sizeof(buff));
	if (len <= 0) {
		return FALSE;
	}
	ftruncate(pstream->fd, 0);
	lseek(pstream->fd, 0, SEEK_SET);
	if (len != write(pstream->fd, buff, len)) {
		return FALSE;
	}
	pstream->st_size = len;
	pstream->offset = 0;
	return TRUE;
}

gxerr_t ftstream_parser_process(FTSTREAM_PARSER *pstream,
    RECORD_MARKER record_marker, RECORD_PROPVAL record_propval, void *pparam)
{
	int len;
	void *pvalue;
	uint32_t marker;
	uint16_t proptype;
	uint16_t codepage;
	TAGGED_PROPVAL propval;
	
	lseek(pstream->fd, 0, SEEK_SET);
	pstream->offset = 0;
	while (TRUE) {
		switch (ftstream_parser_read_element(
			pstream, &marker, &propval)) {
		case FTSTREAM_PARSER_READ_OK: {
			if (0 != marker) {
				gxerr_t err = record_marker(static_cast<FASTUPCTX_OBJECT *>(pparam), marker);
				if (err != GXERR_SUCCESS)
					return err;
				break;
			}
			proptype = PROP_TYPE(propval.proptag);
			if (proptype & 0x8000) {
				codepage = proptype & 0x7FFF;
				len = 2 * strlen(static_cast<char *>(propval.pvalue)) + 2;
				pvalue = common_util_alloc(len);
				if (pvalue == nullptr || common_util_mb_to_utf8(codepage,
				    static_cast<char *>(propval.pvalue),
				    static_cast<char *>(pvalue), len) <= 0) {
					propval.proptag = CHANGE_PROP_TYPE(propval.proptag, PT_STRING8);
				} else {
					propval.proptag = CHANGE_PROP_TYPE(propval.proptag, PT_UNICODE);
					propval.pvalue = pvalue;
				}
			}
			gxerr_t err = record_propval(static_cast<FASTUPCTX_OBJECT *>(pparam), &propval);
			if (err != GXERR_SUCCESS)
				return err;
			break;
		}
		case FTSTREAM_PARSER_READ_CONTINUE:
			return ftstream_parser_truncate_fd(pstream) == TRUE ?
			       GXERR_SUCCESS : GXERR_CALL_FAILED;
		default:
			return GXERR_CALL_FAILED;
		}
	}
}

FTSTREAM_PARSER* ftstream_parser_create(LOGON_OBJECT *plogon)
{
	int stream_id;
	char path[256];
	DCERPC_INFO rpc_info;
	struct stat node_stat;
	
	auto pstream = static_cast<FTSTREAM_PARSER *>(malloc(sizeof(FTSTREAM_PARSER)));
	if (NULL == pstream) {
		return NULL;
	}
	pstream->offset = 0;
	pstream->st_size = 0;
	stream_id = common_util_get_ftstream_id();
	rpc_info = get_rpc_info();
	sprintf(path, "%s/tmp/faststream", rpc_info.maildir);
	if (0 != stat(path, &node_stat)) {
		mkdir(path, 0777);
	} else {
		if (0 == S_ISDIR(node_stat.st_mode)) {
			remove(path);
			mkdir(path, 0777);
		}
	}
	sprintf(pstream->path, "%s/%d.%s", path, stream_id, get_host_ID());
	pstream->fd = open(path, O_CREAT|O_RDWR|O_TRUNC, 0666);
	if (-1 == pstream->fd) {
		free(pstream);
		return NULL;
	}
	pstream->plogon = plogon;
	return pstream;
}

void ftstream_parser_free(FTSTREAM_PARSER *pstream)
{
	close(pstream->fd);
	remove(pstream->path);
	free(pstream);
}

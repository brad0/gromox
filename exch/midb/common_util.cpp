// SPDX-License-Identifier: GPL-2.0-only WITH linking exception
#include <cstdint>
#include <libHX/string.h>
#include <gromox/alloc_context.hpp>
#include <gromox/defs.h>
#include "exmdb_client.h"
#include "common_util.h"
#include <gromox/ext_buffer.hpp>
#include <gromox/fileio.h>
#include <gromox/rop_util.hpp>
#include <gromox/pcl.hpp>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <pthread.h>
#include <cstring>
#include <cstdlib>

using namespace gromox;

namespace {
struct COMMAND_CONTEXT {
	ALLOC_CONTEXT alloc_ctx;
	ALLOC_CONTEXT *ptmp_ctx;
	char maildir[256];
};
}

static thread_local COMMAND_CONTEXT *g_ctx_key;
static thread_local unsigned int g_ctx_refcount;

BOOL common_util_build_environment(const char *maildir)
{
	/*
	 * cu_build_env is already called by midb, and then it _may_ occur
	 * another time during exmdb_client_connect_exmdb, but the latter only
	 * cares about exrpc_alloc succeeding, not the maildir.
	 */
	if (++g_ctx_refcount > 1) {
		if (*maildir != '\0' && strcmp(g_ctx_key->maildir, maildir) != 0)
			fprintf(stderr, "W-1901: T%lu: g_ctx_key->maildir mismatch %s vs %s\n",
			        gx_gettid(), g_ctx_key->maildir, maildir);
		return TRUE;
	}
	auto pctx = me_alloc<COMMAND_CONTEXT>();
	if (NULL == pctx) {
		return FALSE;
	}
	alloc_context_init(&pctx->alloc_ctx);
	pctx->ptmp_ctx = NULL;
	gx_strlcpy(pctx->maildir, maildir, GX_ARRAY_SIZE(pctx->maildir));
	g_ctx_key = pctx;
	return TRUE;
}

void common_util_free_environment()
{
	if (--g_ctx_refcount > 0)
		return;
	auto pctx = g_ctx_key;
	if (NULL == pctx) {
		return;
	}
	alloc_context_free(&pctx->alloc_ctx);
	if (NULL != pctx->ptmp_ctx) {
		alloc_context_free(pctx->ptmp_ctx);
		free(pctx->ptmp_ctx);
		pctx->ptmp_ctx = NULL;
	}
	free(pctx);
	g_ctx_key = nullptr;
}

void* common_util_alloc(size_t size)
{
	auto pctx = g_ctx_key;
	if (NULL == pctx) {
		return NULL;
	}
	if (NULL != pctx->ptmp_ctx) {
		return alloc_context_alloc(pctx->ptmp_ctx, size);
	}
	return alloc_context_alloc(&pctx->alloc_ctx, size);
}

BOOL common_util_switch_allocator()
{
	auto pctx = g_ctx_key;
	if (NULL == pctx) {
		return FALSE;
	}
	if (NULL != pctx->ptmp_ctx) {
		alloc_context_free(pctx->ptmp_ctx);
		free(pctx->ptmp_ctx);
		pctx->ptmp_ctx = NULL;
	} else {
		pctx->ptmp_ctx = me_alloc<ALLOC_CONTEXT>();
		if (NULL == pctx->ptmp_ctx) {
			return FALSE;
		}
		alloc_context_init(pctx->ptmp_ctx);
	}
	return TRUE;
}

void common_util_set_maildir(const char *maildir)
{
	auto pctx = g_ctx_key;
	if (NULL != pctx) {
		gx_strlcpy(pctx->maildir, maildir, GX_ARRAY_SIZE(pctx->maildir));
	}
}

const char* common_util_get_maildir()
{
	auto pctx = g_ctx_key;
	if (NULL == pctx) {
		return NULL;
	}
	return pctx->maildir;
}

char* common_util_dup(const char *pstr)
{
	int len;

	len = strlen(pstr) + 1;
	auto pstr1 = static_cast<char *>(common_util_alloc(len));
	if (NULL == pstr1) {
		return NULL;
	}
	memcpy(pstr1, pstr, len);
	return pstr1;
}

BINARY *cu_xid_to_bin(const XID &xid)
{
	EXT_PUSH ext_push;

	auto pbin = cu_alloc<BINARY>();
	if (NULL == pbin) {
		return NULL;
	}
	pbin->pv = common_util_alloc(24);
	if (pbin->pv == nullptr || !ext_push.init(pbin->pv, 24, 0) ||
	    ext_push.p_xid(xid) != EXT_ERR_SUCCESS)
		return NULL;
	pbin->cb = ext_push.m_offset;
	return pbin;
}

static BOOL common_util_binary_to_xid(const BINARY *pbin, XID *pxid)
{
	EXT_PULL ext_pull;

	if (pbin->cb < 17 || pbin->cb > 24) {
		return FALSE;
	}
	ext_pull.init(pbin->pb, pbin->cb, common_util_alloc, 0);
	return ext_pull.g_xid(pbin->cb, pxid) == EXT_ERR_SUCCESS ? TRUE : false;
}

BINARY* common_util_pcl_append(const BINARY *pbin_pcl,
	const BINARY *pchange_key)
{
	auto pbin = cu_alloc<BINARY>();
	if (NULL == pbin) {
		return NULL;
	}
	PCL ppcl;
	if (pbin_pcl != nullptr && !ppcl.deserialize(pbin_pcl))
		return nullptr;
	XID xid;
	xid.size = pchange_key->cb;
	if (!common_util_binary_to_xid(pchange_key, &xid))
		return NULL;
	if (!ppcl.append(xid))
		return NULL;
	auto ptmp_bin = ppcl.serialize();
	ppcl.clear();
	if (NULL == ptmp_bin) {
		return NULL;
	}
	pbin->cb = ptmp_bin->cb;
	pbin->pv = common_util_alloc(ptmp_bin->cb);
	if (pbin->pv == nullptr) {
		rop_util_free_binary(ptmp_bin);
		return NULL;
	}
	memcpy(pbin->pv, ptmp_bin->pb, pbin->cb);
	rop_util_free_binary(ptmp_bin);
	return pbin;
}

BOOL common_util_create_folder(const char *dir, int user_id,
	uint64_t parent_id, const char *folder_name, uint64_t *pfolder_id)
{
	BINARY *pbin;
	BINARY tmp_bin;
	EXT_PUSH ext_push;
	uint64_t last_time;
	char tmp_buff[128];
	uint64_t change_num;
	TPROPVAL_ARRAY tmp_propvals;
	TAGGED_PROPVAL propval_buff[9];
	
	if (!exmdb_client::allocate_cn(dir, &change_num)) {
		return FALSE;
	}
	uint32_t tmp_type = FOLDER_GENERIC;
	last_time = rop_util_unix_to_nttime(time(NULL));
	tmp_propvals.count = 9;
	tmp_propvals.ppropval = propval_buff;
	propval_buff[0].proptag = PidTagParentFolderId;
	propval_buff[0].pvalue = &parent_id;
	propval_buff[1].proptag = PR_FOLDER_TYPE;
	propval_buff[1].pvalue = &tmp_type;
	propval_buff[2].proptag = PR_DISPLAY_NAME;
	propval_buff[2].pvalue = deconst(folder_name);
	propval_buff[3].proptag = PR_CONTAINER_CLASS;
	propval_buff[3].pvalue  = deconst("IPF.Note");
	propval_buff[4].proptag = PR_CREATION_TIME;
	propval_buff[4].pvalue = &last_time;
	propval_buff[5].proptag = PR_LAST_MODIFICATION_TIME;
	propval_buff[5].pvalue = &last_time;
	propval_buff[6].proptag = PidTagChangeNumber;
	propval_buff[6].pvalue = &change_num;
	XID xid{rop_util_make_user_guid(user_id), change_num};
	if (!ext_push.init(tmp_buff, sizeof(tmp_buff), 0) ||
	    ext_push.p_xid(xid) != EXT_ERR_SUCCESS)
		return false;
	tmp_bin.pv = tmp_buff;
	tmp_bin.cb = ext_push.m_offset;
	propval_buff[7].proptag = PR_CHANGE_KEY;
	propval_buff[7].pvalue = &tmp_bin;
	PCL ppcl;
	if (!ppcl.append(xid))
		return FALSE;
	pbin = ppcl.serialize();
	if (NULL == pbin) {
		return FALSE;
	}
	ppcl.clear();
	propval_buff[8].proptag = PR_PREDECESSOR_CHANGE_LIST;
	propval_buff[8].pvalue = pbin;
	if (!exmdb_client::create_folder_by_properties(
		dir, 0, &tmp_propvals, pfolder_id)) {
		rop_util_free_binary(pbin);
		return FALSE;
	}
	rop_util_free_binary(pbin);
	if (0 == *pfolder_id) {
		return FALSE;
	}
	return TRUE;
}

BOOL common_util_get_propids(const PROPNAME_ARRAY *ppropnames,
	PROPID_ARRAY *ppropids)
{
	return exmdb_client::get_named_propids(
		common_util_get_maildir(), FALSE,
		ppropnames, ppropids);
}

BOOL common_util_get_propids_create(const PROPNAME_ARRAY *names, PROPID_ARRAY *ids)
{
	return exmdb_client::get_named_propids(common_util_get_maildir(),
	       TRUE, names, ids);
}

BOOL common_util_get_propname(
	uint16_t propid, PROPERTY_NAME **pppropname)
{
	PROPID_ARRAY propids;
	PROPNAME_ARRAY propnames;
	
	propids.count = 1;
	propids.ppropid = &propid;
	if (!exmdb_client::get_named_propnames(
		common_util_get_maildir(), &propids, &propnames)) {
		return FALSE;	
	}
	if (0 == propnames.count) {
		*pppropname = NULL;
	}
	*pppropname = propnames.ppropname;
	return TRUE;
}

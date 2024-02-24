// SPDX-License-Identifier: GPL-2.0-only WITH linking exception
#include <algorithm>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <dirent.h>
#include <fcntl.h>
#include <map>
#include <memory>
#include <string>
#include <unistd.h>
#include <utility>
#include <libHX/option.h>
#include <libHX/string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <gromox/bounce_gen.hpp>
#include <gromox/defs.h>
#include <gromox/dsn.hpp>
#include <gromox/element_data.hpp>
#include <gromox/fileio.h>
#include <gromox/mail.hpp>
#include <gromox/mail_func.hpp>
#include <gromox/mime.hpp>
#include <gromox/rop_util.hpp>
#include <gromox/scope.hpp>
#include <gromox/textmaps.hpp>
#include <gromox/util.hpp>

namespace {

using namespace std::string_literals;
using namespace gromox;
using buff_t = bool (*)(const char *, char *, size_t);

static bool bounce_producer_make_content(buff_t gul,
    const char *username, MESSAGE_CONTENT *pbrief, const char *bounce_type,
    std::string &subject, std::string &cttype, std::string &content)
{
	char charset[32], date_buff[128], lang[32];

	charset[0] = '\0';
	auto tsptr = pbrief->proplist.get<const uint64_t>(PR_CLIENT_SUBMIT_TIME);
	auto ts = tsptr == nullptr ? time(nullptr) : rop_util_nttime_to_unix(*tsptr);
	auto from = pbrief->proplist.get<const char>(PR_SENT_REPRESENTING_SMTP_ADDRESS);
	if (from == nullptr)
		from = "";
	if (gul(from, lang, std::size(lang)))
		gx_strlcpy(charset, znul(lang_to_charset(lang)), std::size(charset));
	rfc1123_dstring(date_buff, std::size(date_buff), ts);
	auto message_size = pbrief->proplist.get<const uint32_t>(PR_MESSAGE_SIZE);
	if (message_size == nullptr)
		return false;
	if (*charset == '\0') {
		auto cpid = pbrief->proplist.get<const uint32_t>(PR_INTERNET_CPID);
		if (cpid == nullptr) {
			strcpy(charset, "ascii");
		} else {
			auto pcharset = cpid_to_cset(static_cast<cpid_t>(*cpid));
			gx_strlcpy(charset, pcharset != nullptr ? pcharset : "ascii", std::size(charset));
		}
	}

	auto tpptr = bounce_gen_lookup(charset, bounce_type);
	if (tpptr == nullptr)
		return false;
	auto &tp = *tpptr;
	auto fa = HXformat_init();
	if (fa == nullptr)
		return false;
	auto cl_0 = make_scope_exit([&]() { HXformat_free(fa); });
	unsigned int immed = HXFORMAT_IMMED;
	if (HXformat_add(fa, "time", date_buff,
	    HXTYPE_STRING | immed) < 0 ||
	    HXformat_add(fa, "from", from, HXTYPE_STRING) < 0 ||
	    HXformat_add(fa, "user", username, HXTYPE_STRING) < 0 ||
	    HXformat_add(fa, "rcpts",
	    bounce_gen_rcpts(*pbrief->children.prcpts).c_str(),
	    HXTYPE_STRING | immed) < 0 ||
	    HXformat_add(fa, "postmaster", bounce_gen_postmaster(), HXTYPE_STRING) < 0)
		return false;
	auto subj = pbrief->proplist.get<const char>(PR_SUBJECT);
	if (HXformat_add(fa, "subject", subj != nullptr ? subj : "",
	    HXTYPE_STRING) < 0 ||
	    HXformat_add(fa, "parts",
	    bounce_gen_attachs(*pbrief->children.pattachments).c_str(),
	    HXTYPE_STRING | immed) < 0)
		return false;
	HX_unit_size(date_buff, std::size(date_buff), *message_size, 1000, 0);
	if (HXformat_add(fa, "length", date_buff, HXTYPE_STRING) < 0)
		return false;

	hxmc_t *replaced = nullptr;
	if (HXformat_aprintf(fa, &replaced, &tp.content[tp.body_start]) < 0)
		return false;
	auto cl_1 = make_scope_exit([&]() { HXmc_free(replaced); });
	content = replaced;
	subject = tp.subject;
	cttype  = tp.content_type;
	return true;
}

bool exch_bouncer_make(buff_t gudn, buff_t gul,
    const char *username, MESSAGE_CONTENT *pbrief,
    const char *bounce_type, MAIL *pmail) try
{
	size_t out_len;
	char mime_to[1024], tmp_buff[1024], date_buff[128];
	char mime_from[1024];
	
	if (gudn(username, tmp_buff, std::size(tmp_buff)) && *tmp_buff != '\0') {
		strcpy(mime_from, "=?utf-8?b?");
		encode64(tmp_buff, strlen(tmp_buff), &mime_from[10],
			std::size(mime_from) - 13, &out_len);
		strcpy(&mime_from[10+out_len], "?=");
	} else {
		*mime_from = '\0';
	}
	std::string subject, content_type, content_buff;
	if (!bounce_producer_make_content(gul, username, pbrief,
	    bounce_type, subject, content_type, content_buff))
		return false;
	auto phead = pmail->add_head();
	if (phead == nullptr)
		return false;
	auto pmime = phead;
	pmime->set_content_type("multipart/report");
	pmime->set_content_param("report-type", "disposition-notification");
	auto bv = pbrief->proplist.get<const BINARY>(PR_CONVERSATION_INDEX);
	if (bv != nullptr && encode64(bv->pb, bv->cb, tmp_buff,
	    std::size(tmp_buff), &out_len) == 0)
		pmime->set_field("Thread-Index", tmp_buff);
	auto t_addr = "\""s + mime_from + "\" <" + username + ">";
	pmime->set_field("From", t_addr.c_str());
	t_addr = "<"s + username + ">";
	auto str = pbrief->proplist.get<const char>(PR_SENT_REPRESENTING_NAME);
	if (str != nullptr && *str != '\0') {
		strcpy(mime_to, "\"=?utf-8?b?");
		encode64(str, strlen(str), mime_to + 11,
			sizeof(mime_to) - 15, &out_len);
		strcpy(mime_to + 11 + out_len, "?=\"");
	} else {
		mime_to[0] = '\0';
	}
	str = pbrief->proplist.get<char>(PR_SENT_REPRESENTING_SMTP_ADDRESS);
	if (str != nullptr) {
		out_len = strlen(mime_to);
		if (out_len != 0)
			mime_to[out_len++] = ' ';
		snprintf(mime_to + out_len, sizeof(mime_to) - out_len, "<%s>", str);
	}
	if (*mime_to != '\0')
		pmime->set_field("To", mime_to);
	pmime->set_field("MIME-Version", "1.0");
	pmime->set_field("X-Auto-Response-Suppress", "All");
	rfc1123_dstring(date_buff, std::size(date_buff), 0);
	pmime->set_field("Date", date_buff);
	pmime->set_field("Subject", subject.c_str());
	pmime = pmail->add_child(phead, MIME_ADD_FIRST);
	if (pmime == nullptr)
		return false;
	pmime->set_content_type(content_type.c_str());
	pmime->set_content_param("charset", "\"utf-8\"");
	if (!pmime->write_content(content_buff.c_str(),
	    content_buff.size(), mime_encoding::automatic))
		return false;

	DSN dsn;
	auto pdsn_fields = dsn.get_message_fields();
	t_addr = "rfc822;"s + username;
	dsn.append_field(pdsn_fields, "Final-Recipient", t_addr.c_str());
	if (strcmp(bounce_type, "BOUNCE_NOTIFY_READ") == 0)
		dsn.append_field(pdsn_fields, "Disposition",
			"automatic-action/MDN-sent-automatically; displayed");
	else if (strcmp(bounce_type, "BOUNCE_NOTIFY_NON_READ") == 0)
		dsn.append_field(pdsn_fields, "Disposition",
			"manual-action/MDN-sent-automatically; deleted");
	str = pbrief->proplist.get<char>(PR_INTERNET_MESSAGE_ID);
	if (str != nullptr)
		dsn.append_field(pdsn_fields, "Original-Message-ID", str);
	bv = pbrief->proplist.get<BINARY>(PR_PARENT_KEY);
	if (bv != nullptr) {
		encode64(bv->pb, bv->cb, tmp_buff, std::size(tmp_buff), &out_len);
		dsn.append_field(pdsn_fields,
			"X-MSExch-Correlation-Key", tmp_buff);
	}
	if (*mime_from != '\0')
		dsn.append_field(pdsn_fields, "X-Display-Name", mime_from);
	content_buff.clear();
	content_buff.resize(256 * 1024);
	if (dsn.serialize(content_buff.data(), content_buff.size())) {
		content_buff.resize(strnlen(content_buff.c_str(), content_buff.size()));
		pmime = pmail->add_child(phead, MIME_ADD_LAST);
		if (NULL != pmime) {
			pmime->set_content_type("message/disposition-notification");
			pmime->write_content(content_buff.c_str(),
				content_buff.size(), mime_encoding::none);
		}
	}
	return true;
} catch (const std::bad_alloc &) {
	mlog(LV_ERR, "E-1482: ENOMEM");
	return false;
}

}

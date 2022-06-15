#pragma once
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#ifdef COMPILE_DIAG
#	include <stdexcept>
#endif
#include <string>
#include <type_traits>
#define SOCKET_TIMEOUT 60
namespace gromox {
template<typename T, size_t N> constexpr inline size_t arsizeof(T (&)[N]) { return N; }
#define GX_ARRAY_SIZE arsizeof
}
#define GX_EXPORT __attribute__((visibility("default")))
#define NOMOVE(K) \
	K(K &&) = delete; \
	void operator=(K &&) = delete;

/*
 * The timezone column in the user database ought to be never empty. Having an
 * unusual fallback offset means that missing TZ problems will readily be
 * visible in UIs.
 */
#define GROMOX_FALLBACK_TIMEZONE "Pacific/Chatham"

enum gxerr_t {
	GXERR_SUCCESS = 0,
	GXERR_CALL_FAILED,
	GXERR_OVER_QUOTA,
};

enum ec_error_t {
	ecSuccess = 0,
	// MAPI_E_USER_ABORT = 0x1,
	// MAPI_E_FAILURE = 0x2,
	// MAPI_E_LOGON_FAILURE = 0x3,
	MAPI_E_DISK_FULL = 0x4,
	// MAPI_E_INSUFFICIENT_MEMORY = 0x5,
	// MAPI_E_ACCESS_DENIED = 0x6,
	// MAPI_E_TOO_MANY_SESSIONS = 0x8,
	// MAPI_E_TOO_MANY_FILES = 0x9,
	// MAPI_E_TOO_MANY_RECIPIENTS = 0xA,
	// MAPI_E_ATTACHMENT_NOT_FOUND = 0xB,
	// MAPI_E_ATTACHMENT_OPEN_FAILURE = 0xC,
	// MAPI_E_ATTACHMENT_WRITE_FAILURE = 0xD,
	// MAPI_E_UNKNOWN_RECIPIENT = 0xE,
	// MAPI_E_BAD_RECIPTYPE = 0xF,
	// MAPI_E_NO_MESSAGES = 0x10,
	// MAPI_E_INVALID_MESSAGE = 0x11,
	// MAPI_E_TEXT_TOO_LARGE = 0x12,
	// MAPI_E_INVALID_SESSION = 0x13,
	// MAPI_E_TYPE_NOT_SUPPORTED = 0x14,
	// MAPI_E_AMBIGUOUS_RECIPIENT = 0x15,
	// MAPI_E_MESSAGE_IN_USE = 0x16,
	// MAPI_E_NETWORK_FAILURE = 0x17,
	// MAPI_E_INVALID_EDITFIELDS = 0x18,
	// MAPI_E_INVALID_RECIPS = 0x19,
	// MAPI_E_NOT_SUPPORTED = 0x1A,
	// ecJetError = 0x000003EA,
	ecUnknownUser = 0x000003EB,
	// ecExiting = 0x000003ED,
	// ecBadConfig = 0x000003EE,
	// ecUnknownCodePage = 0x000003EF,
	ecServerOOM = 0x000003F0,
	ecLoginPerm = 0x000003F2,
	// ecDatabaseRolledBack = 0x000003F3,
	// ecDatabaseCopiedError = 0x000003F4,
	// ecAuditNotAllowed = 0x000003F5,
	// ecZombieUser = 0x000003F6,
	// ecUnconvertableACL = 0x000003F7,
	// ecNoFreeJses = 0x0000044C,
	// ecDifferentJses = 0x0000044D,
	// ecFileRemove = 0x0000044F,
	// ecParameterOverflow = 0x00000450,
	// ecBadVersion = 0x00000451,
	// ecTooManyCols = 0x00000452,
	// ecHaveMore = 0x00000453,
	// ecDatabaseError = 0x00000454,
	// ecIndexNameTooBig = 0x00000455,
	// ecUnsupportedProp = 0x00000456,
	// ecMsgNotSaved = 0x00000457,
	// ecUnpubNotif = 0x00000459,
	// ecDifferentRoot = 0x0000045B,
	// ecBadFolderName = 0x0000045C,
	// ecAttachOpen = 0x0000045D,
	// ecInvClpsState = 0x0000045E,
	// ecSkipMyChildren = 0x0000045F,
	// ecSearchFolder = 0x00000460,
	ecNotSearchFolder = 0x00000461,
	// ecFolderSetReceive = 0x00000462,
	ecNoReceiveFolder = 0x00000463,
	// ecNoDelSubmitMsg = 0x00000465,
	// ecInvalidRecips = 0x00000467,
	// ecNoReplicaHere = 0x00000468,
	// ecNoReplicaAvailable = 0x00000469,
	// ecPublicMDB = 0x0000046A,
	// ecNotPublicMDB = 0x0000046B,
	// ecRecordNotFound = 0x0000046C,
	// ecReplConflict = 0x0000046D,
	// ecFxBufferOverrun = 0x00000470,
	// ecFxBufferEmpty = 0x00000471,
	// ecFxPartialValue = 0x00000472,
	// ecFxNoRoom = 0x00000473,
	// ecMaxTimeExpired = 0x00000474,
	// ecDstError = 0x00000475,
	// ecMDBNotInit = 0x00000476,
	ecWrongServer = 0x00000478,
	ecBufferTooSmall = 0x0000047D,
	// ecRequiresRefResolve = 0x0000047E,
	// ecServerPaused = 0x0000047F,
	// ecServerBusy = 0x00000480,
	// ecNoSuchLogon = 0x00000481,
	// ecLoadLibFailed = 0x00000482,
	// ecObjAlreadyConfig = 0x00000483,
	// ecObjNotConfig = 0x00000484,
	// ecDataLoss = 0x00000485,
	// ecMaxSendThreadExceeded = 0x00000488,
	// ecFxErrorMarker = 0x00000489,
	// ecNoFreeJtabs = 0x0000048A,
	// ecNotPrivateMDB = 0x0000048B,
	// ecIsintegMDB = 0x0000048C,
	// ecRecoveryMDBMismatch = 0x0000048D,
	// ecTableMayNotBeDeleted = 0x0000048E,
	ecSearchFolderScopeViolation = 0x00000490,
	// ecRpcRegisterIf = 0x000004B1,
	// ecRpcListen = 0x000004B2,
	ecRpcFormat = 0x000004B6,
	// ecNoCopyTo = 0x000004B7,
	ecNullObject = 0x000004B9,
	// ecRpcAuthentication = 0x000004BC,
	// ecRpcBadAuthenticationLevel = 0x000004BD,
	// ecNullCommentRestriction = 0x000004BE,
	// ecRulesLoadError = 0x000004CC,
	// ecRulesDelivErr = 0x000004CD,
	// ecRulesParsingErr = 0x000004CE,
	// ecRulesCreateDaeErr = 0x000004CF,
	// ecRulesCreateDamErr = 0x000004D0,
	// ecRulesNoMoveCopyFolder = 0x000004D1,
	// ecRulesNoFolderRights = 0x000004D2,
	// ecMessageTooBig = 0x000004D4,
	// ecFormNotValid = 0x000004D5,
	// ecNotAuthorized = 0x000004D6,
	// ecDeleteMessage = 0x000004D7,
	// ecBounceMessage = 0x000004D8,
	ecQuotaExceeded = 0x000004D9,
	// ecMaxSubmissionExceeded = 0x000004DA,
	ecMaxAttachmentExceeded = 0x000004DB,
	// ecSendAsDenied = 0x000004DC,
	// ecShutoffQuotaExceeded = 0x000004DD,
	// ecMaxObjsExceeded = 0x000004DE,
	// ecClientVerDisallowed = 0x000004DF,
	// ecRpcHttpDisallowed = 0x000004E0,
	// ecCachedModeRequired = 0x000004E1,
	// ecFolderNotCleanedUp = 0x000004E3,
	// ecFmtError = 0x000004ED,
	ecNotExpanded = 0x000004F7,
	ecNotCollapsed = 0x000004F8,
	// ecLeaf = 0x000004F9,
	// ecUnregisteredNamedProp = 0x000004FA,
	// ecFolderDisabled = 0x000004FB,
	// ecDomainError = 0x000004FC,
	// ecNoCreateRight = 0x000004FF,
	// ecPublicRoot = 0x00000500,
	// ecNoReadRight = 0x00000501,
	// ecNoCreateSubfolderRight = 0x00000502,
	ecDstNullObject = 0x00000503,
	ecMsgCycle = 0x00000504,
	ecTooManyRecips = 0x00000505,
	// ecVirusScanInProgress = 0x0000050A,
	// ecVirusDetected = 0x0000050B,
	// ecMailboxInTransit = 0x0000050C,
	// ecBackupInProgress = 0x0000050D,
	// ecVirusMessageDeleted = 0x0000050E,
	// ecInvalidBackupSequence = 0x0000050F,
	// ecInvalidBackupType = 0x00000510,
	// ecTooManyBackupsInProgress = 0x00000511,
	// ecRestoreInProgress = 0x00000512,
	// ecDuplicateObject = 0x00000579,
	// ecObjectNotFound = 0x0000057A,
	// ecFixupReplyRule = 0x0000057B,
	// ecTemplateNotFound = 0x0000057C,
	// ecRuleException = 0x0000057D,
	// ecDSNoSuchObject = 0x0000057E,
	// ecMessageAlreadyTombstoned = 0x0000057F,
	// ecRequiresRWTransaction = 0x00000596,
	// ecPaused = 0x0000060E,
	// ecNotPaused = 0x0000060F,
	// ecWrongMailbox = 0x00000648,
	// ecChgPassword = 0x0000064C,
	// ecPwdExpired = 0x0000064D,
	// ecInvWkstn = 0x0000064E,
	// ecInvLogonHrs = 0x0000064F,
	// ecAcctDisabled = 0x00000650,
	// ecRuleVersion = 0x000006A4,
	// ecRuleFormat = 0x000006A5,
	// ecRuleSendAsDenied = 0x000006A6,
	// ecNoServerSupport = 0x000006B9,
	// ecLockTimedOut = 0x000006BA,
	// ecObjectLocked = 0x000006BB,
	// ecInvalidLockNamespace = 0x000006BD,
	RPC_X_BAD_STUB_DATA = 0x000006F7,
	// ecMessageDeleted = 0x000007D6,
	// ecProtocolDisabled = 0x000007D8,
	// ecClearTextLogonDisabled = 0x000007D9,
	ecRejected = 0x000007EE,
	// ecAmbiguousAlias = 0x0000089A,
	// ecUnknownMailbox = 0x0000089B,
	// ecExpReserved = 0x000008FC,
	// ecExpParseDepth = 0x000008FD,
	// ecExpFuncArgType = 0x000008FE,
	// ecExpSyntax = 0x000008FF,
	// ecExpBadStrToken = 0x00000900,
	// ecExpBadColToken = 0x00000901,
	// ecExpTypeMismatch = 0x00000902,
	// ecExpOpNotSupported = 0x00000903,
	// ecExpDivByZero = 0x00000904,
	// ecExpUnaryArgType = 0x00000905,
	// ecNotLocked = 0x00000960,
	// ecClientEvent = 0x00000961,
	// ecCorruptEvent = 0x00000965,
	// ecCorruptWatermark = 0x00000966,
	// ecEventError = 0x00000967,
	// ecWatermarkError = 0x00000968,
	// ecNonCanonicalACL = 0x00000969,
	// ecMailboxDisabled = 0x0000096C,
	// ecRulesFolderOverQuota = 0x0000096D,
	// ecADUnavailable = 0x0000096E,
	// ecADError = 0x0000096F,
	// ecNotEncrypted = 0x00000970,
	// ecADNotFound = 0x00000971,
	// ecADPropertyError = 0x00000972,
	// ecRpcServerTooBusy = 0x00000973,
	// ecRpcOutOfMemory = 0x00000974,
	// ecRpcServerOutOfMemory = 0x00000975,
	// ecRpcOutOfResources = 0x00000976,
	// ecRpcServerUnavailable = 0x00000977,
	// ecSecureSubmitError = 0x0000097A,
	// ecEventsDeleted = 0x0000097C,
	// ecSubsystemStopping = 0x0000097D,
	// ecSAUnavailable = 0x0000097E,
	// ecCIStopping = 0x00000A28,
	// ecFxInvalidState = 0x00000A29,
	// ecFxUnexpectedMarker = 0x00000A2A,
	// ecDuplicateDelivery = 0x00000A2B,
	// ecConditionViolation = 0x00000A2C,
	// ecMaxPoolExceeded = 0x00000A2D,
	// ecRpcInvalidHandle = 0x00000A2E,
	// ecEventNotFound = 0x00000A2F,
	// ecPropNotPromoted = 0x00000A30,
	// ecLowMdbSpace = 0x00000A31,
	// MAPI_W_NO_SERVICE = 0x00040203,
	ecWarnWithErrors = 0x00040380, /* MAPI_W_ERRORS_RETURNED */
	// ecWarnPositionChanged = 0x00040481, /* MAPI_W_POSITION_CHANGED */
	// ecWarnApproxCount = 0x00040482, /* MAPI_W_APPROX_COUNT */
	// MAPI_W_CANCEL_MESSAGE = 0x00040580,
	// ecPartialCompletion = 0x00040680, /* MAPI_W_PARTIAL_COMPLETION */
	// SYNC_W_PROGRESS = 0x00040820,
	SYNC_W_CLIENT_CHANGE_NEWER = 0x00040821,
	ecInterfaceNotSupported = 0x80004002, /* E_NOINTERFACE, MAPI_E_INTERFACE_NOT_SUPPORTED */
	ecError = 0x80004005, /* MAPI_E_CALL_FAILED */
	// STG_E_INVALIDFUNCTION = 0x80030001, /* STG := "storage" */
	STG_E_ACCESSDENIED = 0x80030005,
	// STG_E_INSUFFICIENTMEMORY = 0x80030008,
	// STG_E_INVALIDPOINTER = 0x80030009,
	StreamSeekError = 0x80030019,
	// STG_E_READFAULT = 0x8003001E,
	// STG_E_LOCKVIOLATION = 0x80030021,
	// STG_E_INVALIDPARAMETER = 0x80030057,
	// ecStreamSizeError = 0x80030070, /* STG_E_MEDIUMFULL */
	// STG_E_INVALIDFLAG = 0x800300FF,
	// STG_E_CANTSAVE = 0x80030103,
	ecNotSupported = 0x80040102, /* MAPI_E_NO_SUPPORT */
	// ecBadCharwidth = 0x80040103, /* MAPI_E_BAD_CHARWIDTH */
	// ecStringTooLarge = 0x80040105, /* MAPI_E_STRING_TOO_LONG */
	// ecUnknownFlags = 0x80040106, /* MAPI_E_UNKNOWN_FLAGS */
	// ecInvalidEntryId = 0x80040107, /* MAPI_E_INVALID_ENTRYID */
	ecInvalidObject = 0x80040108, /* MAPI_E_INVALID_OBJECT */
	ecObjectModified = 0x80040109, /* MAPI_E_OBJECT_CHANGED */
	// ecObjectDeleted = 0x8004010A, /* MAPI_E_OBJECT_DELETED */
	// ecBusy = 0x8004010B, /* MAPI_E_BUSY */
	// ecDiskFull = 0x8004010D, /* MAPI_E_NOT_ENOUGH_DISK */
	ecInsufficientResrc = 0x8004010E, /* MAPI_E_NOT_ENOUGH_RESOURCES */
	ecNotFound = 0x8004010F, /* MAPI_E_NOT_FOUND */
	// ecVersion = 0x80040110, /* MAPI_E_VERSION */
	ecLoginFailure = 0x80040111, /* MAPI_E_LOGON_FAILED */
	// ecTooManySessions = 0x80040112, /* MAPI_E_SESSION_LIMIT */
	// ecUserAbort = 0x80040113, /* MAPI_E_USER_CANCEL */
	ecUnableToAbort = 0x80040114, /* MAPI_E_UNABLE_TO_ABORT */
	ecRpcFailed = 0x80040115, /* MAPI_E_NETWORK_ERROR */
	// ecReadFault = 0x80040116, /* ecWriteFault, MAPI_E_DISK_ERROR */
	ecTooComplex = 0x80040117, /* MAPI_E_TOO_COMPLEX */
	// MAPI_E_BAD_COLUMN = 0x80040118,
	// MAPI_E_EXTENDED_ERROR = 0x80040119,
	ecComputed = 0x8004011A, /* MAPI_E_COMPUTED */
	// ecCorruptData = 0x8004011B, /* MAPI_E_CORRUPT_DATA */
	// MAPI_E_UNCONFIGURED = 0x8004011C,
	// MAPI_E_FAILONEPROVIDER = 0x8004011D,
	MAPI_E_UNKNOWN_CPID = 0x8004011E,
	MAPI_E_UNKNOWN_LCID = 0x8004011F,
	// MAPI_E_PASSWORD_CHANGE_REQUIRED = 0x80040120,
	// MAPI_E_PASSWORD_EXPIRED = 0x80040121,
	// MAPI_E_INVALID_WORKSTATION_ACCOUNT = 0x80040122,
	// ecTimeSkew = 0x80040123, /* MAPI_E_INVALID_ACCESS_TIME */
	// MAPI_E_ACCOUNT_DISABLED = 0x80040124,
	// MAPI_E_END_OF_SESSION = 0x80040200,
	// MAPI_E_UNKNOWN_ENTRYID = 0x80040201,
	// MAPI_E_MISSING_REQUIRED_COLUMN = 0x80040202,
	// ecPropBadValue = 0x80040301, /* MAPI_E_BAD_VALUE */
	// ecInvalidType = 0x80040302, /* MAPI_E_INVALID_TYPE */
	// ecTypeNotSupported = 0x80040303, /* MAPI_E_TYPE_NO_SUPPORT */
	// ecPropType = 0x80040304, /* MAPI_E_UNEXPECTED_TYPE */
	ecTooBig = 0x80040305, /* MAPI_E_TOO_BIG */
	MAPI_E_DECLINE_COPY = 0x80040306,
	// MAPI_E_UNEXPECTED_ID = 0x80040307,
	// ecUnableToComplete = 0x80040400, /* MAPI_E_UNABLE_TO_COMPLETE */
	// ecTimeout = 0x80040401, /* MAPI_E_TIMEOUT */
	// ecTableEmpty = 0x80040402, /* MAPI_E_TABLE_EMPTY */
	ecTableTooBig = 0x80040403, /* MAPI_E_TABLE_TOO_BIG */
	ecInvalidBookmark = 0x80040405, /* MAPI_E_INVALID_BOOKMARK */
	// ecWait = 0x80040500, /* MAPI_E_WAIT */
	// ecCancel = 0x80040501, /* MAPI_E_CANCEL */
	// MAPI_E_NOT_ME = 0x80040502,
	// MAPI_E_CORRUPT_STORE = 0x80040600,
	ecNotInQueue = 0x80040601, /* MAPI_E_NOT_IN_QUEUE */
	// MAPI_E_NO_SUPPRESS = 0x80040602,
	ecDuplicateName = 0x80040604, /* MAPI_E_COLLISION */
	ecNotInitialized = 0x80040605, /* MAPI_E_NOT_INITIALIZED */
	// MAPI_E_NON_STANDARD = 0x80040606,
	// MAPI_E_NO_RECIPIENTS = 0x80040607,
	// ecSubmitted = 0x80040608, /* MAPI_E_SUBMITTED */
	// ecFolderHasChildren = 0x80040609, /* MAPI_E_HAS_FOLDERS */
	// ecFolderHasContents = 0x8004060A, /* MAPI_E_HAS_MESSAGES */
	ecRootFolder = 0x8004060B, /* MAPI_E_FOLDER_CYCLE */
	MAPI_E_STORE_FULL = 0x8004060C,
	// ecLockIdLimit = 0x8004060D, /* MAPI_E_LOCKID_LIMIT */
	EC_EXCEEDED_SIZE = 0x80040610,
	ecAmbiguousRecip = 0x80040700, /* MAPI_E_AMBIGUOUS_RECIP */
	SYNC_E_OBJECT_DELETED = 0x80040800,
	SYNC_E_IGNORE = 0x80040801,
	SYNC_E_CONFLICT = 0x80040802,
	SYNC_E_NO_PARENT = 0x80040803,
	// SYNC_E_CYCLE_DETECTED = 0x80040804,
	// SYNC_E_UNSYNCHRONIZED = 0x80040805,
	ecNPQuotaExceeded = 0x80040900, /* MAPI_E_NAMED_PROP_QUOTA_EXCEEDED */
	NotImplemented = 0x80040FFF, /* _not_ the same as ecNotSupported/ecNotImplemented/MAPI_E_NOT_IMPLEMENTED */
	ecAccessDenied = 0x80070005, /* MAPI_E_NO_ACCESS */
	ecMAPIOOM = 0x8007000E, /* MAPI_E_NOT_ENOUGH_MEMORY */
	ecInvalidParam = 0x80070057, /* MAPI_E_INVALID_PARAMETER */
};

enum gx_loglevel {
	LV_CRIT = 1,
	LV_ERR = 2,
	LV_WARN = 3,
	LV_NOTICE = 4,
	LV_INFO = 5,
	LV_DEBUG = 6,
};

enum {
	ULCLPART_SIZE = 65, /* localpart(64) plus \0 */
	UDOM_SIZE = 256, /* domain(255) plus \0 */
	UADDR_SIZE = 321, /* localpart(64) "@" domain \0 */

	/*
	 * The name of a namedprop can be at most 254 UTF-16 chars as per
	 * OXCPRPT v17 §4.1.1. Since Gromox operates in UTF-8, that's a few
	 * more octets. (TNEF uses a larger, 32-bit field.)
	 */
	GUIDSTR_SIZE = 37,
	NP_NAMEBUF_SIZE = 763,
	NP_STRBUF_SIZE = 36 + 11 + NP_NAMEBUF_SIZE, /* "GUID=<>,NAME=<>" */
};

extern GX_EXPORT unsigned int gxerr_to_hresult(gxerr_t);
extern GX_EXPORT const char *mapi_strerror(unsigned int);

template<typename T> constexpr T *deconst(const T *x) { return const_cast<T *>(x); }
#undef roundup /* you naughty glibc */
template<typename T> constexpr T roundup(T x, T y) { return (x + y - 1) / y * y; }
template<typename T, typename U> constexpr auto strange_roundup(T x, U y) -> decltype(x / y) { return (x / y + 1) * y; }
#define SR_GROW_ATTACHMENT_CONTENT 20U
#define SR_GROW_EID_ARRAY 100U
#define SR_GROW_PROPTAG_ARRAY 100U
#define SR_GROW_TAGGED_PROPVAL 100U
#define SR_GROW_TPROPVAL_ARRAY 100U

#ifdef COMPILE_DIAG
/* snprintf takes about 2.65x the time, but we get -Wformat-truncation diagnostics */
#define gx_strlcpy(dst, src, dsize) snprintf((dst), (dsize), "%s", (src))
#else
#define gx_strlcpy(dst, src, dsize) HX_strlcpy((dst), (src), (dsize))
#endif

static inline constexpr bool is_nameprop_id(unsigned int i) { return i >= 0x8000 && i <= 0xFFFE; }

namespace gromox {

struct stdlib_delete {
	inline void operator()(void *x) const { free(x); }
};
template<typename T> static inline T *me_alloc() {
	static_assert(std::is_trivial_v<T> && std::is_trivially_destructible_v<T>);
	return static_cast<T *>(malloc(sizeof(T)));
}
template<typename T> static inline T *me_alloc(size_t elem) {
	static_assert(std::is_trivial_v<T> && std::is_trivially_destructible_v<T>);
	return static_cast<T *>(malloc(sizeof(T) * elem));
}
template<typename T> static inline T *re_alloc(void *x) {
	static_assert(std::is_trivial_v<T> && std::is_trivially_destructible_v<T>);
	return static_cast<T *>(realloc(x, sizeof(T)));
}
template<typename T> static inline T *re_alloc(void *x, size_t elem) {
	static_assert(std::is_trivial_v<T> && std::is_trivially_destructible_v<T>);
	return static_cast<T *>(realloc(x, sizeof(T) * elem));
}
static inline const char *snul(const std::string &s) { return s.size() != 0 ? s.c_str() : nullptr; }
static inline const char *znul(const char *s) { return s != nullptr ? s : ""; }

template<typename U, typename V> static int three_way_compare(U &&a, V &&b)
{
	return (a < b) ? -1 : (a == b) ? 0 : 1;
}

#ifdef COMPILE_DIAG
struct errno_t {
	constexpr errno_t(int x) : m_value(x) {
#ifdef COVERITY
		assert(x >= 0);
#else
		if (x < 0)
			throw std::logic_error("errno_t value must be >=0");
#endif
	}
	constexpr operator int() const { return m_value; }
	constexpr operator bool() const = delete;
	private:
	int m_value = 0;
};
#else
using errno_t = int;
#endif

}

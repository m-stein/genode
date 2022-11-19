/**
 * SmartCard-HSM PKCS#11 Module
 *
 * Copyright (c) 2013, CardContact Systems GmbH, Minden, Germany
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of CardContact Systems GmbH nor the
 *       names of its contributors may be used to endorse or promote products
 *       derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL CardContact Systems GmbH BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * @file testpkcs11.c
 * @author Andreas Schwier
 * @brief Unit test for PKCS#11 interface
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>

#include <sc-hsm/sc-hsm-pkcs11.h>

#include <common/mutex.h>
#include <common/asn1.h>

/* Number of threads used for multi-threading test */
#define NUM_THREADS		30

/* Default SO-PIN unless --so-pin is defined */
#define SOPIN "3537363231383830"


#ifndef _WIN32

#include <unistd.h>
#include <sys/wait.h>
#include <dlfcn.h>
#define LIB_HANDLE void*
#define P11LIBNAME "/usr/local/lib/libsc-hsm-pkcs11.so"

#else

#include <windows.h>
#include <malloc.h>
#define LIB_HANDLE HMODULE
#define P11LIBNAME "sc-hsm-pkcs11.dll"

#define dlopen(fn, flag) LoadLibrary(fn)
#define dlclose(h) FreeLibrary(h)
#define dlsym(h, n) GetProcAddress(h, n)
#define pthread_t HANDLE
#define pthread_create(t, a, f, p) (*t = CreateThread(0, 0, f, p, 0, 0), *t ? 0 : GetLastError())
#define pthread_join(t, s) WaitForSingleObject(t, INFINITE)
#define pthread_exit(r) ExitThread(0)
#define pthread_attr_t int
#define pthread_attr_init(a)
#define pthread_attr_setdetachstate(a, f)
#define pthread_attr_destroy(a)

char* dlerror()
{
	char* msg = "UNKNOWN";
	FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM, 0, GetLastError(), 0, (char*)&msg, 0, 0);
	return msg;
}

size_t getline(char** pp, size_t* pl, FILE* f)
{
	char buf[256];
	buf[0] = 0;
	fgets(buf, sizeof(buf), f);
	*pl = strlen(buf) + 1;
	if (*pp)
		free(*pp);
	*pp = (char*)malloc(*pl);
	if (*pp == 0) {
		printf("malloc(%zd) failed.", *pl);
		exit(1);
	}
	memcpy(*pp, buf, *pl);
	return *pl - 1;
}

void usleep(unsigned int usec)
{
    HANDLE timer;
    LARGE_INTEGER ft;

    ft.QuadPart = -(10 * (__int64)usec);

    timer = CreateWaitableTimer(NULL, TRUE, NULL);
    SetWaitableTimer(timer, &ft, 0, NULL, NULL, 0);
    WaitForSingleObject(timer, INFINITE);
    CloseHandle(timer);
}

#endif /* _WIN32 */



#include <pkcs11/cryptoki.h>
#include <sc-hsm/sc-hsm-pkcs11.h>

struct id2name_t {
	unsigned long       id;
	char                *name;
	unsigned long       attr;
};

struct id2name_t p11CKRName[] = {
		{ CKR_CANCEL                            , "CKR_CANCEL", 0 },
		{ CKR_HOST_MEMORY                       , "CKR_HOST_MEMORY", 0 },
		{ CKR_SLOT_ID_INVALID                   , "CKR_SLOT_ID_INVALID", 0 },
		{ CKR_GENERAL_ERROR                     , "CKR_GENERAL_ERROR", 0 },
		{ CKR_FUNCTION_FAILED                   , "CKR_FUNCTION_FAILED", 0 },
		{ CKR_ARGUMENTS_BAD                     , "CKR_ARGUMENTS_BAD", 0 },
		{ CKR_NO_EVENT                          , "CKR_NO_EVENT", 0 },
		{ CKR_NEED_TO_CREATE_THREADS            , "CKR_NEED_TO_CREATE_THREADS", 0 },
		{ CKR_CANT_LOCK                         , "CKR_CANT_LOCK", 0 },
		{ CKR_ATTRIBUTE_READ_ONLY               , "CKR_ATTRIBUTE_READ_ONLY", 0 },
		{ CKR_ATTRIBUTE_SENSITIVE               , "CKR_ATTRIBUTE_SENSITIVE", 0 },
		{ CKR_ATTRIBUTE_TYPE_INVALID            , "CKR_ATTRIBUTE_TYPE_INVALID", 0 },
		{ CKR_ATTRIBUTE_VALUE_INVALID           , "CKR_ATTRIBUTE_VALUE_INVALID", 0 },
		{ CKR_DATA_INVALID                      , "CKR_DATA_INVALID", 0 },
		{ CKR_DATA_LEN_RANGE                    , "CKR_DATA_LEN_RANGE", 0 },
		{ CKR_DEVICE_ERROR                      , "CKR_DEVICE_ERROR", 0 },
		{ CKR_DEVICE_MEMORY                     , "CKR_DEVICE_MEMORY", 0 },
		{ CKR_DEVICE_REMOVED                    , "CKR_DEVICE_REMOVED", 0 },
		{ CKR_ENCRYPTED_DATA_INVALID            , "CKR_ENCRYPTED_DATA_INVALID", 0 },
		{ CKR_ENCRYPTED_DATA_LEN_RANGE          , "CKR_ENCRYPTED_DATA_LEN_RANGE", 0 },
		{ CKR_FUNCTION_CANCELED                 , "CKR_FUNCTION_CANCELED", 0 },
		{ CKR_FUNCTION_NOT_PARALLEL             , "CKR_FUNCTION_NOT_PARALLEL", 0 },
		{ CKR_FUNCTION_NOT_SUPPORTED            , "CKR_FUNCTION_NOT_SUPPORTED", 0 },
		{ CKR_KEY_HANDLE_INVALID                , "CKR_KEY_HANDLE_INVALID", 0 },
		{ CKR_KEY_SIZE_RANGE                    , "CKR_KEY_SIZE_RANGE", 0 },
		{ CKR_KEY_TYPE_INCONSISTENT             , "CKR_KEY_TYPE_INCONSISTENT", 0 },
		{ CKR_KEY_NOT_NEEDED                    , "CKR_KEY_NOT_NEEDED", 0 },
		{ CKR_KEY_CHANGED                       , "CKR_KEY_CHANGED", 0 },
		{ CKR_KEY_NEEDED                        , "CKR_KEY_NEEDED", 0 },
		{ CKR_KEY_INDIGESTIBLE                  , "CKR_KEY_INDIGESTIBLE", 0 },
		{ CKR_KEY_FUNCTION_NOT_PERMITTED        , "CKR_KEY_FUNCTION_NOT_PERMITTED", 0 },
		{ CKR_KEY_NOT_WRAPPABLE                 , "CKR_KEY_NOT_WRAPPABLE", 0 },
		{ CKR_KEY_UNEXTRACTABLE                 , "CKR_KEY_UNEXTRACTABLE", 0 },
		{ CKR_MECHANISM_INVALID                 , "CKR_MECHANISM_INVALID", 0 },
		{ CKR_MECHANISM_PARAM_INVALID           , "CKR_MECHANISM_PARAM_INVALID", 0 },
		{ CKR_OBJECT_HANDLE_INVALID             , "CKR_OBJECT_HANDLE_INVALID", 0 },
		{ CKR_OPERATION_ACTIVE                  , "CKR_OPERATION_ACTIVE", 0 },
		{ CKR_OPERATION_NOT_INITIALIZED         , "CKR_OPERATION_NOT_INITIALIZED", 0 },
		{ CKR_PIN_INCORRECT                     , "CKR_PIN_INCORRECT", 0 },
		{ CKR_PIN_INVALID                       , "CKR_PIN_INVALID", 0 },
		{ CKR_PIN_LEN_RANGE                     , "CKR_PIN_LEN_RANGE", 0 },
		{ CKR_PIN_EXPIRED                       , "CKR_PIN_EXPIRED", 0 },
		{ CKR_PIN_LOCKED                        , "CKR_PIN_LOCKED", 0 },
		{ CKR_SESSION_CLOSED                    , "CKR_SESSION_CLOSED", 0 },
		{ CKR_SESSION_COUNT                     , "CKR_SESSION_COUNT", 0 },
		{ CKR_SESSION_HANDLE_INVALID            , "CKR_SESSION_HANDLE_INVALID", 0 },
		{ CKR_SESSION_PARALLEL_NOT_SUPPORTED    , "CKR_SESSION_PARALLEL_NOT_SUPPORTED", 0 },
		{ CKR_SESSION_READ_ONLY                 , "CKR_SESSION_READ_ONLY", 0 },
		{ CKR_SESSION_EXISTS                    , "CKR_SESSION_EXISTS", 0 },
		{ CKR_SESSION_READ_ONLY_EXISTS          , "CKR_SESSION_READ_ONLY_EXISTS", 0 },
		{ CKR_SESSION_READ_WRITE_SO_EXISTS      , "CKR_SESSION_READ_WRITE_SO_EXISTS", 0 },
		{ CKR_SIGNATURE_INVALID                 , "CKR_SIGNATURE_INVALID", 0 },
		{ CKR_SIGNATURE_LEN_RANGE               , "CKR_SIGNATURE_LEN_RANGE", 0 },
		{ CKR_TEMPLATE_INCOMPLETE               , "CKR_TEMPLATE_INCOMPLETE", 0 },
		{ CKR_TEMPLATE_INCONSISTENT             , "CKR_TEMPLATE_INCONSISTENT", 0 },
		{ CKR_TOKEN_NOT_PRESENT                 , "CKR_TOKEN_NOT_PRESENT", 0 },
		{ CKR_TOKEN_NOT_RECOGNIZED              , "CKR_TOKEN_NOT_RECOGNIZED", 0 },
		{ CKR_TOKEN_WRITE_PROTECTED             , "CKR_TOKEN_WRITE_PROTECTED", 0 },
		{ CKR_UNWRAPPING_KEY_HANDLE_INVALID     , "CKR_UNWRAPPING_KEY_HANDLE_INVALID", 0 },
		{ CKR_UNWRAPPING_KEY_SIZE_RANGE         , "CKR_UNWRAPPING_KEY_SIZE_RANGE", 0 },
		{ CKR_UNWRAPPING_KEY_TYPE_INCONSISTENT  , "CKR_UNWRAPPING_KEY_TYPE_INCONSISTENT", 0 },
		{ CKR_USER_ALREADY_LOGGED_IN            , "CKR_USER_ALREADY_LOGGED_IN", 0 },
		{ CKR_USER_NOT_LOGGED_IN                , "CKR_USER_NOT_LOGGED_IN", 0 },
		{ CKR_USER_PIN_NOT_INITIALIZED          , "CKR_USER_PIN_NOT_INITIALIZED", 0 },
		{ CKR_USER_TYPE_INVALID                 , "CKR_USER_TYPE_INVALID", 0 },
		{ CKR_USER_ANOTHER_ALREADY_LOGGED_IN    , "CKR_USER_ANOTHER_ALREADY_LOGGED_IN", 0 },
		{ CKR_USER_TOO_MANY_TYPES               , "CKR_USER_TOO_MANY_TYPES", 0 },
		{ CKR_WRAPPED_KEY_INVALID               , "CKR_WRAPPED_KEY_INVALID", 0 },
		{ CKR_WRAPPED_KEY_LEN_RANGE             , "CKR_WRAPPED_KEY_LEN_RANGE", 0 },
		{ CKR_WRAPPING_KEY_HANDLE_INVALID       , "CKR_WRAPPING_KEY_HANDLE_INVALID", 0 },
		{ CKR_WRAPPING_KEY_SIZE_RANGE           , "CKR_WRAPPING_KEY_SIZE_RANGE", 0 },
		{ CKR_WRAPPING_KEY_TYPE_INCONSISTENT    , "CKR_WRAPPING_KEY_TYPE_INCONSISTENT", 0 },
		{ CKR_RANDOM_SEED_NOT_SUPPORTED         , "CKR_RANDOM_SEED_NOT_SUPPORTED", 0 },
		{ CKR_RANDOM_NO_RNG                     , "CKR_RANDOM_NO_RNG", 0 },
		{ CKR_DOMAIN_PARAMS_INVALID             , "CKR_DOMAIN_PARAMS_INVALID", 0 },
		{ CKR_BUFFER_TOO_SMALL                  , "CKR_BUFFER_TOO_SMALL", 0 },
		{ CKR_SAVED_STATE_INVALID               , "CKR_SAVED_STATE_INVALID", 0 },
		{ CKR_INFORMATION_SENSITIVE             , "CKR_INFORMATION_SENSITIVE", 0 },
		{ CKR_STATE_UNSAVEABLE                  , "CKR_STATE_UNSAVEABLE", 0 },
		{ CKR_CRYPTOKI_NOT_INITIALIZED          , "CKR_CRYPTOKI_NOT_INITIALIZED", 0 },
		{ CKR_CRYPTOKI_ALREADY_INITIALIZED      , "CKR_CRYPTOKI_ALREADY_INITIALIZED", 0 },
		{ CKR_MUTEX_BAD                         , "CKR_MUTEX_BAD", 0 },
		{ CKR_MUTEX_NOT_LOCKED                  , "CKR_MUTEX_NOT_LOCKED", 0 },
		{ CKR_OK			                    , "CKR_OK", 0 },
		{ 0, NULL }
};


#define CKT_BBOOL       1
#define CKT_BIN         2
#define CKT_DATE        3
#define CKT_LONG        4
#define CKT_ULONG       5

#define P11CKA			71

struct id2name_t p11CKAName[P11CKA + 1] = {
		{ CKA_CLASS                              , "CKA_CLASS", CKT_LONG },
		{ CKA_TOKEN                              , "CKA_TOKEN", CKT_BBOOL },
		{ CKA_PRIVATE                            , "CKA_PRIVATE", CKT_BBOOL },
		{ CKA_LABEL                              , "CKA_LABEL", 0 },
		{ CKA_APPLICATION                        , "CKA_APPLICATION", 0 },
		{ CKA_VALUE                              , "CKA_VALUE", CKT_BIN },
		{ CKA_OBJECT_ID                          , "CKA_OBJECT_ID", 0 },
		{ CKA_CERTIFICATE_TYPE                   , "CKA_CERTIFICATE_TYPE", CKT_ULONG },
		{ CKA_CERTIFICATE_CATEGORY               , "CKA_CERTIFICATE_CATEGORY", CKT_ULONG },
		{ CKA_ISSUER                             , "CKA_ISSUER", 0 },
		{ CKA_SERIAL_NUMBER                      , "CKA_SERIAL_NUMBER", 0 },
		{ CKA_AC_ISSUER                          , "CKA_AC_ISSUER", 0 },
		{ CKA_OWNER                              , "CKA_OWNER", 0 },
		{ CKA_ATTR_TYPES                         , "CKA_ATTR_TYPES", 0 },
		{ CKA_TRUSTED                            , "CKA_TRUSTED", CKT_BBOOL },
		{ CKA_KEY_TYPE                           , "CKA_KEY_TYPE", 0 },
		{ CKA_SUBJECT                            , "CKA_SUBJECT", 0 },
		{ CKA_ID                                 , "CKA_ID", CKT_BIN },
		{ CKA_SENSITIVE                          , "CKA_SENSITIVE", CKT_BBOOL },
		{ CKA_ENCRYPT                            , "CKA_ENCRYPT", CKT_BBOOL },
		{ CKA_DECRYPT                            , "CKA_DECRYPT", CKT_BBOOL },
		{ CKA_WRAP                               , "CKA_WRAP", CKT_BBOOL },
		{ CKA_UNWRAP                             , "CKA_UNWRAP", CKT_BBOOL },
		{ CKA_SIGN                               , "CKA_SIGN", CKT_BBOOL },
		{ CKA_SIGN_RECOVER                       , "CKA_SIGN_RECOVER", CKT_BBOOL },
		{ CKA_VERIFY                             , "CKA_VERIFY", CKT_BBOOL },
		{ CKA_VERIFY_RECOVER                     , "CKA_VERIFY_RECOVER", CKT_BBOOL },
		{ CKA_DERIVE                             , "CKA_DERIVE", CKT_BBOOL },
		{ CKA_START_DATE                         , "CKA_START_DATE", CKT_DATE },
		{ CKA_END_DATE                           , "CKA_END_DATE", CKT_DATE },
		{ CKA_MODULUS                            , "CKA_MODULUS", 0 },
		{ CKA_MODULUS_BITS                       , "CKA_MODULUS_BITS", CKT_ULONG },
		{ CKA_PUBLIC_EXPONENT                    , "CKA_PUBLIC_EXPONENT", 0 },
		{ CKA_PRIVATE_EXPONENT                   , "CKA_PRIVATE_EXPONENT", 0 },
		{ CKA_PRIME_1                            , "CKA_PRIME_1", 0 },
		{ CKA_PRIME_2                            , "CKA_PRIME_2", 0 },
		{ CKA_EXPONENT_1                         , "CKA_EXPONENT_1", 0 },
		{ CKA_EXPONENT_2                         , "CKA_EXPONENT_2", 0 },
		{ CKA_COEFFICIENT                        , "CKA_COEFFICIENT", 0 },
		{ CKA_PRIME                              , "CKA_PRIME", 0 },
		{ CKA_SUBPRIME                           , "CKA_SUBPRIME", 0 },
		{ CKA_BASE                               , "CKA_BASE", 0 },
		{ CKA_PRIME_BITS                         , "CKA_PRIME_BITS", 0 },
		{ CKA_SUBPRIME_BITS                      , "CKA_SUBPRIME_BITS", 0 },
		{ CKA_VALUE_BITS                         , "CKA_VALUE_BITS", 0 },
		{ CKA_VALUE_LEN                          , "CKA_VALUE_LEN", CKT_LONG },
		{ CKA_EXTRACTABLE                        , "CKA_EXTRACTABLE", CKT_BBOOL },
		{ CKA_LOCAL                              , "CKA_LOCAL", CKT_BBOOL },
		{ CKA_NEVER_EXTRACTABLE                  , "CKA_NEVER_EXTRACTABLE", CKT_BBOOL },
		{ CKA_ALWAYS_SENSITIVE                   , "CKA_ALWAYS_SENSITIVE", CKT_BBOOL },
		{ CKA_KEY_GEN_MECHANISM                  , "CKA_KEY_GEN_MECHANISM", CKT_LONG },
		{ CKA_MODIFIABLE                         , "CKA_MODIFIABLE", CKT_BBOOL },
		{ CKA_EC_PARAMS                          , "CKA_EC_PARAMS", 0 },
		{ CKA_EC_POINT                           , "CKA_EC_POINT", 0 },
		{ CKA_SECONDARY_AUTH                     , "CKA_SECONDARY_AUTH", 0 },
		{ CKA_AUTH_PIN_FLAGS                     , "CKA_AUTH_PIN_FLAGS", 0 },
		{ CKA_HW_FEATURE_TYPE                    , "CKA_HW_FEATURE_TYPE", 0 },
		{ CKA_RESET_ON_INIT                      , "CKA_RESET_ON_INIT", 0 },
		{ CKA_HAS_RESET                          , "CKA_HAS_RESET", 0 },
		{ CKA_ALWAYS_AUTHENTICATE                , "CKA_ALWAYS_AUTHENTICATE", CKT_BBOOL },

		{ CKA_CVC_INNER_CAR                      , "CKA_CVC_INNER_CAR", CKT_BIN },
		{ CKA_CVC_OUTER_CAR                      , "CKA_CVC_OUTER_CAR", CKT_BIN },
		{ CKA_CVC_CHR                            , "CKA_CVC_CHR", CKT_BIN },
		{ CKA_CVC_CED                            , "CKA_CVC_CED", CKT_BIN },
		{ CKA_CVC_CXD                            , "CKA_CVC_CXD", CKT_BIN },
		{ CKA_CVC_CHAT                           , "CKA_CVC_CHAT", CKT_BIN },
		{ CKA_CVC_CURVE_OID                      , "CKA_CVC_CURVE_OID", CKT_BIN },
		{ CKA_SC_HSM_PUBLIC_KEY_ALGORITHM        , "CKA_SC_HSM_PUBLIC_KEY_ALGORITHM", CKT_BIN },
		{ CKA_SC_HSM_KEY_USE_COUNTER             , "CKA_SC_HSM_KEY_USE_COUNTER", CKT_BIN },
		{ CKA_SC_HSM_ALGORITHM_LIST              , "CKA_SC_HSM_ALGORITHM_LIST", CKT_BIN },
		{ CKA_CVC_REQUEST                        , "CKA_CVC_REQUEST", CKT_BIN },

		{ 0, NULL }
};

struct id2name_t p11CKKName[] = {
		{ CKK_RSA                                , "CKK_RSA", 0 },
		{ CKK_DSA                                , "CKK_DSA", 0 },
		{ CKK_DH                                 , "CKK_DH", 0 },
		{ CKK_EC                                 , "CKK_EC", 0 },
		{ CKK_X9_42_DH                           , "CKK_X9_42_DH", 0 },
		{ CKK_KEA                                , "CKK_KEA", 0 },
		{ CKK_GENERIC_SECRET                     , "CKK_GENERIC_SECRET", 0 },
		{ CKK_RC2                                , "CKK_RC2", 0 },
		{ CKK_RC4                                , "CKK_RC4", 0 },
		{ CKK_DES                                , "CKK_DES", 0 },
		{ CKK_DES2                               , "CKK_DES2", 0 },
		{ CKK_DES3                               , "CKK_DES3", 0 },
		{ CKK_CAST                               , "CKK_CAST", 0 },
		{ CKK_CAST3                              , "CKK_CAST3", 0 },
		{ CKK_CAST128                            , "CKK_CAST128", 0 },
		{ CKK_RC5                                , "CKK_RC5", 0 },
		{ CKK_IDEA                               , "CKK_IDEA", 0 },
		{ CKK_SKIPJACK                           , "CKK_SKIPJACK", 0 },
		{ CKK_BATON                              , "CKK_BATON", 0 },
		{ CKK_JUNIPER                            , "CKK_JUNIPER", 0 },
		{ CKK_CDMF                               , "CKK_CDMF", 0 },
		{ CKK_AES                                , "CKK_AES", 0 },
		{ 0, NULL }
};

/* Data structure for parameters passed to thread */
struct thread_data {
	int thread_id;
	CK_SLOT_ID slotid;
	CK_FUNCTION_LIST_PTR p11;
	int iterations;
};


char *p11libname = P11LIBNAME;

CK_UTF8CHAR *pin = NULL;
CK_UTF8CHAR wrongpin[] = "111111";
CK_ULONG pinlen = 6;

CK_UTF8CHAR *sopin = (CK_UTF8CHAR *)SOPIN;
CK_ULONG sopinlen = 16;

static MUTEX verdictMutex; /* initialized in main */
static int testscompleted = 0;
static int testsfailed = 0;

static int optTestInsertRemove = 0;
static int optTestPINBlock = 0;
static int optTestMultiOnly = 0;
static int optTestHotplug = 0;
static int optTestEvent = 0;
static int optTestFork = 0;
static int optTestInvasive = 0;
static int optOneThreadPerToken = 0;
static int optNoClass3Tests = 0;
static int optMultiThreadingTests = 0;
static int optThreadsPerToken = 1;
static int optIteration = 1;
static int optUnlockPIN = 0;
static int optFailFast = 0;
static long optSlotId = -1;
static char *optTokenFilter = "";

static char namebuf[40]; /* used by main thread */

static struct bytestring_s ecparam_prime256v1 = { (unsigned char *)"\x30\x81\xE0\x02\x01\x01\x30\x2C\x06\x07\x2A\x86\x48\xCE\x3D\x01\x01\x02\x21\x00\xFF\xFF\xFF\xFF\x00\x00\x00\x01\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\x30\x44\x04\x20\xFF\xFF\xFF\xFF\x00\x00\x00\x01\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFC\x04\x20\x5A\xC6\x35\xD8\xAA\x3A\x93\xE7\xB3\xEB\xBD\x55\x76\x98\x86\xBC\x65\x1D\x06\xB0\xCC\x53\xB0\xF6\x3B\xCE\x3C\x3E\x27\xD2\x60\x4B\x04\x41\x04\x6B\x17\xD1\xF2\xE1\x2C\x42\x47\xF8\xBC\xE6\xE5\x63\xA4\x40\xF2\x77\x03\x7D\x81\x2D\xEB\x33\xA0\xF4\xA1\x39\x45\xD8\x98\xC2\x96\x4F\xE3\x42\xE2\xFE\x1A\x7F\x9B\x8E\xE7\xEB\x4A\x7C\x0F\x9E\x16\x2B\xCE\x33\x57\x6B\x31\x5E\xCE\xCB\xB6\x40\x68\x37\xBF\x51\xF5\x02\x21\x00\xFF\xFF\xFF\xFF\x00\x00\x00\x00\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xBC\xE6\xFA\xAD\xA7\x17\x9E\x84\xF3\xB9\xCA\xC2\xFC\x63\x25\x51\x02\x01\x01", 227 };




static char *verdict(int condition) {
	mutex_lock(&verdictMutex);
	testscompleted++;

	if (condition) {
		mutex_unlock(&verdictMutex);
		return "Passed";
	} else {
		testsfailed++;
		mutex_unlock(&verdictMutex);
		if (optFailFast) {
			printf("Failed after %d tests.\n", testscompleted);
			exit(1);
		}
		return "Failed";
	}
}



static char *id2name(struct id2name_t *p, unsigned long id, unsigned long *attr, char scr[40]) {

	if (attr)
		*attr = 0;

	while (p->name && (p->id != id)) {
		p++;
	}

	if (p->name) {
		strcpy(scr, p->name);
		if (attr)
			*attr = p->attr;
	} else {
		sprintf(scr, "*** Undefined 0x%lx ***", id);
	}
	return scr;
}



static char *p11string(CK_UTF8CHAR *str, size_t len)
{
	static char buffer[81];
	int i;

	if (len > sizeof(buffer) - 1)
		return "**Input too long***";

	memcpy(buffer, str, len);
	buffer[len] = 0;

	i = (int)len;
	while (i > 0) {
		i--;
		if (buffer[i] == ' ') {
			buffer[i] = 0;
		} else {
			break;
		}
	}
	return buffer;
}



static void bin2str(char *st, int stlen, unsigned char *data, int datalen)
{
	int ascii, i;
	unsigned char *d;

	ascii = 1;
	d = data;
	i = datalen;

	while (i && (stlen > 2)) {
		sprintf(st, "%02X", *d);

		if (ascii && !isprint(*d) && *d)
			ascii = 0;

		st += 2;
		stlen -= 2;
		i--;
		d++;
	}

	if (ascii && (stlen > datalen + 3)) {
		*st++ = ' ';
		*st++ = '"';
		memcpy(st, data, datalen);
		st += datalen;
		*st++ = '"';
	}

	*st = '\0';
}



void dumpAttribute(CK_ATTRIBUTE_PTR attr)
{
	char attribute[30], scr[4096];
	unsigned long atype;

	strcpy(attribute, id2name(p11CKAName, attr->type, &atype, namebuf));

	switch(attr->type) {

	case CKA_KEY_TYPE:
		printf("  %s = %s\n", attribute, id2name(p11CKKName, *(CK_KEY_TYPE *)attr->pValue, NULL, namebuf));
		break;

	default:
		switch(atype) {
		case CKT_BBOOL:
			if (attr->pValue) {
				printf("  %s = %s [%d]\n", attribute, *(CK_BBOOL *)attr->pValue ? "TRUE" : "FALSE", *(CK_BBOOL *)attr->pValue);
			} else {
				printf("  %s\n", attribute);
			}
			break;
		case CKT_DATE:
			// pdate = (CK_DATE *)attr->pValue;
			// if (pdate != NULL) {
			//     sprintf(res, "  %s = %4s-%2s-%2s", attribute, pdate->year, pdate->month, pdate->day);
			// }
			printf("  %s\n", attribute);
			break;
		case CKT_LONG:
			printf("  %s = %d [0x%X]\n", attribute, (int)*(CK_LONG *)attr->pValue, (int)*(CK_LONG *)attr->pValue);
			break;
		case CKT_ULONG:
			printf("  %s = %u [0x%X]\n", attribute, (unsigned int)*(CK_ULONG *)attr->pValue, (unsigned int)*(CK_ULONG *)attr->pValue);
			break;
		case CKT_BIN:
		default:
			bin2str(scr, sizeof(scr), attr->pValue, attr->ulValueLen);
			printf("  %s = %s\n", attribute, scr);
			break;
		}
	}
}



void dumpObject(CK_FUNCTION_LIST_PTR p11, CK_SESSION_HANDLE session, CK_OBJECT_HANDLE hnd)
{
	CK_ATTRIBUTE template[P11CKA];
	int rc, i;

	memset(template, 0, sizeof(template));
	for (i = 0; i < P11CKA; i++) {
		template[i].type = p11CKAName[i].id;
	}
	printf("Calling C_GetAttributeValue ");
	rc = p11->C_GetAttributeValue(session, hnd, (CK_ATTRIBUTE_PTR)&template, P11CKA);
	printf("- %s : %s\n", id2name(p11CKRName, rc, 0, namebuf), (rc == CKR_OK) || (rc == CKR_ATTRIBUTE_TYPE_INVALID) ? "Passed" : "Failed");

	for (i = 0; i < P11CKA; i++) {
		if ((CK_LONG)template[i].ulValueLen > 0) {
			template[i].pValue = alloca(template[i].ulValueLen);
		}
	}

	printf("Calling C_GetAttributeValue ");
	rc = p11->C_GetAttributeValue(session, hnd, (CK_ATTRIBUTE_PTR)&template, P11CKA);
	printf("- %s : %s\n", id2name(p11CKRName, rc, 0, namebuf), (rc == CKR_OK) || (rc == CKR_ATTRIBUTE_TYPE_INVALID) ? "Passed" : "Failed");

	for (i = 0; i < P11CKA; i++) {
		if ((CK_LONG)template[i].ulValueLen > 0) {
			dumpAttribute(&template[i]);
		}
	}
}



void listObjects(CK_FUNCTION_LIST_PTR p11, CK_SESSION_HANDLE session, CK_ATTRIBUTE_PTR attr, int len)
{
	CK_OBJECT_HANDLE hnd;
	CK_ULONG cnt;
	int rc;

	printf("Calling C_FindObjectsInit ");
	rc = p11->C_FindObjectsInit(session, attr, len);
	printf("- %s : %s\n", id2name(p11CKRName, rc, 0, namebuf), verdict(rc == CKR_OK));

	if (rc != CKR_OK) {
		return;
	}

	cnt = 1;
	while ((rc == CKR_OK) && (cnt)) {
		printf("Calling C_FindObjects ");
		rc = p11->C_FindObjects(session, &hnd, 1, &cnt);
		printf("- %s : %s\n", id2name(p11CKRName, rc, 0, namebuf), verdict(rc == CKR_OK));

		if ((rc == CKR_OK) && (cnt == 1)) {
			dumpObject(p11, session, hnd);
		}
	}

	printf("Calling C_FindObjectsFinal ");
	p11->C_FindObjectsFinal(session);
	printf("- %s : %s\n", id2name(p11CKRName, rc, 0, namebuf), verdict(rc == CKR_OK));
}



int findObject(CK_FUNCTION_LIST_PTR p11, CK_SESSION_HANDLE session, CK_ATTRIBUTE_PTR attr, int len, int ofs, CK_OBJECT_HANDLE_PTR phnd)
{
	CK_ULONG cnt;
	CK_OBJECT_HANDLE hnd;
	int rc;

	printf("Calling C_FindObjectsInit ");
	rc = p11->C_FindObjectsInit(session, attr, len);
	printf("- %s : %s\n", id2name(p11CKRName, rc, 0, namebuf), verdict(rc == CKR_OK));

	if (rc != CKR_OK) {
		return rc;
	}

	do	{
		cnt = 1;
		printf("Calling C_FindObjects ");
		rc = p11->C_FindObjects(session, &hnd, 1, &cnt);
		printf("- %s : %s\n", id2name(p11CKRName, rc, 0, namebuf), verdict(rc == CKR_OK));
	} while ((rc == CKR_OK) && ofs--);

	printf("Calling C_FindObjectsFinal ");
	p11->C_FindObjectsFinal(session);
	printf("- %s : %s\n", id2name(p11CKRName, rc, 0, namebuf), verdict(rc == CKR_OK));

	if (cnt == 0) {
		return CKR_ARGUMENTS_BAD;
	}

	*phnd = hnd;
	return CKR_OK;
}



void testSymmetricKeyDerivation(CK_FUNCTION_LIST_PTR p11, CK_SESSION_HANDLE session)
{
	int rc;
	CK_CHAR labelBase[] = "TestBaseAESKey", labelDerived[] = "TestDerivedKey";
	CK_BBOOL _false = FALSE;
	CK_BBOOL _true = TRUE;

	CK_OBJECT_CLASS secretKeyClass = CKO_SECRET_KEY;
	CK_ULONG len = 16;
	CK_BYTE algoList[] = {0x10, 0x11, 0x18, 0x99};
	CK_ATTRIBUTE secretKeyTemplate[20] = {
			{ CKA_CLASS, &secretKeyClass, sizeof(secretKeyClass) },
			{ CKA_TOKEN, &_true, sizeof(_true)},
			{ CKA_PRIVATE, &_true, sizeof(_true)},
			{ CKA_SENSITIVE, &_true, sizeof(_true)},
			{ CKA_LABEL, &labelBase, (CK_ULONG)strlen((char *)labelBase) },
			{ CKA_VALUE_LEN, &len, sizeof(len) },
			{ CKA_SC_HSM_ALGORITHM_LIST, &algoList, sizeof(algoList)},
			{ CKA_SIGN, &_true, sizeof(_true)},
			{ CKA_DERIVE, &_true, sizeof(_true)},
			{ CKA_ENCRYPT, &_true, sizeof(_true)},
			{ CKA_DECRYPT, &_true, sizeof(_true)}
	};
	int secretKeyAttributes = 11;

	CK_OBJECT_HANDLE hndBaseKey, hndDerivedKey;
	CK_MECHANISM mech_genaes = { CKM_AES_KEY_GEN, 0, 0 };

	CK_KEY_TYPE keyType = CKK_GENERIC_SECRET;
	CK_ATTRIBUTE deriveTemplate[5] = {
			{ CKA_CLASS, &secretKeyClass, sizeof(secretKeyClass) },
			{ CKA_KEY_TYPE, &keyType, sizeof(keyType)},
			{ CKA_SIGN, &_true, sizeof(_true)},
			{ CKA_LABEL, &labelDerived, (CK_ULONG)strlen((char *)labelDerived) }
	};
	int derivedAttributes = 4;

	CK_BYTE keyValue[32];
	CK_ATTRIBUTE keyValueTemplate[1] = {
			{ CKA_VALUE, &keyValue, 32 }
	};
	unsigned char param[32] = {
			0xCA, 0xFE, 0xBA, 0xBE, 0xCA, 0xFE, 0xBA, 0xBE, 0xCA, 0xFE, 0xBA, 0xBE, 0xCA, 0xFE, 0xBA, 0xBE,
			0xCA, 0xFE, 0xBA, 0xBE, 0xCA, 0xFE, 0xBA, 0xBE, 0xCA, 0xFE, 0xBA, 0xBE, 0xCA, 0xFE, 0xBA, 0xBE
	};
	CK_MECHANISM mech_derive = { CKM_SC_HSM_SP80056C_DERIVE, &param, sizeof(param) };

	printf("Calling C_GenerateKey(AES 128) ");
	rc = p11->C_GenerateKey(session, &mech_genaes, secretKeyTemplate, secretKeyAttributes, &hndBaseKey);
	printf("- %s : %s\n", id2name(p11CKRName, rc, 0, namebuf), verdict(rc == CKR_OK));

	rc = p11->C_DeriveKey(session, &mech_derive, hndBaseKey, deriveTemplate, derivedAttributes, &hndDerivedKey);
	printf("- %s : %s\n", id2name(p11CKRName, rc, 0, namebuf), verdict(rc == CKR_OK));

	if (rc == CKR_OK) {
		printf("Derived Secret Key:\n");
		dumpObject(p11, session, hndDerivedKey);
	}

	hndDerivedKey = CK_INVALID_HANDLE;
	rc = findObject(p11, session, (CK_ATTRIBUTE_PTR)&deriveTemplate, derivedAttributes, 0, &hndDerivedKey);

	printf("Calling C_GetAttributeValue ");
	rc = p11->C_GetAttributeValue(session, hndDerivedKey, (CK_ATTRIBUTE_PTR)&keyValueTemplate, 1);
	printf("- %s : %s\n", id2name(p11CKRName, rc, 0, namebuf), verdict(rc == CKR_OK));

	printf("Calling C_DestroyObject(DerivedKey) ");
	rc = p11->C_DestroyObject(session, hndDerivedKey);
	printf("- %s : %s\n", id2name(p11CKRName, rc, 0, namebuf), verdict(rc == CKR_OK));

	printf("Calling C_DestroyObject(BaseKey) ");
	rc = p11->C_DestroyObject(session, hndBaseKey);
	printf("- %s : %s\n", id2name(p11CKRName, rc, 0, namebuf), verdict(rc == CKR_OK));
}



void testLogin(CK_FUNCTION_LIST_PTR p11, CK_SESSION_HANDLE session)
{
	int rc;
	CK_SESSION_INFO sessioninfo;
	CK_TOKEN_INFO tokeninfo;
	CK_OBJECT_HANDLE hnd;
	CK_MECHANISM mech = { CKM_SHA1_RSA_PKCS, 0, 0 };
	CK_OBJECT_CLASS class = CKO_PRIVATE_KEY;
//	CK_KEY_TYPE keyType = CKK_RSA;
	CK_ATTRIBUTE template[] = {
			{ CKA_CLASS, &class, sizeof(class) }
//			{ CKA_KEY_TYPE, &keyType, sizeof(keyType) }
	};

/*
	printf("Calling C_GetSessionInfo ");
	rc = p11->C_GetSessionInfo(session, &sessioninfo);
	printf("- %s : %s\n", id2name(p11CKRName, rc, 0, namebuf), verdict(rc == CKR_OK));
	printf("Session state %lu - %s\n", sessioninfo.state, verdict(sessioninfo.state == CKS_RW_PUBLIC_SESSION));
*/

	printf("Calling C_Login User ");
	rc = p11->C_Login(session, CKU_USER, pin, pinlen);
	printf("- %s : %s\n", id2name(p11CKRName, rc, 0, namebuf), verdict(rc == CKR_OK));

	if (rc != CKR_OK) {
		exit(1);
	}
}



void unlockPIN(CK_FUNCTION_LIST_PTR p11, CK_SLOT_ID slotid)
{
	CK_RV rc;
	CK_SESSION_HANDLE session;

	rc = p11->C_OpenSession(slotid, CKF_RW_SESSION | CKF_SERIAL_SESSION, NULL, NULL, &session);
	printf("Calling C_OpenSession - %s : %s\n", id2name(p11CKRName, rc, 0, namebuf), verdict(rc == CKR_OK));

	rc = p11->C_Login(session, CKU_SO, sopin, sopinlen);
	printf("Calling C_Login(SO) - %s : %s\n", id2name(p11CKRName, rc, 0, namebuf), verdict(rc == CKR_OK));

	rc = p11->C_InitPIN(session, NULL, 0);
	printf("Calling C_InitPIN() - %s : %s\n", id2name(p11CKRName, rc, 0, namebuf), verdict(rc == CKR_OK));

	rc = p11->C_CloseSession(session);
	printf("- %s : %s\n", id2name(p11CKRName, rc, 0, namebuf), verdict(rc == CKR_OK));
}



void usage()
{
	printf("sc-hsm-tool [--module <p11-file>] [--pin <user-pin>] [--token <tokenname>] [--threads <count>] [--iterations <count>]\n");
	printf("  --test-insert-remove       Enable insert / remove test\n");
	printf("  --test-pin-block           Enable PIN blocking test\n");
	printf("  --test-multithreading-only Perform multi-threading tests only\n");
	printf("  --test-hotplug-only        Perform hot-plug tests only\n");
	printf("  --test-fork                Test behavior during fork()\n");
	printf("  --one-thread-per-token     Create a single thread per token rather than distributing %d\n", NUM_THREADS);
	printf("  --no-class3-tests          No PIN tests with attached class 3 PIN PAD\n");
	printf("  --multithreading-tests     Perform multi-threading tests\n");
	printf("  --fail-fast                Abort at first failed test\n");
	printf("  --invasive                 Enable tests that change keys on the device\n");
	printf("  --unlock-pin               Unlock PIN without setting a new value\n");
}



void decodeArgs(int argc, char **argv)
{
	argv++;
	argc--;

	while (argc--) {
		if (!strcmp(*argv, "--pin")) {
			if (argc < 0) {
				printf("Argument for --pin missing\n");
				exit(1);
			}
			argv++;
			pin = (CK_UTF8CHAR_PTR)*argv;
			pinlen = (CK_ULONG)strlen((char *)pin);
			argc--;
		} else if (!strcmp(*argv, "--so-pin")) {
			if (argc < 0) {
				printf("Argument for --so-pin missing\n");
				exit(1);
			}
			argv++;
			sopin = (CK_UTF8CHAR_PTR)*argv;
			sopinlen = (CK_ULONG)strlen((char *)sopin);
			argc--;
		} else if (!strcmp(*argv, "--module")) {
			if (argc < 0) {
				printf("Argument for --module missing\n");
				exit(1);
			}
			argv++;
			p11libname = *argv;
			argc--;
		} else if (!strcmp(*argv, "--token")) {
			if (argc < 0) {
				printf("Argument for --token missing\n");
				exit(1);
			}
			argv++;
			optTokenFilter = *argv;
			argc--;
		} else if (!strcmp(*argv, "--slotid")) {
			if (argc < 0) {
				printf("Argument for --slotid missing\n");
				exit(1);
			}
			argv++;
			optSlotId = atol(*argv);
			argc--;
		} else if (!strcmp(*argv, "--threads")) {
			if (argc < 0) {
				printf("Argument for --threads missing\n");
				exit(1);
			}
			argv++;
			optThreadsPerToken = atol(*argv);
			argc--;
		} else if (!strcmp(*argv, "--iterations")) {
			if (argc < 0) {
				printf("Argument for --iterations missing\n");
				exit(1);
			}
			argv++;
			optIteration = atol(*argv);
			argc--;
		} else if (!strcmp(*argv, "--test-insert-remove")) {
			optTestInsertRemove = 1;
		} else if (!strcmp(*argv, "--test-pin-block")) {
			optTestPINBlock = 1;
		} else if (!strcmp(*argv, "--test-multithreading-only")) {
			optTestMultiOnly = 1;
		} else if (!strcmp(*argv, "--test-hotplug-only")) {
			optTestHotplug = 1;
		} else if (!strcmp(*argv, "--test-event-only")) {
			optTestEvent = 1;
		} else if (!strcmp(*argv, "--test-fork")) {
			optTestFork = 1;
		} else if (!strcmp(*argv, "--one-thread-per-token")) {
			optOneThreadPerToken = 1;
		} else if (!strcmp(*argv, "--no-class3-tests")) {
			optNoClass3Tests = 1;
		} else if (!strcmp(*argv, "--multithreading-tests")) {
			optMultiThreadingTests = 1;
		} else if (!strcmp(*argv, "--fail-fast")) {
			optFailFast = 1;
		} else if (!strcmp(*argv, "--unlock-pin")) {
			optUnlockPIN = 1;
		} else if (!strcmp(*argv, "--invasive")) {
			optTestInvasive = 1;
		} else {
			printf("Unknown argument %s\n", *argv);
			usage();
			exit(1);
		}
		argv++;
	}
}


int generate_psk()
{
	int i;
	CK_RV rc;
	CK_ULONG slots;
	CK_SESSION_HANDLE session;
	CK_INFO info;
	CK_SLOT_ID_PTR slotlist = NULL;
	CK_SLOT_ID slotid;
	CK_SLOT_INFO slotinfo;
	CK_TOKEN_INFO tokeninfo;
	CK_ATTRIBUTE attr[6];
	CK_FUNCTION_LIST_PTR p11;
	LIB_HANDLE dlhandle;
	CK_RV (*C_GetFunctionList)(CK_FUNCTION_LIST_PTR_PTR);
	CK_C_INITIALIZE_ARGS initArgs;

	char *argv[] = {
		"sc-hsm-pkcs11-test",
		"--module",
		"sc_hsm_pkcs11.lib.so",
		"--pin",
		"123456",
		"--invasive"
	};
	int argc = sizeof(argv) / sizeof(argv[0]);

	decodeArgs(argc, argv);

	printf("PKCS11 unit test running.\n");

	dlhandle = dlopen(p11libname, RTLD_NOW);

	if (!dlhandle) {
		printf("dlopen failed with %s\n", dlerror());
		exit(1);
	}

	C_GetFunctionList = (CK_RV (*)(CK_FUNCTION_LIST_PTR_PTR))dlsym(dlhandle, "C_GetFunctionList");

	printf("Calling C_GetFunctionList ");

	(*C_GetFunctionList)(&p11);

	memset(&initArgs, 0, sizeof(initArgs));
	initArgs.flags = CKF_OS_LOCKING_OK;

	printf("Calling C_Initialize ");

	rc = p11->C_Initialize(&initArgs);
	printf("- %s : %s\n", id2name(p11CKRName, rc, 0, namebuf), verdict(rc == CKR_OK));

	if (rc != CKR_OK) {
		exit(1);
	}

	if (optTestHotplug) {
	} else if (optTestEvent) {
	} else {
		printf("Calling C_GetSlotList ");

		rc = p11->C_GetSlotList(TRUE, NULL, &slots);

		if (rc != CKR_OK) {
			printf("- %s : %s\n", id2name(p11CKRName, rc, 0, namebuf), verdict(rc == CKR_OK));
			exit(1);
		}

		slotlist = (CK_SLOT_ID_PTR) malloc(sizeof(CK_SLOT_ID) * slots);

		rc = p11->C_GetSlotList(FALSE, slotlist, &slots);
		printf("- %s : %s\n", id2name(p11CKRName, rc, 0, namebuf), verdict(rc == CKR_OK));

		if (rc != CKR_OK) {
			exit(1);
		}

		if (optTestFork) {
		}

		i = 0;

		while (i < (int)slots) {
			slotid = *(slotlist + i);
			i++;

			if ((optSlotId != -1) && (optSlotId != slotid))
				continue;

			if (optTestInsertRemove) {
				continue;
			}

			printf("Calling C_GetSlotInfo for slot %lu ", slotid);

			rc = p11->C_GetSlotInfo(slotid, &slotinfo);
			printf("- %s : %s\n", id2name(p11CKRName, rc, 0, namebuf), verdict(rc == CKR_OK));

			if (rc != CKR_OK) {
				printf("Error getting slot information from cryptoki. slotid = %lu, rc = %lu = %s\n", slotid, rc, id2name(p11CKRName, rc, NULL, namebuf));
				free(slotlist);
				exit(1);
			}

			printf("Slot manufacturer: %s\n", p11string(slotinfo.manufacturerID, sizeof(slotinfo.manufacturerID)));
			printf("Slot ID : Slot description: %ld : %s\n", slotid, p11string(slotinfo.slotDescription, sizeof(slotinfo.slotDescription)));
			printf("Slot flags: %x\n", (int)slotinfo.flags);

			printf("Calling C_GetTokenInfo ");

			rc = p11->C_GetTokenInfo(slotid, &tokeninfo);
			printf("- %s : %s\n", id2name(p11CKRName, rc, 0, namebuf), rc == CKR_OK ? "Passed" : rc == CKR_TOKEN_NOT_PRESENT ? "No token" : "Failed");

			if (rc != CKR_OK && rc != CKR_TOKEN_NOT_PRESENT) {
				printf("Error getting token information from cryptoki. slotid = %lu, rc = %lu = %s\n", slotid, rc, id2name(p11CKRName, rc, NULL, namebuf));
				free(slotlist);
				exit(1);
			}

			if (rc == CKR_OK) {
				printf("Token label       : %s\n", p11string(tokeninfo.label, sizeof(tokeninfo.label)));
				printf("Token manufacturer: %s\n", p11string(tokeninfo.manufacturerID, sizeof(tokeninfo.manufacturerID)));
				printf("Token model       : %s\n", p11string(tokeninfo.model, sizeof(tokeninfo.model)));
				printf("Token flags       : %lx\n", tokeninfo.flags);

				if (pin == NULL) {
					printf("Skipping tests that require a PIN. PIN can be set with --pin\n");
					continue;
				}

				if (optTestMultiOnly)
					continue;

				if (*optTokenFilter && strncmp(optTokenFilter, (const char *)tokeninfo.label, strlen(optTokenFilter)))
					continue;


				if (!strncmp("SmartCard-HSM", (char *)tokeninfo.label, 13)) {
					if (tokeninfo.flags & CKF_USER_PIN_TO_BE_CHANGED) {
					}
				}

				if (optUnlockPIN) {
					unlockPIN(p11, slotid);
					break;
				}

				rc = p11->C_OpenSession(slotid, CKF_RW_SESSION | CKF_SERIAL_SESSION, NULL, NULL, &session);
				printf("- %s : %s\n", id2name(p11CKRName, rc, 0, namebuf), verdict(rc == CKR_OK));

				if (rc != CKR_OK) {
					exit(1);
				}

				// List public objects
				memset(attr, 0, sizeof(attr));
				listObjects(p11, session, attr, 0);

#ifdef ENABLE_LIBCRYPTO
#endif

				testLogin(p11, session);

				// List all objects
				memset(attr, 0, sizeof(attr));
				//listObjects(p11, session, attr, 0);

				if (optTestInvasive && !strncmp("SmartCard-HSM", (char *)tokeninfo.label, 13)) {

					testSymmetricKeyDerivation(p11, session);
				}

				printf("Calling C_CloseSession ");
				rc = p11->C_CloseSession(session);
				printf("- %s : %s\n", id2name(p11CKRName, rc, 0, namebuf), verdict(rc == CKR_OK));
			}
		}
		free(slotlist);

#ifndef _WIN32
		if (optMultiThreadingTests) { }
#endif
	}

	printf("Calling C_Finalize ");

	rc = p11->C_Finalize(NULL);
	printf("- %s : %s\n", id2name(p11CKRName, rc, 0, namebuf), verdict(rc == CKR_OK));

	if (rc != CKR_OK) {
		exit(1);
	}

	dlclose(dlhandle);

	printf("Unit test finished.\n");
	printf("%d tests performed.\n", testscompleted);
	printf("%d tests failed.\n", testsfailed);

	if (testsfailed) {
		return -1;
	}
	return 0;
}


int main(int argc, char *argv[])
{
	printf("Error: main called\n");
	return -1;
}

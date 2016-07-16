/*
  +----------------------------------------------------------------------+
  | PHP Version 7                                                        |
  +----------------------------------------------------------------------+
  | Copyright (c) 1997-2016 The PHP Group                                |
  +----------------------------------------------------------------------+
  | This source file is subject to version 3.01 of the PHP license,      |
  | that is bundled with this package in the file LICENSE, and is        |
  | available through the world-wide-web at the following url:           |
  | http://www.php.net/license/3_01.txt                                  |
  | If you did not receive a copy of the PHP license and are unable to   |
  | obtain it through the world-wide-web, please send a note to          |
  | license@php.net so we can mail you a copy immediately.               |
  +----------------------------------------------------------------------+
  | Author:                                                              |
  +----------------------------------------------------------------------+
*/

/* $Id$ */

#ifndef PHP_AKM_H
#define PHP_AKM_H

extern zend_module_entry akm_module_entry;
#define phpext_akm_ptr &akm_module_entry

#define PHP_AKM_VERSION "0.1.0" /* Replace with version number for your extension */

#ifdef PHP_WIN32
#	define PHP_AKM_API __declspec(dllexport)
#elif defined(__GNUC__) && __GNUC__ >= 4
#	define PHP_AKM_API __attribute__ ((visibility("default")))
#else
#	define PHP_AKM_API
#endif

#ifdef ZTS
#include "TSRM.h"
#endif

#include "ahocorasick/ahocorasick.h"

/*
  	Declare any global variables you may need between the BEGIN
	and END macros here:

ZEND_BEGIN_MODULE_GLOBALS(akm)
	zend_long  global_value;
	char *global_string;
ZEND_END_MODULE_GLOBALS(akm)
*/

typedef AC_TRIE_t      akm_trie_t;
typedef AC_PATTERN_t   akm_pattern_t;
typedef AC_MATCH_t     akm_match_t;
typedef AC_TEXT_t      akm_text_t;

#define AKM_PATTID_TYPE_STRING AC_PATTID_TYPE_STRING

#define  akm_trie_add        ac_trie_add
#define  akm_trie_create     ac_trie_create
#define  akm_trie_finalize   ac_trie_finalize
#define  akm_trie_release    ac_trie_release
#define  akm_trie_settext    ac_trie_settext
#define  akm_trie_findnext   ac_trie_findnext

PHP_FUNCTION(akm_match);

/* Always refer to the globals in your function as AKM_G(variable).
   You are encouraged to rename these macros something shorter, see
   examples in any other php module directory.
*/
#define AKM_G(v) ZEND_MODULE_GLOBALS_ACCESSOR(akm, v)

#if defined(ZTS) && defined(COMPILE_DL_AKM)
ZEND_TSRMLS_CACHE_EXTERN();
#endif

#endif	/* PHP_AKM_H */


/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */

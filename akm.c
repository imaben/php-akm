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
   | Author: maben <www.maben@foxmail.com>                                |
   +----------------------------------------------------------------------+
   */

/* $Id$ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "php.h"
#include "php_ini.h"
#include "ext/standard/info.h"
#include "php_akm.h"

#include <dirent.h>
#include <sys/stat.h>

/* If you declare any globals in php_akm.h uncomment this:
   ZEND_DECLARE_MODULE_GLOBALS(akm)
   */

/* True global resources - no need for thread safety here */
static int le_akm;

static int   akm_enable = 0;
static char *akm_dict_dir = NULL;

static HashTable *akm_dict_ht = NULL;

#define DELIMITER '|'

/**
 * {{{ akm_dict_ht
 */

static inline void akm_build_node(akm_trie_t *trie, char *keyword,
		size_t keyword_len, char *extension)
{
	akm_pattern_t patt;

	/* Fill the pattern data */
	patt.ptext.astring = keyword;
	patt.ptext.length  = keyword_len;

	/* The replacement pattern is not applicable in this program, so better
	 * to initialize it with 0 */
	patt.rtext.astring = NULL;
	patt.rtext.length  = 0;

	patt.id.u.stringy = extension;
	patt.id.type      = AKM_PATTID_TYPE_STRING;

	/* Add pattern to automata */
	akm_trie_add (trie, &patt, 1);
}

static void akm_build_tree(char *filename, char *fullpath)
{
	php_stream *stream;
	char       *line;
	size_t      line_len = 0, i;

	char       *keyword,
			   *extension;
	size_t      keyword_len;

	struct stat st;

	stat(fullpath, &st);
	if (st.st_size == 0) {
		return;
	}

	/* add to HashTable */
	akm_trie_t *trie = akm_trie_create ();
	zval ztrie;
	ZVAL_PTR(&ztrie, trie);

	zend_hash_add(akm_dict_ht,
			zend_string_init(filename, strlen(filename), 1),
			&ztrie);

	stream = php_stream_open_wrapper(fullpath, "r", REPORT_ERRORS, NULL);
	if (!stream) {
		return;
	}

	while (NULL != (line = php_stream_get_line(stream, NULL, 0, &line_len))) {
		/* remove \r\n */
		if (line[line_len - 1] == '\n') {
			line[line_len - 1] = '\0';
		}

		if (line[line_len - 2] == '\r') {
			line[line_len - 2] = '\0';
		}
		line_len = strlen(line);

		if (line_len == 0 || line[0] == DELIMITER) {
			continue;
		}

		/* find delimiter */
		keyword     = line;
		keyword_len = 0;
		extension   = NULL;
		for (i = 0; i < line_len; i++) {
			if (line[i] == DELIMITER) {
				keyword_len = i;
				break;
			}
		}

		if (keyword_len == 0) { /* not found */
			keyword_len = line_len;
		} else {
			if (keyword_len + 1 == line_len) { /* example: "keyword|" */
				keyword_len = line_len - 1;
			} else {
				extension = keyword + keyword_len + 1;
			}
		}

		akm_build_node(trie, keyword, keyword_len, extension);
		efree(line);
	}

	akm_trie_finalize (trie);
}

static int akm_scan_directory(char *dirname,
		void (*callback)(char *filename, char *fullpath))
{
	int success = 0;
	char fullpath[PATH_MAX] = { 0 };
	struct dirent *ent = NULL;

	DIR *dir = opendir(dirname);
	if (dir == NULL) {
		return -1;
	}

	while (NULL != (ent = readdir(dir))) {
		if (ent->d_type == DT_REG) {
			sprintf(fullpath, "%s%s", dirname, ent->d_name);
			akm_build_tree(ent->d_name, fullpath);
			success++;
		}
	}

	return success;
}

static int akm_dict_ht_init()
{
	ALLOC_HASHTABLE(akm_dict_ht);
	if (akm_dict_ht == NULL) {
		return -1;
	}

	zend_hash_init(akm_dict_ht, 0, NULL, ZVAL_PTR_DTOR, 0);

	if (access(akm_dict_dir, R_OK) < 0) {
		return -1;
	}

	if (akm_scan_directory(akm_dict_dir, akm_build_tree) < 1) {
		return -1;
	}
	return 0;
}

static void akm_dict_ht_free()
{
	if (akm_dict_ht) {
		zend_string *key;
		zval *value;
		zend_ulong idx;
		akm_trie_t *trie;

		ZEND_HASH_FOREACH_KEY_VAL(akm_dict_ht, idx, key, value) {

			zend_string_free(key);
			trie = Z_PTR_P(value);
			akm_trie_release (trie);

		} ZEND_HASH_FOREACH_END();

		FREE_HASHTABLE(akm_dict_ht);
	}
}

static akm_trie_t *akm_get_trie(char *key, size_t len)
{
	zval *trie = zend_hash_str_find(akm_dict_ht, key, len);
	if (trie) {
		return Z_PTR_P(trie);
	}
	return NULL;
}

/* }}} */

/* {{{ PHP_INI
*/

ZEND_INI_MH(php_akm_enable)
{
	if (!new_value || new_value->len == 0) {
		return FAILURE;
	}

	if (!strcasecmp(new_value->val, "on") || !strcmp(new_value->val, "1")) {
		akm_enable = 1;
	} else {
		akm_enable = 0;
	}

	return SUCCESS;
}

ZEND_INI_MH(php_akm_dict_dir)
{
	if (!new_value || new_value->len == 0) {
		return FAILURE;
	}
	if (new_value->val[new_value->len] != '/') {
		akm_dict_dir = pemalloc(new_value->len + 2, 1);
		strcpy(akm_dict_dir, new_value->val);
		akm_dict_dir[new_value->len] = '/';
		akm_dict_dir[new_value->len + 1] = '\0';
	} else {
		akm_dict_dir = strdup(new_value->val);
	}
	if (akm_dict_dir == NULL) {
		return FAILURE;
	}
	return SUCCESS;
}

PHP_INI_BEGIN()
	PHP_INI_ENTRY("akm.enable", "0", PHP_INI_ALL, php_akm_enable)
	PHP_INI_ENTRY("akm.dict_dir", "", PHP_INI_ALL, php_akm_dict_dir)
PHP_INI_END()
	/* }}} */

	/* {{{ php function */

static void akm_fill_array(akm_match_t *m, zend_ulong *idx, zval *return_value)
{
	unsigned int j;
	akm_pattern_t *pp;
	zval entry,
		 keyword,
		 offset,
		 extension;
	array_init_size(&entry, 3);

	for (j = 0; j < m->size; j++)
	{
		pp = &m->patterns[j];

		ZVAL_NEW_STR(&keyword, zend_string_init(pp->ptext.astring, pp->ptext.length, 0));
		ZVAL_LONG(&offset, m->position);
		if (pp->id.u.stringy == NULL) {
			ZVAL_NULL(&extension);
		} else {
			ZVAL_NEW_STR(&extension, zend_string_init(pp->id.u.stringy, strlen(pp->id.u.stringy), 0));
		}
		zend_hash_add_new(Z_ARRVAL_P(&entry), zend_string_init("keyword", strlen("keyword"), 0), &keyword);
		zend_hash_add_new(Z_ARRVAL_P(&entry), zend_string_init("offset", strlen("offset"), 0), &offset);
		zend_hash_add_new(Z_ARRVAL_P(&entry), zend_string_init("extension", strlen("extension"), 0), &extension);
		zend_hash_index_add_new(Z_ARRVAL_P(return_value), *idx, &entry);

		(*idx)++;
	}
}

PHP_FUNCTION(akm_match)
{
	char *dict_name,
		 *text;

	size_t dict_name_len,
		   text_len;

	akm_text_t  chunk;
	akm_match_t match;

	zend_ulong idx = 0;

	if (zend_parse_parameters(ZEND_NUM_ARGS(), "ss", &dict_name, &dict_name_len, &text, &text_len) == FAILURE) {
		return;
	}

	akm_trie_t *trie = akm_get_trie(dict_name, dict_name_len);
	if (trie == NULL) {
		php_error_docref(NULL, E_WARNING, "Dict name #%.*s is not found", dict_name_len, dict_name);
		RETURN_FALSE;
	}

	array_init(return_value);

	chunk.astring = text;
	chunk.length  = text_len;

	akm_trie_settext (trie, &chunk, 0);

	while ((match = akm_trie_findnext(trie)).size) {
		akm_fill_array(&match, &idx, return_value);
	}
}

/* }}} */


/* {{{ php_akm_init_globals
*/
/* Uncomment this function if you have INI entries
   static void php_akm_init_globals(zend_akm_globals *akm_globals)
   {
   akm_globals->global_value = 0;
   akm_globals->global_string = NULL;
   }
   */
/* }}} */

/* {{{ PHP_MINIT_FUNCTION
*/
PHP_MINIT_FUNCTION(akm)
{
	REGISTER_INI_ENTRIES();

	if (!akm_enable) {
		return SUCCESS;
	}

	if (akm_dict_ht_init() < 0) {
		return FAILURE;
	}

	return SUCCESS;
}
/* }}} */

/* {{{ PHP_MSHUTDOWN_FUNCTION
*/
PHP_MSHUTDOWN_FUNCTION(akm)
{
	UNREGISTER_INI_ENTRIES();

	akm_dict_ht_free();

	return SUCCESS;
}
/* }}} */

/* Remove if there's nothing to do at request start */
/* {{{ PHP_RINIT_FUNCTION
*/
PHP_RINIT_FUNCTION(akm)
{
#if defined(COMPILE_DL_AKM) && defined(ZTS)
	ZEND_TSRMLS_CACHE_UPDATE();
#endif
	return SUCCESS;
}
/* }}} */

/* Remove if there's nothing to do at request end */
/* {{{ PHP_RSHUTDOWN_FUNCTION
*/
PHP_RSHUTDOWN_FUNCTION(akm)
{
	return SUCCESS;
}
/* }}} */

/* {{{ PHP_MINFO_FUNCTION
*/
PHP_MINFO_FUNCTION(akm)
{
	php_info_print_table_start();
	php_info_print_table_header(2, "akm support", "enabled");
	php_info_print_table_end();

	/* Remove comments if you have entries in php.ini
	   DISPLAY_INI_ENTRIES();
	   */
}
/* }}} */

/* {{{ akm_functions[]
 *
 * Every user visible function must have an entry in akm_functions[].
 */
const zend_function_entry akm_functions[] = {
	PHP_FE(akm_match,				NULL)
	PHP_FE_END	/* Must be the last line in akm_functions[] */
};
/* }}} */

/* {{{ akm_module_entry
*/
zend_module_entry akm_module_entry = {
	STANDARD_MODULE_HEADER,
	"akm",
	akm_functions,
	PHP_MINIT(akm),
	PHP_MSHUTDOWN(akm),
	PHP_RINIT(akm),		/* Replace with NULL if there's nothing to do at request start */
	PHP_RSHUTDOWN(akm),	/* Replace with NULL if there's nothing to do at request end */
	PHP_MINFO(akm),
	PHP_AKM_VERSION,
	STANDARD_MODULE_PROPERTIES
};
/* }}} */

#ifdef COMPILE_DL_AKM
#ifdef ZTS
ZEND_TSRMLS_CACHE_DEFINE();
#endif
ZEND_GET_MODULE(akm)
#endif

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */

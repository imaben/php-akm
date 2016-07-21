/*
   +----------------------------------------------------------------------+
   | PHP Version 5                                                        |
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
#include "standard/php_smart_str.h"

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
    FILE       *stream;
    char       *line;
    size_t      line_len = 0, read, i;

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
    zend_hash_add(akm_dict_ht, filename, strlen(filename), (void **)&trie, sizeof(akm_trie_t *), NULL);

    stream = fopen(fullpath, "r");

    if (!stream) {
        return;
    }

    while (-1 != (read = getline(&line, &line_len, stream))) {
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
    }
    free(line);
    fclose(stream);
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
            callback(ent->d_name, fullpath);
            success++;
        }
    }
    closedir(dir);
    return success;
}

static int akm_dict_ht_init()
{
    akm_dict_ht = pemalloc(sizeof(*akm_dict_ht), 1);
    if (akm_dict_ht == NULL) {
        return -1;
    }

    zend_hash_init(akm_dict_ht, 0, NULL, ZVAL_PTR_DTOR, 1);
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
        akm_trie_t **trie;
        for (zend_hash_internal_pointer_reset(akm_dict_ht);
            zend_hash_get_current_data(akm_dict_ht, (void **)&trie) == SUCCESS;
            zend_hash_move_forward(akm_dict_ht)
        ) {
            akm_trie_release (*trie);
        }
        pefree(akm_dict_ht, 1);
    }
}

static akm_trie_t *akm_get_trie(char *key, int len)
{
    akm_trie_t **trie;
    if (zend_hash_find(akm_dict_ht, key, len, (void **)&trie) == SUCCESS) {
        return *trie;
    }
    return NULL;
}

/* }}} */

/* {{{ PHP_INI
*/

ZEND_INI_MH(php_akm_enable)
{
    if (!new_value || new_value_length == 0) {
        return FAILURE;
    }

    if (!strcasecmp(new_value, "on") || !strcmp(new_value, "1")) {
        akm_enable = 1;
    } else {
        akm_enable = 0;
    }

    return SUCCESS;
}

ZEND_INI_MH(php_akm_dict_dir)
{
    if (!new_value || new_value_length == 0) {
        return FAILURE;
    }
    if (new_value[new_value_length] != '/') {
        akm_dict_dir = pemalloc(new_value_length + 2, 1);
        strcpy(akm_dict_dir, new_value);
        akm_dict_dir[new_value_length] = '/';
        akm_dict_dir[new_value_length + 1] = '\0';
    } else {
        new_value[new_value_length] = '\0';
        akm_dict_dir = strdup(new_value);
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

struct _akm_replace_params {
    zval *return_value;
    zend_fcall_info *fci;
    zend_fcall_info_cache *fci_cache;
    HashTable *ht;
};

static void akm_trie_traversal(akm_trie_t *trie,
        void(*callback)(const char *, size_t, zend_ulong, const char *, size_t, void *), void *args)
{
    akm_match_t m;
    unsigned int j;
    akm_pattern_t *pp;
    const char *keyword,
               *extension;

    size_t keyword_len,
           extension_len;

    zend_ulong offset;
    while ((m = akm_trie_findnext(trie)).size) {
        for (j = 0; j < m.size; j++) {
            pp = &m.patterns[j];
            keyword = pp->ptext.astring;
            keyword_len = pp->ptext.length;
            extension = pp->id.u.stringy;
            extension_len = (extension == NULL ? 0 : strlen(extension));
            offset = m.position;
            callback(keyword, keyword_len, offset, extension, extension_len, args);
        }
    }
}


static void akm_match_handler(const char *keyword, size_t keyword_len,
        zend_ulong offset, const char *extension, size_t extension_len, void *args)
{
    zval *return_value = args;
    zval *entry,
         *zkeyword,
         *zoffset,
         *zextension;

    MAKE_STD_ZVAL(entry);
    array_init_size(entry, 3);

    MAKE_STD_ZVAL(zkeyword);
    MAKE_STD_ZVAL(zoffset);
    MAKE_STD_ZVAL(zextension);
    ZVAL_STRINGL(zkeyword, keyword, keyword_len, 1);
    ZVAL_LONG(zoffset, offset);
    if (extension == NULL) {
        ZVAL_NULL(zextension);
    } else {
        ZVAL_STRINGL(zextension, extension, extension_len, 1);
    }

    zend_hash_add(Z_ARRVAL_P(entry), "keyword", sizeof("keyword") - 1, (void **)&zkeyword, sizeof(zval *), NULL);
    zend_hash_add(Z_ARRVAL_P(entry), "offset", sizeof("offset") - 1, (void **)&zoffset, sizeof(zval *), NULL);
    zend_hash_add(Z_ARRVAL_P(entry), "extension", sizeof("extension") - 1, (void **)&zextension, sizeof(zval *), NULL);
    zend_hash_index_update(Z_ARRVAL_P(return_value), Z_ARRVAL_P(return_value)->nNumOfElements, &entry, sizeof(zval *), NULL);
}

static void akm_replace_handler(const char *keyword, size_t keyword_len,
        zend_ulong offset, const char *extension, size_t extension_len, void *args)
{
    struct _akm_replace_params *params = (struct _akm_replace_params *)args;
    zval **cb_args[3];
    zval *retval = NULL,
         *entry,
         *zkeyword,
         *zoffset,
         *zextension,
         *rkeyword,
         *roffset;

    cb_args[0] = &zkeyword;
    cb_args[1] = &zoffset;
    cb_args[2] = &zextension;

    MAKE_STD_ZVAL(zkeyword);
    MAKE_STD_ZVAL(zoffset);
    MAKE_STD_ZVAL(zextension);

    ZVAL_STRINGL(zkeyword, keyword, keyword_len, 0);
    ZVAL_LONG(zoffset, offset);
    if (extension == NULL) {
        ZVAL_NULL(zextension);
    } else {
        ZVAL_STRINGL(zextension, extension, extension_len, 0);
    }

    params->fci->params = cb_args;
    params->fci->retval_ptr_ptr = &retval;


    if (zend_call_function(params->fci, params->fci_cache) == SUCCESS) {
        if (Z_TYPE_P(retval) == IS_STRING) {
            MAKE_STD_ZVAL(entry);
            array_init_size(entry, 3);
            MAKE_STD_ZVAL(rkeyword);
            MAKE_STD_ZVAL(roffset);
            ZVAL_STRINGL(rkeyword, keyword, keyword_len, 0);
            ZVAL_LONG(roffset, offset);
            zend_hash_add(Z_ARRVAL_P(entry), "keyword", sizeof("keyword"), (void **)&rkeyword, sizeof(zval *), NULL);
            zend_hash_add(Z_ARRVAL_P(entry), "offset", sizeof("offset"), (void **)&roffset, sizeof(zval *), NULL);
            zend_hash_add(Z_ARRVAL_P(entry), "replace", sizeof("replace"), (void **)&retval, sizeof(zval *), NULL);
            zend_hash_index_update(params->ht, params->ht->nNumOfElements, &entry, sizeof(zval *), NULL);
        }
    }
}

/* {{{ php function */

PHP_FUNCTION(akm_match)
{
    char *dict_name,
         *text;

    int dict_name_len,
        text_len;

    akm_text_t  chunk;

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
    akm_trie_traversal(trie, akm_match_handler, (void *)return_value);
}

PHP_FUNCTION(akm_replace)
{
    char *dict_name;
    zval *text;
    char *text_c;
    size_t text_l;

    size_t dict_name_len;
    zend_ulong replace_count = 0;

    HashTable *replace_ht;

    ALLOC_HASHTABLE(replace_ht);
    zend_hash_init(replace_ht, 0, NULL, ZVAL_PTR_DTOR, 0);

    zend_fcall_info fci = empty_fcall_info;
    zend_fcall_info_cache fci_cache = empty_fcall_info_cache;

    akm_text_t  chunk;
    akm_match_t match;

    zend_ulong idx = 0;

    if (zend_parse_parameters(ZEND_NUM_ARGS(), "szf", &dict_name, &dict_name_len,
                &text, &fci, &fci_cache) == FAILURE) {
        return;
    }

    if (Z_TYPE_P(text) != IS_STRING) {
        php_error_docref(NULL, E_ERROR, "argment 2 must be string", dict_name_len, dict_name);
        RETURN_FALSE;
    }

    akm_trie_t *trie = akm_get_trie(dict_name, dict_name_len);
    if (trie == NULL) {
        php_error_docref(NULL, E_WARNING, "Dict name #%.*s is not found", dict_name_len, dict_name);
        RETURN_FALSE;
    }

    text_c = Z_STRVAL_P(text);
    text_l = Z_STRLEN_P(text);

    fci.no_separation = 0;
    fci.param_count   = 3;

    chunk.astring = text_c;
    chunk.length  = text_l;

    akm_trie_settext (trie, &chunk, 0);

    struct _akm_replace_params params;
    params.return_value = return_value;
    params.fci = &fci;
    params.fci_cache = &fci_cache;
    params.ht = replace_ht;

    akm_trie_traversal(trie, akm_replace_handler, (void *)&params);

    if (zend_hash_num_elements(replace_ht) == 0) goto finally;

    smart_str replaced = { 0 };
    zend_ulong copied_idx = 0, last_copy_len = 0;
    int copy_len= 0;

    zval **entry,
         **keyword,
         **offset,
         **replace;

    for (zend_hash_internal_pointer_reset(replace_ht);
         zend_hash_get_current_data(replace_ht, (void **)&entry) == SUCCESS;
         zend_hash_move_forward(replace_ht)
    ) {
        zend_hash_find(Z_ARRVAL_P(*entry), "keyword", sizeof("keyword"), (void **)&keyword);
        zend_hash_find(Z_ARRVAL_P(*entry), "offset", sizeof("offset"), (void **)&offset);
        zend_hash_find(Z_ARRVAL_P(*entry), "replace", sizeof("replace"), (void **)&replace);

        copy_len = Z_LVAL_P(*offset) - copied_idx - Z_STRLEN_P(*keyword);

        if (copy_len <= 0 && idx != 0) { /* cover previous keyword */
            replaced.len -= last_copy_len;
            copied_idx -= last_copy_len;
            copy_len = 0;
            replace_count--;
        }

        if (copy_len)
            smart_str_appendl(&replaced, text_c + copied_idx, copy_len);

        smart_str_appendl(&replaced, Z_STRVAL_P(*replace), Z_STRLEN_P(*replace));

        last_copy_len = Z_STRLEN_P(*replace);
        replace_count++;

        copied_idx = Z_LVAL_P(*offset);
        idx++;
    }

    if (copied_idx < text_l) {
        smart_str_appendl(&replaced, text_c + copied_idx, text_l - copied_idx);
    }
    smart_str_0(&replaced);

    /* replace */
    zval_dtor(text);
    Z_STRVAL_P(text) = replaced.c;
    Z_STRLEN_P(text) = replaced.len;
	Z_TYPE_P(text) = IS_STRING;

finally:

    FREE_HASHTABLE(replace_ht);
    RETURN_LONG(replace_count);
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

    DISPLAY_INI_ENTRIES();
}
/* }}} */

ZEND_BEGIN_ARG_INFO_EX(arginfo_akm_match, 0, 0, 2)
	ZEND_ARG_INFO(0, dict_name)
	ZEND_ARG_INFO(0, text)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_akm_replace, 0, 0, 3)
	ZEND_ARG_INFO(0, dict_name)
	ZEND_ARG_INFO(1, text)
	ZEND_ARG_INFO(0, callback)
ZEND_END_ARG_INFO()

/* {{{ akm_functions[]
 *
 * Every user visible function must have an entry in akm_functions[].
 */
const zend_function_entry akm_functions[] = {
    PHP_FE(akm_match,   arginfo_akm_match)
    PHP_FE(akm_replace, arginfo_akm_replace)
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

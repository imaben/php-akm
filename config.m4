dnl $Id$
dnl config.m4 for extension akm

dnl Comments in this file start with the string 'dnl'.
dnl Remove where necessary. This file will not work
dnl without editing.

dnl If your extension references something external, use with:

PHP_ARG_WITH(akm, for akm support,
Make sure that the comment is aligned:
[  --with-akm             Include akm support])

dnl Otherwise use enable:

PHP_ARG_ENABLE(akm, whether to enable akm support,
Make sure that the comment is aligned:
[  --enable-akm           Enable akm support])

if test "$PHP_AKM" != "no"; then
  dnl Write more examples of tests here...

  dnl # --with-akm -> check with-path
  dnl SEARCH_PATH="/usr/local /usr"     # you might want to change this
  dnl SEARCH_FOR="/include/akm.h"  # you most likely want to change this
  dnl if test -r $PHP_AKM/$SEARCH_FOR; then # path given as parameter
  dnl   AKM_DIR=$PHP_AKM
  dnl else # search default path list
  dnl   AC_MSG_CHECKING([for akm files in default path])
  dnl   for i in $SEARCH_PATH ; do
  dnl     if test -r $i/$SEARCH_FOR; then
  dnl       AKM_DIR=$i
  dnl       AC_MSG_RESULT(found in $i)
  dnl     fi
  dnl   done
  dnl fi
  dnl
  dnl if test -z "$AKM_DIR"; then
  dnl   AC_MSG_RESULT([not found])
  dnl   AC_MSG_ERROR([Please reinstall the akm distribution])
  dnl fi

  dnl # --with-akm -> add include path
  dnl PHP_ADD_INCLUDE($AKM_DIR/ahocorasick)

  dnl # --with-akm -> check for lib and symbol presence
  dnl LIBNAME=akm # you may want to change this
  dnl LIBSYMBOL=akm # you most likely want to change this

  dnl PHP_CHECK_LIBRARY($LIBNAME,$LIBSYMBOL,
  dnl [
  dnl   PHP_ADD_LIBRARY_WITH_PATH($LIBNAME, $AKM_DIR/$PHP_LIBDIR, AKM_SHARED_LIBADD)
  dnl   AC_DEFINE(HAVE_AKMLIB,1,[ ])
  dnl ],[
  dnl   AC_MSG_ERROR([wrong akm lib version or lib not found])
  dnl ],[
  dnl   -L$AKM_DIR/$PHP_LIBDIR -lm
  dnl ])
  dnl
  dnl PHP_SUBST(AKM_SHARED_LIBADD)

  PHP_NEW_EXTENSION(akm, akm.c ahocorasick/ahocorasick.c ahocorasick/mpool.c ahocorasick/node.c ahocorasick/replace.c, $ext_shared,, -DZEND_ENABLE_STATIC_TSRMLS_CACHE=1)
fi

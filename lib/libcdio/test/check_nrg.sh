#!/bin/sh
#$Id: check_nrg.sh.in,v 1.11 2004/09/04 00:06:50 rocky Exp $

if test -n "-L/usr/local/lib -lvcdinfo -liso9660 -lcdio -lm  " ; then
  vcd_opt='--no-vcd'
fi

if test -z $srcdir ; then
  srcdir=`pwd`
fi

. ${srcdir}/check_common_fn

if test ! -x ../src/cd-info ; then
  exit 77
fi

BASE=`basename $0 .sh`
test_name=videocd

test_cdinfo "--quiet --no-device-info --nrg-file ${srcdir}/${test_name}.nrg $vcd_opt --iso9660 " \
  ${test_name}.dump ${srcdir}/${test_name}.right
RC=$?
check_result $RC 'cd-info NRG test 1'

BASE=`basename $0 .sh`
nrg_file=${srcdir}/monvoisin.nrg

if test -f  $nrg_file ; then
  test_cdinfo "-q --no-device-info --nrg-file $nrg_file $vcd_opt --iso9660 " \
    monvoisin.dump ${srcdir}/monvoisin.right
  RC=$?
  check_result $RC 'cd-info NRG test 2'
else 
  echo "Don't see NRG file ${nrg_file}. Test skipped."
  exit 0
fi

test_name='svcdgs'
nrg_file=${srcdir}/${test_name}.nrg
if test -f  $nrg_file ; then
  test_cdinfo "-q --no-device-info --nrg-file $nrg_file $vcd_opt --iso9660" \
    ${test_name}.dump ${srcdir}/${test_name}.right
  RC=$?
  check_result $RC "cd-info NRG $test_name"
else 
  echo "Don't see NRG file ${nrg_file}. Test skipped."
  exit $SKIP_TEST_EXITCODE
fi

test_name='cdda-mcn'
nrg_file=${srcdir}/${test_name}.nrg
if test -f  $nrg_file ; then
  test_cdinfo "-q --no-device-info --nrg-file $nrg_file --no-cddb" \
    ${test_name}.dump ${srcdir}/${test_name}.right
  RC=$?
  check_result $RC "cd-info NRG $test_name"
  
  exit $RC
else 
  echo "Don't see NRG file ${nrg_file}. Test skipped."
  exit $SKIP_TEST_EXITCODE
fi

#;;; Local Variables: ***
#;;; mode:shell-script ***
#;;; eval: (sh-set-shell "bash") ***
#;;; End: ***

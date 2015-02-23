#!/usr/bin/env python

import subprocess, glob, sys, os

tests = glob.glob('pthread_*')

tests = filter(lambda x: not 'pthread_atfork' in x, tests)
tests = filter(lambda x: not 'pthread_sigmask' in x, tests)

#tests = filter(lambda x: 'pthread_attr_init' in x or 'pthread_barrier_init' in x, tests)

print 'Running ' + str(len(tests)) + ' tests: ' + str(tests)
print ''

total_num_tests_passed = 0
total_num_tests_skipped = 0
total_num_tests_unknown = 0
total_num_tests_failed = 0

origcwd = os.getcwd()
for t in tests:
  os.chdir(os.path.join(origcwd, t))
  proc = subprocess.Popen('run_emcc_tests.py', stdout=subprocess.PIPE, stderr=subprocess.PIPE)
  (stdout, stderr) = proc.communicate()
  stdout = stdout.strip()
  stderr = stderr.strip()
  if len(stdout) > 0:
    num_tests_passed = 0
    num_tests_skipped = 0
    num_tests_failed = 0
    num_tests_unknown = 0
    n = '\n'.join(filter(lambda x: ' tests passed:' in x, stdout.split('\n'))).strip().split()
    if len(n) > 0:
      num_tests_passed = int(n[0])
      total_num_tests_passed += num_tests_passed

    n = '\n'.join(filter(lambda x: ' tests failed:' in x, stdout.split('\n'))).strip().split()
    if len(n) > 0:
      num_tests_failed = int(n[0])
      total_num_tests_failed += num_tests_failed

    n = '\n'.join(filter(lambda x: ' tests skipped:' in x, stdout.split('\n'))).strip().split()
    if len(n) > 0:
      num_tests_skipped = int(n[0])
      total_num_tests_skipped += num_tests_skipped

    n = '\n'.join(filter(lambda x: ' tests finished with unknown result:' in x, stdout.split('\n'))).strip().split()
    if len(n) > 0:
      num_tests_unknown = int(n[0])
      total_num_tests_unknown += num_tests_unknown

    if '--verbose' in sys.argv or len(stderr) > 0 or num_tests_failed > 0 or num_tests_unknown > 0:
      print stdout
    else:
      print t + '/: ' + str(num_tests_passed) + ' passed, ' + str(num_tests_skipped) + ' skipped.'

  if len(stderr) > 0:
    print stderr

print ''
print '-------------- SUMMARY --------------'
print str(total_num_tests_passed + total_num_tests_failed + total_num_tests_skipped + total_num_tests_unknown) + ' tests run total, of which:'
print '  ' + str(total_num_tests_passed) + ' tests passed.'
print '  ' + str(total_num_tests_failed) + ' tests failed.'
print '  ' + str(total_num_tests_skipped) + ' tests skipped.'
print '  ' + str(total_num_tests_unknown) + ' tests finished with unknown result.'
print ''

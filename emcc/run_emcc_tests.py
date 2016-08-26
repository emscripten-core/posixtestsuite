#!/usr/bin/env python

import subprocess, glob, sys, os

if len(sys.argv) > 1:
  tests = sys.argv[1:]
else:
  tests = filter(lambda x: x[0].isdigit(), glob.glob('*.c'))
print ''
print '---------------------------------- ' + os.path.basename(os.getcwd()) + '/ ----------------------------------'
print 'Running ' + str(len(tests)) + ' tests: ' + str(tests) + ' in ' + os.getcwd()
print ''

successes = []
failures = []
indeterminate = []
skipped = []

if not os.environ.get('EMSCRIPTEN_BROWSER'):
  print >> sys.stderr, 'Please set EMSCRIPTEN_BROWSER environment variable to point to the target browser executable to run!'
  sys.exit(1)

for f in tests:
  out = f.replace('.c', '.html')
  cmd = ['emcc', f, '-s', 'USE_PTHREADS=1', '-Wno-format-security', '-s', 'TOTAL_MEMORY=268435456', '-s', 'PTHREAD_POOL_SIZE=40', '-o', out, '-I../../../include', '--emrun']
  print ' '.join(cmd)
  subprocess.call(cmd)
  cmd = ['emrun', '--kill_start', '--kill_exit', '--timeout=15', '--silence_timeout=15', '--browser='+os.getenv('EMSCRIPTEN_BROWSER'), '--safe_firefox_profile', out]
  #print ' '.join(cmd)
  proc = subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
  (stdout, stderr) = proc.communicate()
  stdout = '\n'.join(filter(lambda x: not x.startswith('2015-'), stdout.split('\n')))
  stderr = '\n'.join(filter(lambda x: not x.startswith('2015-'), stderr.split('\n')))
  stdout = stdout.strip()
  stderr = stderr.strip()
  if 'test skipped' in stdout.lower():
    skipped += [f]
    reason = '\n'.join(filter(lambda x: 'Test SKIPPED' in x, stdout.split('\n')))
    print reason.strip()
  elif proc.returncode == 0 and ('test pass' in stdout.lower() or 'Test executed successfully.' in stdout):
    successes += [f]
    print 'PASS'
  elif proc.returncode != 0:
    failures += [f]
    print 'FAILED:'
#    print 'stdout:'
    if len(stdout) > 0: print stdout
#    print ''
#    print 'stderr:'
    print stderr
  else:
    indeterminate += [f]
    print 'UNKNOWN:'
#    print 'stdout:'
    if len(stdout) > 0: print stdout
#    print ''
#    print 'stderr:'
    print stderr
  print ''

print '=== FINISHED ==='
if len(successes) > 0:
  print str(len(successes)) + ' tests passed: '
  print ', '.join(successes)
  print ''

if len(failures) > 0:
  print str(len(failures)) + ' tests failed: '
  print ', '.join(failures)
  print ''

if len(indeterminate) > 0:
  print str(len(indeterminate)) + ' tests finished with unknown result: '
  print ', '.join(indeterminate)
  print ''

if len(skipped) > 0:
  print str(len(skipped)) + ' tests skipped: '
  print ', '.join(skipped)
  print ''
print ''
print ''

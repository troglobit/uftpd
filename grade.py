#! /usr/bin/python
import subprocess
import random
import time
import filecmp
import struct
import os
import shutil
import string
from ftplib import FTP

credit = 40
minor = 3
major = 8

def build():
  global credit
  proc = subprocess.Popen('make', stdout=subprocess.PIPE, stderr=subprocess.PIPE)
  while True:
    stdout = proc.stdout.readline()
    stderr = proc.stderr.readline()
    if not (stdout and stderr):
      break
    if stdout and '-Wall' not in stdout:
      print 'No -Wall argument'
      print 'Your credit is 0'
      exit(0)
    if stderr and credit == 40:
      print 'There are warnings when compiling your program'
      credit -= major

def create_test_file(filename):
  f = open(filename, 'wb')
  for i in xrange(10000):
    data = struct.pack('d', random.random())
    f.write(data)
  f.close()

def test(port=21, directory='/tmp'):
  global credit
  if port == 21 and directory == '/tmp':
    server = subprocess.Popen('./server', stdout=subprocess.PIPE)
  else:
    server = subprocess.Popen(['./server', '-port', '%d' % port, '-root', directory], stdout=subprocess.PIPE)
  time.sleep(0.1)
  try:
    ftp = FTP()
    # connect
    if not ftp.connect('127.0.0.1', port).startswith('220'):
      print 'You missed response 220'
      credit -= minor
    # login
    if not ftp.login().startswith('230'):
      print 'You missed response 230'
      credit -= minor
    # SYST
    if ftp.sendcmd('SYST') != '215 UNIX Type: L8':
      print 'Bad response for SYST'
      credit -= minor
    # TYPE
    if ftp.sendcmd('TYPE I') != '200 Type set to I.':
      print 'Bad response for TYPE I'
      credit -= minor
    # PORT download
    filename = 'test%d.data' % random.randint(100, 200)
    create_test_file(directory + '/' + filename)
    ftp.set_pasv(False)
    if not ftp.retrbinary('RETR %s' % filename, open(filename, 'wb').write).startswith('226'):
      print 'Bad response for RETR'
      credit -= minor
    if not filecmp.cmp(filename, directory + '/' + filename):
      print 'Something wrong with RETR'
      credit -= major
    os.remove(directory + '/' + filename)
    os.remove(filename)
    # PASV upload
    ftp2 = FTP()
    ftp2.connect('127.0.0.1', port)
    ftp2.login()
    filename = 'test%d.data' % random.randint(100, 200)
    create_test_file(filename)
    print filename
    if not ftp2.storbinary('STOR %s' % filename, open(filename, 'rb')).startswith('226'):
      print 'Bad response for STOR'
      credit -= minor
    if not filecmp.cmp(filename, directory + '/' + filename):
      print 'Something wrong with STOR'
      credit -= major
    #os.remove(directory + '/' + filename)
    #os.remove(filename)
    # QUIT
    if not ftp.quit().startswith('221'):
      print 'Bad response for QUIT'
      credit -= minor
    ftp2.quit()
  except Exception as e:
    print 'Exception occurred:', e
    credit = 0
  server.kill()

build()
# Test 1
test()
# Test 2
port = random.randint(2000, 3000)
directory = ''.join(random.choice(string.ascii_letters) for x in xrange(10))
if os.path.isdir(directory):
  shutil.rmtree(directory)
os.mkdir(directory)
print port
print directory
test(port, directory)
shutil.rmtree(directory)
# Clean
subprocess.Popen(['make', 'clean'], stdout=subprocess.PIPE)
# Result
print 'Your credit is %d' % credit

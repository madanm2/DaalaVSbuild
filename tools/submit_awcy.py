#!/usr/bin/env python

from __future__ import print_function

import requests
import argparse
import os
import subprocess
import sys

if 'DAALA_ROOT' not in os.environ:
    print("Please specify the DAALA_ROOT environment variable to use this tool.")
    sys.exit(1)

key = None
with open('secret_key','r') as keyfile:
    key = keyfile.read().strip()

if key is None:
    print("Could not open secret_key")
    sys.exit(1)

daala_root = os.environ['DAALA_ROOT']
os.chdir(daala_root)

branch = subprocess.check_output('git symbolic-ref -q --short HEAD',shell=True).strip()

parser = argparse.ArgumentParser(description='Submit test to arewecompressedyet.com')
parser.add_argument('-prefix',default=branch)
args = parser.parse_args()

commit = subprocess.check_output('git rev-parse HEAD',shell=True).strip()
short = subprocess.check_output('git rev-parse --short HEAD',shell=True).strip()
date = subprocess.check_output(['git','show','-s','--format=%ci',commit]).strip()
date_short = date.split()[0]
user = args.prefix

run_id = user+'-'+date_short+'-'+short

print('Creating run '+run_id)
r = requests.post("https://arewecompressedyet.com/submit/job", {'run_id': run_id, 'commit': commit, 'key': key})
print(r)

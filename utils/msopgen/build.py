#!/usr/bin/env python3
# -*- coding: utf-8 -*-
import os
import subprocess
import logging
import argparse
import sys
import glob


def exec_cmd(cmd):
    result = subprocess.run(cmd, capture_output=False, text=True, timeout=3600)
    if result.returncode != 0:
        logging.error("execute command %s failed, please check the log", " ".join(cmd))
        sys.exit(result.returncode)


def run_tests():
    script_dir = os.path.dirname(os.path.abspath(__file__))
    test_dir = os.path.join(script_dir, 'test')
    os.chdir(test_dir)
    subprocess.run([sys.executable, 'run_test.py'], check=True)

 
if __name__ == '__main__':
    parser = argparse.ArgumentParser(description='Build script with optional testing')
    parser.add_argument('command', nargs='?', default='build', 
                       choices=['build', 'test'],
                       help='Command to execute (build or test)')
    
    args = parser.parse_args()
    current_dir = os.path.abspath(os.path.dirname(os.path.realpath(__file__)))
    os.chdir(current_dir)
    if args.command == 'test':
        run_tests()
    else:
        # msopgen打whl包
        exec_cmd([sys.executable, 'setup.py', 'egg_info', '--egg-base', 'build', 'bdist_wheel', '--dist-dir',  os.path.join(current_dir, 'output')])
        whl_package = glob.glob(os.path.join(current_dir, "dist", "*.whl"))
        for file in whl_package:
            os.chmod(file, 0o550)

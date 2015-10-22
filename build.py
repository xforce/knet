import optparse
import os
import subprocess
import sys
import multiprocessing

from sys import platform as _platform

try:
    from _winreg import *
except ImportError:
    print "No Windows"

parser = optparse.OptionParser()
parser.add_option('--debug',
    action='store_true',
    dest='debug',
    help='build in debug [Without its building release]')


(options, args) = parser.parse_args()

def GetMSBuildPath():
    aReg = ConnectRegistry(None,HKEY_LOCAL_MACHINE)

    aKey = OpenKey(aReg, r"SOFTWARE\Microsoft\MSBuild\ToolsVersions\14.0")
    try:
        val=QueryValueEx(aKey, "MSBuildToolsPath")[0] + "MSBuild.exe"
        return val
    except EnvironmentError:
        print "Unable to find msbuild"
        return ""

def execute(argv, env=os.environ):
  try:
    subprocess.check_call(argv)
    return 0
  except subprocess.CalledProcessError as e:
    print e.returncode
  raise e


def execute_stdout(argv, env=os.environ):
  return execute(argv, env)

def RunMSBuild(configuration):
    msbuild = GetMSBuildPath()
    if msbuild == "":
        return
    var = execute_stdout([msbuild, "knet.sln", "/t:Build", "/p:Configuration=" + configuration])
    return var

def RunMake(configuration):
    var = execute_stdout(["make","-C", "out", "-j" + str(multiprocessing.cpu_count())])
    return var

if _platform == "win32":
    if options.debug:
        sys.exit(RunMSBuild("Debug"))
    else:
        sys.exit(RunMSBuild("Release"))

if _platform.startswith("linux"):
    if options.debug:
        sys.exit(RunMake("Debug"))
    else:
        sys.exit(RunMake("Release"))

#!/usr/bin/env python
# -*- coding: utf-8 -*-
import os
import sys
import time
import sh
import platform
import json

class bcolors:
    HEADER = '\033[95m'
    OKBLUE = '\033[94m'
    OKGREEN = '\033[92m'
    WARNING = '\033[93m'
    FAIL = '\033[91m'
    ENDC = '\033[0m'
    BOLD = '\033[1m'
    UNDERLINE = '\033[4m'

def get_git_version():
    version = sh.git.describe("--tags", "--always", "--dirty").strip()
    return version


def get_prg_version(base_dir, argv):
    git_version = get_git_version()
    v_data_time_str = time.strftime('%Y%m%d', time.localtime(time.time()))
    version = "".join(argv) + "_" + git_version + "_" + v_data_time_str
    print(bcolors.OKGREEN + version + bcolors.ENDC)
    return version


def get_main_dir():
    """Return the script directory - whether we're frozen or not."""
    return os.path.abspath(os.path.dirname(sys.argv[0]))


def get_system_info():
    sysinfo = ""
    for i in platform.uname()[0:3]:
        sysinfo += i + "@"
    sysinfo = sysinfo[0:-1]
    print(sysinfo)
    return sysinfo


def get_complier_info(complier_name="arm-gcc"):
    complier_info = ""
    cinfo = complier_name
    if complier_name == "arm-gcc":
        complier_info = str(sh.Command("arm-none-eabi-gcc")("--version"))
    import re
    version = re.findall(r'\w{1,5}-\d{1,5}-\w{1,10}-\w{1,10}', complier_info)
    date = re.findall(r'\d{8}', complier_info)
    vnumber = re.findall(r'\d{1,3}\.\d{1,3}\.\d{1,3}', complier_info)
    for v in (version, vnumber, date):
        if v:
            cinfo += "@" + v[0]
    print("%s" % cinfo)
    return cinfo


def gen_board_prototype(base_dir, version_info):
    with open(base_dir+'/board_prototype.template', 'r') as f:
        config = json.load(f)
        config['version'] = version_info["version"][1:]
        with open(base_dir+'/board.prototype', 'w') as f_w:
            json.dump(config, f_w, indent=4)

def gen_prg_header_file(base_dir, version_info):
    header_file_name = "/bl_version.h"
    f_version_h = open(base_dir + header_file_name, "w")
    f_version_h.writelines("//" + version_info["version"] + "\n")
    f_version_h.writelines("#ifndef _BL_VERSION_H_\n")
    f_version_h.writelines("#define _BL_VERSION_H_\n")
    f_version_h.writelines("#define FULL_BL_VERSION \"%s\"\n" % version_info["version"])
    ver_str = version_info["version"].replace("_v", "").split("_")[0]
    spl_ver = ver_str.split("-")
    major_min = spl_ver[0].split(".")
    major_ver = int(major_min[0])
    minor_ver = int(major_min[1])
    rel_ver = int(spl_ver[1])
    print(major_ver, minor_ver, rel_ver)
    bl_ver_code = (major_ver << 16) + (minor_ver << 8) + rel_ver
    f_version_h.writelines("#define BL_VERSION_CODE %d\n" % bl_ver_code)
    f_version_h.writelines("#endif\n")
    f_version_h.close()


def main(argv):
    # if "".join(argv) == "clean":
        # run_prg("git checkout prg_version.h")
        # return
    base_dir = get_main_dir()
    version_info = {"version": "", "sysinfo": "", "complier": ""}
    try:
        version_info["version"] = get_prg_version(base_dir, argv)
        version_info["sysinfo"] = get_system_info()
        version_info["complier"] = get_complier_info()
    except Exception as e:
        print("GenVersionException:%s" % e)
    gen_prg_header_file(base_dir, version_info)

if __name__ == '__main__':
    main(sys.argv[1:])

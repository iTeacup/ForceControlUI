import os
import time

VERSION_FILE = "Version.h"


def find_best_encoding(c_file):
    for encoding in ['utf-8', 'utf-8-sig', 'gbk']:
        try:
            with open(c_file, 'r', encoding=encoding) as fp:
                fp.readline()
                return encoding
        except:
            continue
    return None


def cur_time_str():
    return time.strftime('%Y/%m/%d %H:%M:%S', time.localtime(time.time()))


def update_build_info(file_name):
    encoding = find_best_encoding(file_name)
    lines = open(file_name, 'r', encoding=encoding).readlines()
    with open(file_name, 'w', encoding=encoding) as fp:
        for line in lines:
            if 'VERSION_BUILD_NO' in line:
                build_no_str = line.split()[-1]
                new_build_no_str = str(int(build_no_str) + 1)
                line = '#define VERSION_BUILD_NO\t\t\t{}\n'.format(new_build_no_str)
                print('update build no to {}'.format(new_build_no_str))
            elif 'VERSION_BUILD_TIME' in line:
                t = cur_time_str()
                line = '#define VERSION_BUILD_TIME\t\t\t"{}"\n'.format(t)
                print('update build time to {}'.format(t))
            fp.write(line)


def main():
    file_name = os.path.join(os.getcwd(), VERSION_FILE)
    update_build_info(file_name)


if __name__ == '__main__':
    main()

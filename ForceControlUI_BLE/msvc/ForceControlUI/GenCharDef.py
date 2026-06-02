import time

# 要转换的中文字符列表
STRING_LIST = [
	'辑帧屏楷姚咨弧馈'
	]


def main():
	TARGET_FILE_NAME = 'UICharDef.h'
	FILE_HEADER_STR = ['#pragma once\n',
					'// This file is auto-generated, please do not modify, use the python script to update\n',
					'// update time: {}\n\n'.format(time.strftime('%Y/%m/%d %H:%M:%S', time.localtime()))]

	with open(TARGET_FILE_NAME, 'w') as fp:
		for line in FILE_HEADER_STR:
			fp.write(line)
		fp.write('unsigned short EXTRA_IMCHAR_LIST[] = {\n')

		for s in STRING_LIST:
			for c in s:
				value = c.encode('unicode_escape').decode()
				value = '\t0x{},\t//{}\n'.format(value.replace('\\u',''), c)
				fp.write(value)
		fp.write('\t0\n')
		fp.write('};\n')


if __name__ == '__main__':
	main()

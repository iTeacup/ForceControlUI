import time

# 要转换的中文字符列表
STRING_LIST = [
	'辑帧屏楷姚咨弧馈',
	'戳轴',
	'一上下不与且个中为于五人令伸位余作使例保信值停先入关其内出到前加动单压反变可号名后向启周和器四固在均型处备复外大失始完定实对射层已常并应度开异当待态恢成或戳所手择持指按据接推效数整断新方无时映暂更最有期未机标样模止正比法测点牙状用电留的目直确秒称移稳等置肘节蓝藏行览角认设试请调败超足跟路软载输运近进连选速采重间际限随隐集零需静须预馈',
	'习共具制化取同回学小就广必打控播操智本果步监神练经结络绪网能脚训身通道遥',
	'之也产从件份会低侧修候允全公再写准分列初别刷刻功务匹协历原参发叠只史右合命坐基存导将少尚强录总抗捕换插支收文明显替服束查检次求没波流混满滤版率生盘相知码磁示离稍空精系线组续缺联自至表被要议记许证识话该误说资轮过退送邻部配链错闭队降隔频验齐',
	'冻围平延归才是滑短继范越迟默',
	'减去',
	'各宽工放粗终非',
	'响快慢敏每灵缓都',
	'悬看细详题',
	'但径权'
	]


def main():
	TARGET_FILE_NAME = 'UICharDef.h'
	FILE_HEADER_STR = ['#pragma once\n',
					'// This file is auto-generated, please do not modify, use the python script to update\n',
					'// update time: {}\n\n'.format(time.strftime('%Y/%m/%d %H:%M:%S', time.localtime()))]

	with open(TARGET_FILE_NAME, 'w', encoding='utf-8-sig') as fp:
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

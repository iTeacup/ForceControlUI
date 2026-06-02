#include <locale>
#include <iostream>
#include "UIApplication.h"

int main(int, char**)
{
	// 设置locale，支持中文
	setlocale(LC_ALL, "LC_CTYPE=zh_CN.UTF-8");

	//UIApplication app;
	try
	{
		g_app.Run();
	}
	catch (const std::runtime_error& theError)
	{
		std::cerr << theError.what() << std::endl;
		return EXIT_FAILURE;
	}
	return 0;
}

#pragma once
#include "Version.h"

#define _TO_VERSION_STR(a,b,c,d)	#a "." #b "." #c "." #d
#define TO_VERSION_STR(a,b,c,d)		_TO_VERSION_STR(a,b,c,d)

#define VERSION_INFO_STR			TO_VERSION_STR(VERSION_MAJOR, VERSION_MINOR, VERSION_REVISION_NO, VERSION_BUILD_NO)

// 软件标题
#define UI_WIN_TITLE	"ForceControlUI-" VERSION_INFO_STR

// company name
#define APP_COMPANY_NAME			"个人有限公司"
// copy right
#define APP_COPYRIGHT				"Copyright (C) 2022"

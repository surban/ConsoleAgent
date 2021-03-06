#pragma once

#ifndef STRICT
#define STRICT
#endif

#include "targetver.h"

#define _ATL_APARTMENT_THREADED
#define _ATL_NO_AUTOMATIC_NAMESPACE
#define _ATL_CSTRING_EXPLICIT_CONSTRUCTORS	// some CString constructors will be explicit
#define ATL_NO_ASSERT_ON_DESTROY_NONEXISTENT_WINDOW


#include "resource.h"
#include <atlbase.h>
#include <atlcom.h>
#include <atlctl.h>
#include <atlfile.h>
#include <atlsecurity.h>

#include <comutil.h>
#include <sddl.h>
#include <WtsApi32.h>
#include <ShlObj.h>

#include <string>
#include <vector>
#include <map>



#define ELPP_UNICODE
#include "easylogging++.h"



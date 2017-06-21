{
	"target_defaults": {
		"cflags" : ["-std=c++11", "-Wall", "-Wextra", "-Wno-unused-parameter", "-fexceptions"],
		"cflags_cc" : ["-std=c++11", "-Wall", "-Wextra", "-Wno-unused-parameter", "-fexceptions"],
		"defines": [ "V8_DEPRECATION_WARNINGS=1" ],
		"include_dirs": ["<!(node -e \"require('nan')\")"],
		"target_conditions": [
			['OS=="linux"', {
				"cflags" : ["-std=c++11", "-Wall", "-Wextra", "-Wno-unused-parameter", "-fexceptions"],
				"cflags_cc" : ["-std=c++11", "-Wall", "-Wextra", "-Wno-unused-parameter", "-fexceptions"],
				"ldflags": [ "-pthread" ],
			}],
			['OS=="mac"', {
				"xcode_settings": {
					"GCC_ENABLE_CPP_EXCEPTIONS": "YES",
					"CLANG_CXX_LIBRARY": "libc++",
					"GCC_ENABLE_CPP_EXCEPTIONS": "YES",
					"MACOSX_DEPLOYMENT_TARGET": "10.10",
					"OTHER_CPLUSPLUSFLAGS": ["-std=c++11", "-Wall", "-Wextra", "-Wno-unused-parameter", "-fexceptions"],
				}
			}]
		]
	}, "targets": [
		{
			"target_name" : "eventemitter",
			"sources"     : [ "cpp/eventemitter.cpp" ]
		},
	]
}

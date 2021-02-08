{
  "targets": [
    {
      "target_name"  : "wamem",
      "conditions": [
        ["OS=='mac'", {
          "xcode_settings": {
            "GCC_ENABLE_CPP_RTTI": "YES",
            "GCC_ENABLE_CPP_EXCEPTIONS": "YES"
          },
        }]
      ],
      "sources": [
        "src/v8-factory.cc",
        "src/vm.cc",
        "src/trap.cc",
        "src/api.cc"
      ],
      "include_dirs": [
        "node/deps/v8",
      ],
      'cflags!': [
        '-fno-exceptions',
	      '-fno-rtti'
       ],
      'cflags_cc!': [
        '-fno-exceptions',
	      '-fno-rtti'
      ],
      "cflags": [
        '-Wall',
        '-O2',
        '-O3',
        '-std=c++11'
      ]
    }
  ]
}
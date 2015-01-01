{
  'targets': [
    {
      'target_name': 'api',
      'type': 'executable',
      'sources': [
        'api.cc',
      ],
      'include_dirs': [
        'http-server',
        'http-server/libuv/include',
      ],
      'dependencies': [
        'http-server/http_server.gyp:http_server',
      ],
      'cflags_cc': [ '-std=c++11' ],
      'conditions': [
        ['OS == "mac"', {
          'xcode_settings': {
            'CLANG_CXX_LANGUAGE_STANDARD': 'c++11',
            'OTHER_CPLUSPLUSFLAGS': [ '-stdlib=libc++' ]
          }
        }],
      ]
    },
  ],
}

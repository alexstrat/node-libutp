{
  'variables' : {
    'libutp' : 'deps/libutp'
  },

  'targets': [
    {
      'target_name': 'utp_wrap',
      'defines' : ['POSIX'],
      'sources': [
        './src/utp_wrap.cc',
        '<@(libutp)/utp.cpp',
        '<@(libutp)/utp_utils.cpp'
      ],
      'include_dirs' : [
        '<@(libutp)',
        './src'
      ]
   }
  ]
}
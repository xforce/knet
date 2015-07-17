{
  'includes': ['toolchain.gypi'],
  'variables': {
    'debug_warning_level_win%': '4',
    'component%': 'static_library',
    'msvs_multi_core_compile%': '1',
    'optimized_debug%': '0',
    'knet_target_type%': 'static_library',
    'variables': {
      'variables': {
        'variables': {
          'conditions': [
            ['OS=="linux" or OS=="freebsd" or OS=="openbsd" or \
               OS=="netbsd" or OS=="mac" or OS=="qnx"', {
              # This handles the Unix platforms we generally deal with.
              # Anything else gets passed through, which probably won't work
              # very well; such hosts should pass an explicit target_arch
              # to gyp.
              'host_arch%': '<!pymod_do_main(detect_host_arch)',
            }, {
              # OS!="linux" and OS!="freebsd" and OS!="openbsd" and
              # OS!="netbsd" and OS!="mac"
              'host_arch%': 'ia32',
            }],
          ],
        },
        'host_arch%': '<(host_arch)',
        'target_arch%': '<(host_arch)',
      },
      'host_arch%': '<(host_arch)',
      'target_arch%': '<(target_arch)',
    },
    'host_arch%': '<(host_arch)',
    'target_arch%': '<(target_arch)',
    'werror%': '-Werror',
},
'target_defaults': {
    'default_configuration': 'Debug',
    'configurations': {
      'DebugBaseCommon': {
        'cflags': [ '-g', '-O0' ],
        'conditions': [
          ['(target_arch=="ia32" or target_arch=="x87") and \
            OS=="linux"', {
            'defines': [
              '_GLIBCXX_DEBUG'
            ],
          }],
        ],
      },
      'Optdebug': {
        'inherit_from': [ 'DebugBaseCommon', 'DebugBase2' ],
      },
      'Debug': {
        # Xcode insists on this empty entry.
      },
      'Release': {
        # Xcode insists on this empty entry.
      },
    },
  },
}
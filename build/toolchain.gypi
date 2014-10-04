# Copyright 2013 the V8 project authors. All rights reserved.
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
#     * Redistributions of source code must retain the above copyright
#       notice, this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above
#       copyright notice, this list of conditions and the following
#       disclaimer in the documentation and/or other materials provided
#       with the distribution.
#     * Neither the name of Google Inc. nor the names of its
#       contributors may be used to endorse or promote products derived
#       from this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

# Shared definitions for all V8-related targets.

{
  'variables': {
    'msvs_use_common_release': 0,
    'gcc_version%': 'unknown',
    'clang%': 0,
    'knet_target_arch%': '<(target_arch)',

    # Default arch variant for MIPS.
    'mips_arch_variant%': 'r2',

    # Possible values fp32, fp64, fpxx.
    # fp32 - 32 32-bit FPU registers are available, doubles are placed in
    #        register pairs.
    # fp64 - 32 64-bit FPU registers are available.
    # fpxx - compatibility mode, it chooses fp32 or fp64 depending on runtime
    #        detection
    'mips_fpu_mode%': 'fp32',

    # Some versions of GCC 4.5 seem to need -fno-strict-aliasing.
    'knet_no_strict_aliasing%': 0,

    # Chrome needs this definition unconditionally. For standalone V8 builds,
    # it's handled in build/standalone.gypi.
    'want_separate_host_toolset%': 1,

    'host_os%': '<(OS)',
    'werror%': '-Werror',
    # For a shared library build, results in "libv8-<(soname_version).so".
    'soname_version%': '',

    # Allow to suppress the array bounds warning (default is no suppression).
    'wno_array_bounds%': '',

    # Link-Time Optimizations
    'use_lto%': 0,
  },
  'conditions': [
    ['host_arch=="ia32" or host_arch=="x64" or clang==1', {
      'variables': {
        'host_cxx_is_biarch%': 1,
       },
     }, {
      'variables': {
        'host_cxx_is_biarch%': 0,
      },
    }],
    ['target_arch=="ia32" or target_arch=="x64" or target_arch=="x87" or \
      clang==1', {
      'variables': {
        'target_cxx_is_biarch%': 1,
       },
     }, {
      'variables': {
        'target_cxx_is_biarch%': 0,
      },
    }],
  ],
  'target_defaults': {
    'conditions': [
      ['knet_target_arch=="arm64"', {
        'defines': [
          'KNET_TARGET_ARCH_ARM64',
        ],
      }],
      ['knet_target_arch=="ia32"', {
        'defines': [
          'KNET_TARGET_ARCH_IA32',
        ],
      }],  # knet_target_arch=="ia32"
      ['knet_target_arch=="x64"', {
        'defines': [
          'KNET_TARGET_ARCH_X64',
        ],
        'xcode_settings': {
          'ARCHS': [ 'x86_64' ],
        },
        'msvs_settings': {
          'VCLinkerTool': {
            'StackReserveSize': '2097152',
          },
        },
        'msvs_configuration_platform': 'x64',
      }],  # knet_target_arch=="x64"
      ['knet_target_arch=="x32"', {
        'defines': [
          # x32 port shares the source code with x64 port.
          'KNET_TARGET_ARCH_X64',
          'KNET_TARGET_ARCH_32_BIT',
        ],
        'cflags': [
          '-mx32',
          # Inhibit warning if long long type is used.
          '-Wno-long-long',
        ],
        'ldflags': [
          '-mx32',
        ],
      }],  # knet_target_arch=="x32"
      ['OS=="win"', {
        'defines': [
          'WIN32',
        ],
        # 4351: VS 2005 and later are warning us that they've fixed a bug
        #       present in VS 2003 and earlier.
        'msvs_disabled_warnings': [4351],
        'msvs_configuration_attributes': {
          'OutputDirectory': '<(DEPTH)\\build\\$(ConfigurationName)',
          'IntermediateDirectory': '$(OutDir)\\obj\\$(ProjectName)',
          'CharacterSet': '1',
        },
      }],
      ['OS=="win" and knet_target_arch=="ia32"', {
        'msvs_settings': {
          'VCCLCompilerTool': {
            # Ensure no surprising artifacts from 80bit double math with x86.
            'AdditionalOptions': ['/arch:SSE2'],
          },
        },
      }],
      ['(OS=="linux" or OS=="freebsd"  or OS=="openbsd" or OS=="solaris" \
         or OS=="netbsd" or OS=="mac" or OS=="android" or OS=="qnx") and \
        (knet_target_arch=="ia32")', {
        'target_conditions': [
          ['_toolset=="host"', {
            'conditions': [
              ['host_cxx_is_biarch==1', {
                'cflags': [ '-m32' ],
                'ldflags': [ '-m32' ]
              }],
            ],
            'xcode_settings': {
              'ARCHS': [ 'i386' ],
            },
          }],
          ['_toolset=="target"', {
            'conditions': [
              ['target_cxx_is_biarch==1 and nacl_target_arch!="nacl_x64"', {
                'cflags': [ '-m32' ],
                'ldflags': [ '-m32' ],
              }],
            ],
            'xcode_settings': {
              'ARCHS': [ 'i386' ],
            },
          }],
        ],
      }],
      ['(OS=="linux" or OS=="android") and \
        (knet_target_arch=="x64" or knet_target_arch=="arm64")', {
        'target_conditions': [
          ['_toolset=="host"', {
            'conditions': [
              ['host_cxx_is_biarch==1', {
                'cflags': [ '-m64' ],
                'ldflags': [ '-m64' ]
              }],
             ],
           }],
           ['_toolset=="target"', {
             'conditions': [
               ['target_cxx_is_biarch==1', {
                 'cflags': [ '-m64' ],
                 'ldflags': [ '-m64' ],
               }],
             ]
           }],
         ],
      }],
      ['OS=="linux" or OS=="freebsd" or OS=="openbsd" or OS=="solaris" \
         or OS=="netbsd" or OS=="qnx"', {
        'conditions': [
          [ 'v8_no_strict_aliasing==1', {
            'cflags': [ '-fno-strict-aliasing' ],
          }],
        ],  # conditions
      }],
      ['OS=="solaris"', {
        'defines': [ '__C99FEATURES__=1' ],  # isinf() etc.
      }],
      ['OS=="freebsd" or OS=="openbsd"', {
        'cflags': [ '-I/usr/local/include' ],
      }],
      ['OS=="netbsd"', {
        'cflags': [ '-I/usr/pkg/include' ],
      }],
    ],  # conditions
    'configurations': {
      # Abstract configuration for knet_optimized_debug == 0.
      'DebugBase0': {
        'abstract': 1,
        'msvs_settings': {
          'VCCLCompilerTool': {
            'Optimization': '0',
            'conditions': [
              ['component=="shared_library"', {
                'RuntimeLibrary': '3',  # /MDd
              }, {
                'RuntimeLibrary': '1',  # /MTd
              }],
            ],
          },
          'VCLinkerTool': {
            'LinkIncremental': '2',
          },
        },
        'conditions': [
          ['OS=="linux" or OS=="freebsd" or OS=="openbsd" or OS=="netbsd" or \
            OS=="qnx"', {
            'cflags!': [
              '-O0',
              '-O3',
              '-O2',
              '-O1',
              '-Os',
            ],
            'cflags': [
              '-fdata-sections',
              '-ffunction-sections',
            ],
          }],
          ['OS=="mac"', {
            'xcode_settings': {
               'GCC_OPTIMIZATION_LEVEL': '0',  # -O0
            },
          }],
        ],
      },  # DebugBase0
      # Abstract configuration for knet_optimized_debug == 1.
      'DebugBase1': {
        'abstract': 1,
        'msvs_settings': {
          'VCCLCompilerTool': {
            'Optimization': '1',
            'InlineFunctionExpansion': '2',
            'EnableIntrinsicFunctions': 'true',
            'FavorSizeOrSpeed': '0',
            'StringPooling': 'true',
            'BasicRuntimeChecks': '0',
            'conditions': [
              ['component=="shared_library"', {
                'RuntimeLibrary': '3',  # /MDd
              }, {
                'RuntimeLibrary': '1',  # /MTd
              }],
            ],
          },
          'VCLinkerTool': {
            'LinkIncremental': '2',
          },
        },
        'conditions': [
          ['OS=="linux" or OS=="freebsd" or OS=="openbsd" or OS=="netbsd" or \
            OS=="qnx"', {
            'cflags!': [
              '-O0',
              '-O3', # TODO(2807) should be -O1.
              '-O2',
              '-Os',
            ],
            'cflags': [
              '-fdata-sections',
              '-ffunction-sections',
              '-O1', # TODO(2807) should be -O3.
            ],
            'conditions': [
              ['gcc_version==44 and clang==0', {
                'cflags': [
                  # Avoid crashes with gcc 4.4 in the v8 test suite.
                  '-fno-tree-vrp',
                ],
              }],
            ],
          }],
          ['OS=="mac"', {
            'xcode_settings': {
               'GCC_OPTIMIZATION_LEVEL': '3',  # -O3
               'GCC_STRICT_ALIASING': 'YES',
            },
          }],
        ],
      },  # DebugBase1
      # Abstract configuration for knet_optimized_debug == 2.
      'DebugBase2': {
        'abstract': 1,
        'msvs_settings': {
          'VCCLCompilerTool': {
            'Optimization': '2',
            'InlineFunctionExpansion': '2',
            'EnableIntrinsicFunctions': 'true',
            'FavorSizeOrSpeed': '0',
            'StringPooling': 'true',
            'BasicRuntimeChecks': '0',
            'conditions': [
              ['component=="shared_library"', {
                'RuntimeLibrary': '3',  #/MDd
              }, {
                'RuntimeLibrary': '1',  #/MTd
              }],
              ['knet_target_arch=="x64"', {
                # TODO(2207): remove this option once the bug is fixed.
                'WholeProgramOptimization': 'true',
              }],
            ],
          },
          'VCLinkerTool': {
            'LinkIncremental': '1',
            'OptimizeReferences': '2',
            'EnableCOMDATFolding': '2',
          },
        },
        'conditions': [
          ['OS=="linux" or OS=="freebsd" or OS=="openbsd" or OS=="netbsd" or \
            OS=="qnx"', {
            'cflags!': [
              '-O0',
              '-O1',
              '-Os',
            ],
            'cflags': [
              '-fdata-sections',
              '-ffunction-sections',
            ],
            'defines': [
              'OPTIMIZED_DEBUG'
            ],
            'conditions': [
              # TODO(crbug.com/272548): Avoid -O3 in NaCl
              ['nacl_target_arch=="none"', {
                'cflags': ['-O3'],
                'cflags!': ['-O2'],
                }, {
                'cflags': ['-O2'],
                'cflags!': ['-O3'],
              }],
              ['gcc_version==44 and clang==0', {
                'cflags': [
                  # Avoid crashes with gcc 4.4 in the v8 test suite.
                  '-fno-tree-vrp',
                ],
              }],
            ],
          }],
          ['OS=="mac"', {
            'xcode_settings': {
              'GCC_OPTIMIZATION_LEVEL': '3',  # -O3
              'GCC_STRICT_ALIASING': 'YES',
            },
          }],
        ],
      },  # DebugBase2
      # Common settings for the Debug configuration.
      'DebugBaseCommon': {
        'abstract': 1,
        'defines': [
          'ENABLE_DISASSEMBLER',
          'V8_ENABLE_CHECKS',
          'OBJECT_PRINT',
          'VERIFY_HEAP',
          'DEBUG'
        ],
        'conditions': [
          ['OS=="linux" or OS=="freebsd" or OS=="openbsd" or OS=="netbsd" or \
            OS=="qnx"', {
            'cflags': [ '-Woverloaded-virtual', '<(wno_array_bounds)', ],
          }],
          ['OS=="linux" and v8_enable_backtrace==1', {
            # Support for backtrace_symbols.
            'ldflags': [ '-rdynamic' ],
          }],
          ['OS=="android"', {
            'variables': {
              'android_full_debug%': 1,
            },
            'conditions': [
              ['android_full_debug==0', {
                # Disable full debug if we want a faster v8 in a debug build.
                # TODO(2304): pass DISABLE_DEBUG_ASSERT instead of hiding DEBUG.
                'defines!': [
                  'DEBUG',
                ],
              }],
            ],
          }],
        ],
      },  # DebugBaseCommon
      'Debug': {
        'inherit_from': ['DebugBaseCommon'],
        'conditions': [
          ['knet_optimized_debug==0', {
            'inherit_from': ['DebugBase0'],
          }],
          ['knet_optimized_debug==1', {
            'inherit_from': ['DebugBase1'],
          }],
          ['knet_optimized_debug==2', {
            'inherit_from': ['DebugBase2'],
          }],
        ],
      },  # Debug
      'Release': {
        'conditions': [
          ['OS=="linux" or OS=="freebsd" or OS=="openbsd" or OS=="netbsd"', {
            'cflags!': [
              '-Os',
            ],
            'cflags': [
              '-fdata-sections',
              '-ffunction-sections',
              '<(wno_array_bounds)',
            ],
            'conditions': [
              [ 'gcc_version==44 and clang==0', {
                'cflags': [
                  # Avoid crashes with gcc 4.4 in the v8 test suite.
                  '-fno-tree-vrp',
                ],
              }],
              # TODO(crbug.com/272548): Avoid -O3 in NaCl
              ['nacl_target_arch=="none"', {
                'cflags': ['-O3'],
                'cflags!': ['-O2'],
              }, {
                'cflags': ['-O2'],
                'cflags!': ['-O3'],
              }],
            ],
          }],
          ['OS=="android"', {
            'cflags!': [
              '-O3',
              '-Os',
            ],
            'cflags': [
              '-fdata-sections',
              '-ffunction-sections',
              '-O2',
            ],
            'conditions': [
              [ 'gcc_version==44 and clang==0', {
                'cflags': [
                  # Avoid crashes with gcc 4.4 in the v8 test suite.
                  '-fno-tree-vrp',
                ],
              }],
            ],
          }],
          ['OS=="mac"', {
            'xcode_settings': {
              'GCC_OPTIMIZATION_LEVEL': '3',  # -O3

              # -fstrict-aliasing.  Mainline gcc
              # enables this at -O2 and above,
              # but Apple gcc does not unless it
              # is specified explicitly.
              'GCC_STRICT_ALIASING': 'YES',
            },
          }],  # OS=="mac"
          ['OS=="win"', {
            'msvs_settings': {
              'VCCLCompilerTool': {
                'Optimization': '2',
                'InlineFunctionExpansion': '2',
                'EnableIntrinsicFunctions': 'true',
                'FavorSizeOrSpeed': '0',
                'StringPooling': 'true',
                'conditions': [
                  ['component=="shared_library"', {
                    'RuntimeLibrary': '2',  #/MD
                  }, {
                    'RuntimeLibrary': '0',  #/MT
                  }],
                  ['knet_target_arch=="x64"', {
                    # TODO(2207): remove this option once the bug is fixed.
                    'WholeProgramOptimization': 'true',
                  }],
                ],
              },
              'VCLinkerTool': {
                'LinkIncremental': '1',
                'OptimizeReferences': '2',
                'EnableCOMDATFolding': '2',
              },
            },
          }],  # OS=="win"
        ],  # conditions
      },  # Release
    },  # configurations
  },  # target_defaults
}

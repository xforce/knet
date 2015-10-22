{
	'variables' : {
		'knet_target_type%': 'static_library',
		'deps_path%': '/deps',
	},
	'targets': [
		{
			'target_name': 'knet',
			'type': '<(knet_target_type)',
			'defines': [
				'NOMINMAX',
			],
			'include_dirs': [
				'include',
			],
			'sources': [
				#Source files
				'src/peer.cpp',
				'src/reliability_layer.cpp',
				'src/sockets/berkley_socket.cpp',

				#Header Files, for VS

				# Public API
				'include/network_helper_types.h',
			],
			'conditions': [
				['OS=="win"', {
					'defines': [
						'_WINSOCK_DEPRECATED_NO_WARNINGS',
					],
					'link_settings':  {
						'libraries': [ '-lwinmm.lib', '-lws2_32.lib' ],
					},
					'msvs_settings': {
						'VCLinkerTool': {
							'ImageHasSafeExceptionHandlers': 'false',
						},
					},
				}],
			],
		},
		{
			'target_name': 'knet_tests',
			'type': 'executable',
			'dependencies': [
				'<!@pymod_do_main(make_rel_path "<(deps_path)/gtest/gtest.gyp"):gtest',
				'knet',
			],
			'include_dirs': [
				'include',
				'<(deps_path)/gtest',
				'<(deps_path)/gtest/include',
			],
			'sources': [
				'test/test.cpp',
				'test/test_bitstream.cpp',
				'test/test_connect.cpp',
			],
			'conditions': [
				['OS=="win"', {
					'defines': [
						'_WINSOCK_DEPRECATED_NO_WARNINGS',
						'WIN32',
						'NOMINMAX',
					],
				}],
                ['OS!="win"', {

                'link_settings': {
                    'libraries': [
                        '-lpthread'
                    ]
                },
              }],
			],
			'VCLinkerTool': {
            	'ImageHasSafeExceptionHandlers': 'false',
            	'LinkIncremental': '2',
          	},
		},
	]
}

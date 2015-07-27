{
	'variables' : {
		#'logger_enabled': '<!pymod_do_main(find_logger)',
		'kframework_target_type%': 'static_library',
	},
	'targets': [
		{
			'target_name': 'kNet',
			'type': '<(kframework_target_type)',
			'defines': [
				'NOMINMAX',
			],
			'include_dirs': [
				'include',
			],
			'sources': [
				#Source files
				'src/kNet.cpp',
				'src/Peer.cpp',
				'src/ReliabilityLayer.cpp',
				'src/sockets/BerkleySocket.cpp',
				
				#Header Files, for VS
				'include/BitStream.h',

				'include/Peer.h',
				'include/ReliabilityLayer.h',
				
				# Internal Header files,
				# mostly used internally, but can be if necessary accessed from outside
				# for example when implementing plugins
				'include/internal/DatagramHeader.h',
				'include/internal/DatagramPacket.h',
				'include/internal/EventHandler.h',
				'include/internal/function_traits.h',
				'include/internal/ReliablePacket.h',
				
				# All the stuff required for the sockets
				'include/sockets/BerkleySocket.h',
				'include/sockets/ISocket.h',
				'include/sockets/SocketAddress.h',
			],
			'conditions': [
				['OS=="win"', {
					'defines': [
						'_WINSOCK_DEPRECATED_NO_WARNINGS',
						'WIN32',
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
			'target_name': 'kNet_tests',
			'type': 'executable',
			'dependencies': [
				'deps/gtest/gtest.gyp:*',
				'kNet',
			],
			'include_dirs': [
				'include',
				'deps/gtest',
				'deps/gtest/include',
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
					],
				}],
			],
			'VCLinkerTool': {
            	'ImageHasSafeExceptionHandlers': 'false',
            	'LinkIncremental': '2',
          	},
		},
	]
}

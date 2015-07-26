{
	'variables' : {
		#'logger_enabled': '<!pymod_do_main(find_logger)',
		'kframework_target_type%': 'executable',
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
				'include/Common.h',
				'include/DatagramHeader.h',
				'include/DatagramPacket.h',
				'include/EventHandler.h',
				'include/function_traits.h',
				'include/Peer.h',
				'include/ReliabilityLayer.h',
				'include/ReliablePacket.h',
				'include/sockets/BerkleySocket.h',
				'include/sockets/ISocket.h',
				'include/sockets/SocketAddress.h',
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
		},
	]
}

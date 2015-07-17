{
	'variables' : {
		#'logger_enabled': '<!pymod_do_main(find_logger)',
		'knet_target_type%': 'static_library',
	},
	'targets': [
		{
			'target_name': 'kNet',
			'type': '<(knet_target_type)',
			'defines': [
				'KNET_EXPORTS',
				'NOMINMAX',
			],
			'include_dirs': [
				'include',
			],
			'sources': [
				'src/kNet.cpp',
				'src/Peer.cpp',
				'src/ReliabilityLayer.cpp',
				'src/Sockets/BerkleySocket.cpp',
				
				#Header Files, for VS
				'include/BitStream.h',
				'include/Common.h',
				'include/DatagramHeader.h',
				'include/DatagramPacket.h',
				'include/EventHandler.h',
				'include/Peer.h',
				'include/ReliabilityLayer.h',
				'include/ReliablePacket.h',
			],
			'conditions': [
				['OS=="win"', {
					'defines': [
						'_RAKNET_DLL',
						'_WINSOCK_DEPRECATED_NO_WARNINGS',
					],
					'link_settings':  {
						'libraries': [ '-lwinmm.lib', '-lws2_32.lib' ],
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

{
	'variables' : {
		'logger_enabled': '<!pymod_do_main(find_logger)',
	},
	'targets': [
		{
			'target_name': 'kNet',
			'type': 'executable',
			'conditions': [
				['logger_enabled=="true"', {
					'defines': [
						'LOGGER_STATIC',
					],
					'dependencies': [
						'vendor/kLogger/kLogger.gyp:kLogger',
					],
					'include_dirs': [
						'vendor/kLogger/include',
					],
				}],
				['OS=="win"', {
					'defines': [
						'WIN32_LEAN_AND_MEAN',
						'_WINSOCK_DEPRECATED_NO_WARNINGS',
					],
					'link_settings':  {
						'libraries': [ '-lwinmm.lib', '-lws2_32.lib' ],
					},
				}],
			],
			'defines': [
				'KNET_EXPORTS',
				'NOMINMAX',
			],
			'include_dirs': [
				'../include',
			],
			'sources': [
				'<!@(find ../src -name *.cpp)',
				'<!@(find ../include -name *.h)',
			],
		},
	]
}

{
  'target_defaults': {
    'variables': {
      'deps': [
        'libbrillo-<(libbase_ver)',
        'libchrome-<(libbase_ver)',
      ],
    },
  },
  'targets': [
    {
      'target_name': 'biod',
      'type': 'executable',
      'link_settings': {
        'libraries': [
          '-ldl',
        ],
      },
      'sources': [
        'bio_library.cc',
        'biod_storage.cc',
        'biometrics_daemon.cc',
        'fake_biometrics_manager.cc',
        'fpc_biometrics_manager.cc',
        'main.cc',
        'scoped_umask.cc',
      ],
    },
    {
      'target_name': 'biod_client_tool',
      'type': 'executable',
      'sources': ['tools/biod_client_tool.cc'],
    },
    {
      'target_name': 'fake_biometric_tool',
      'type': 'executable',
      'sources': ['tools/fake_biometric_tool.cc'],
    },
  ],
}

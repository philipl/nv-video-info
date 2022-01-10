project('nv-video-info',
        'c',
        version : '1.0',
        license: 'LGPL2.1+',
        default_options : [
            'c_std=c11',
            'warning_level=0'
        ]
)

cc = meson.get_compiler('c')
cc.find_library('dl')

ffnvcodec = dependency('ffnvcodec', version: '>= 9.1.23.0')

executable('nvdecinfo', ['nvdecinfo.c'], dependencies: [ffnvcodec], install: true)
executable('nvencinfo', ['nvencinfo.c'], dependencies: [ffnvcodec], install: true)

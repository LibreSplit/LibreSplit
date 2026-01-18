project(
    'libresplit',
    'c',
    default_options: ['c_std=gnu23'],
    meson_version: '>=1.1',
)

if get_option('buildtype') == 'debug'
    add_project_arguments('-DDEBUG', language: 'c')
endif

cc = meson.get_compiler('c')
has_procmap_ioctl = cc.has_header_symbol('linux/fs.h', 'PROCMAP_QUERY')
ioctl_maps = get_option('ioctl_maps').enable_auto_if(has_procmap_ioctl)
if ioctl_maps.enabled()
    add_project_arguments('-DIOCTL_MAPS', language: 'c')
endif

threads = dependency('threads')
gtk = dependency('gtk+-3.0')
luajit = dependency('luajit')
x11 = dependency('x11')
jansson = dependency('jansson')

executable(
    'libresplit',
    [
        'src/main.c',
        'src/bind.c',
        'src/server.c',
        'src/shared.c',
        'src/timer.c',

        # Settings
        'src/settings/definitions.c',
        'src/settings/settings.c',
        'src/settings/utils.c',

        # LASR
        'src/lasr/auto-splitter.c',
        'src/lasr/utils.c',
        'src/lasr/maps/maps.c',
        'src/lasr/functions/bitwise.c',
        'src/lasr/functions/getBaseAddress.c',
        'src/lasr/functions/getModuleSize.c',
        'src/lasr/functions/getPID.c',
        'src/lasr/functions/getMaps.c',
        'src/lasr/functions/print_tbl.c',
        'src/lasr/functions/process.c',
        'src/lasr/functions/readAddress.c',
        'src/lasr/functions/shallow_copy_tbl.c',
        'src/lasr/functions/signature.c',
        'src/lasr/functions/sizeOf.c',

        # Components
        'src/component/best-sum.c',
        'src/component/clock.c',
        'src/component/components.c',
        'src/component/pb.c',
        'src/component/prev-segment.c',
        'src/component/splits.c',
        'src/component/title.c',
        'src/component/wr.c',
        'src/component/detailed-timer.c',
    ],
    dependencies: [threads, gtk, luajit, x11, jansson],
    c_args: [
        '-DPREFIX="' + get_option('prefix') + '"',
        '-DDATADIR="' + get_option('datadir') + '"',
        '-Werror',
        '-Wall',
        '-Wbad-function-cast',
        '-Wsign-compare',
        '-Wpedantic',
    ],
    install: true,
)

executable(
    'libresplit-ctl',
    'src/ctl.c',
    'src/shared.c',
    install: true,
)

message('prefix: ' + get_option('prefix')) # /usr/local by default
message('datadir: ' + get_option('datadir')) # share by default
message('buildtype: ' + get_option('buildtype'))
message('ioctl_maps: ' + ioctl_maps.enabled().to_string())

install_data(
    'assets/libresplit.desktop',
    install_dir: get_option('prefix') / get_option('datadir') / 'applications',
)
install_data(
    'README.md',
    install_dir: get_option('prefix') / get_option('datadir') / 'doc' / meson.project_name(),
)
install_data(
    'LICENSE',
    install_dir: get_option('prefix') / get_option('datadir') / 'licenses' / meson.project_name(),
)

# TODO: better install dir for this to make desktop and gtk icons work on debug builds/uninstalled tests
foreach size : [16, 22, 24, 32, 36, 48, 64, 72, 96, 128, 256, 512, 1024]
    install_data(
        'assets/icons/libresplit-@0@.png'.format(size),
        install_dir: get_option('prefix') / get_option('datadir') / 'icons/hicolor/@0@x@0@/apps/'.format(size),
        rename: 'libresplit.png',
        install_mode: 'rw-r--r--',
    )
endforeach

gnome = import('gnome')
#gnome.post_install(update_desktop_database: true, gtk_update_icon_cache: true)
gnome.post_install(gtk_update_icon_cache: true)

project(
    'libresplit',
    'c',
    version: 'pre-release',
    default_options: ['c_std=gnu23'],
    meson_version: '>=1.1',
)

add_project_arguments('-DAPP_VERSION="@0@"'.format(meson.project_version()), language: 'c')

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

libresplit_sources = files(
    'src/main.c',
    'src/keybinds/bind.c',
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

    # Keybinds
    'src/keybinds/keybinds.c',
    'src/keybinds/keybinds_callbacks.c',

    # GUI
    'src/gui/utils.c',
    'src/gui/welcome_box.c',
    'src/gui/dialogs.c',
    'src/gui/theming.c',
    'src/gui/timer.c',
    'src/gui/game.c',
    'src/gui/app_window.c',
    'src/gui/help_dialog.c',
    'src/gui/settings_dialog.c',

    # Components
    'src/gui/component/best-sum.c',
    'src/gui/component/clock.c',
    'src/gui/component/components.c',
    'src/gui/component/pb.c',
    'src/gui/component/prev-segment.c',
    'src/gui/component/splits.c',
    'src/gui/component/title.c',
    'src/gui/component/wr.c',
    'src/gui/component/detailed-timer.c',
)

libresplit_ctl_sources = files(
    'src/ctl.c',
    'src/shared.c',
)

ld = find_program('ld', required: true)
css_o = custom_target(
    'fallback-css-o',
    input: 'src/fallback.css',
    output: 'fallback.o',
    command: [ld, '-r', '-b', 'binary', '-o', '@OUTPUT@', '@INPUT@'],
    build_by_default: true,
)

executable(
    'libresplit',
    libresplit_sources + css_o,
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
    libresplit_ctl_sources,
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
install_data(
    'assets/libresplit.svg',
    install_dir: get_option('prefix') / get_option('datadir') / 'icons/hicolor/scalable/apps/',
    rename: 'libresplit.png',
    install_mode: 'rw-r--r--',
)

# Code formatting test
clang_format = find_program('clang-format', required: false)
if clang_format.found()
    # Check LibreSplit
    test(
        'clang-format-libresplit',
        clang_format,
        args: [
            '--dry-run',
            '--Werror',
            libresplit_sources,
        ],
        suite: 'format',
    )
    # Check LibreSplitCTL
    test(
        'clang-format-ctl',
        clang_format,
        args: [
            '--dry-run',
            '--Werror',
            libresplit_ctl_sources,
        ],
        suite: 'format',
    )
else
    message('clang-format not found, skipping formatting test')
endif

# Static analysis + Linting
cppcheck = find_program('cppcheck', required: false)
if cppcheck.found()
    # Set args for analysing both sources.
    cppcheck_base_args = [
        '--std=c23',
        '--language=c',
        '--enable=warning,performance',
        '--error-exitcode=1',
        # Recommended silences for GTK/GLib apps
        '--library=gtk.cfg',
        '--suppress=missingIncludeSystem',
        '--suppress=unknownMacro',
        '--suppress=preprocessorErrorDirective',
        '--suppress=unusedParameter',
    ]
    # Check LibreSplit
    test(
        'cppcheck-libresplit',
        cppcheck,
        args: cppcheck_base_args + libresplit_sources,
        suite: 'lint',
    )
    # Check LibreSplitCTL
    test(
        'cppcheck-ctl',
        cppcheck,
        args: cppcheck_base_args + libresplit_ctl_sources,
        suite: 'lint',
    )
else
    message('cppcheck not found, skipping linting test')
endif

gnome = import('gnome')
#gnome.post_install(update_desktop_database: true, gtk_update_icon_cache: true)
gnome.post_install(gtk_update_icon_cache: true)

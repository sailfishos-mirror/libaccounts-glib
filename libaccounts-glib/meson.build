public_headers = files(
    'libaccounts-glib.h',
    'ag-types.h',
    'ag-account.h',
    'ag-account-service.h',
    'ag-application.h',
    'ag-auth-data.h',
    'ag-autocleanups.h',
    'ag-errors.h',
    'ag-manager.h',
    'ag-provider.h',
    'ag-service.h',
    'ag-service-type.h'
)

private_headers = files(
    'ag-debug.h',
    'ag-internals.h',
    'ag-util.h'
)

c_files = files(
    'ag-account.c',
    'ag-account-service.c',
    'ag-application.c',
    'ag-auth-data.c',
    'ag-debug.c',
    'ag-manager.c',
    'ag-provider.c',
    'ag-service.c',
    'ag-service-type.c',
    'ag-util.c'
)

marshal_files = gnome.genmarshal('ag-marshal',
    sources: 'ag-marshal.list',
    prefix: 'ag_marshal'
)

private_headers += marshal_files[1]
c_files += marshal_files[0]

src_dir = include_directories('.')

ag_library = library('accounts-glib',
    public_headers + private_headers + c_files,
    c_args: [
        '-DG_LOG_DOMAIN="accounts-glib"',
        '-DACCOUNTS_GLIB_COMPILATION=1',
    ],
    version: meson.project_version(),
    soversion: so_version,
    dependencies: accounts_glib_library_deps,
    include_directories: root_dir,
    install: true
)

install_headers(public_headers + files('accounts-glib.h'),
    subdir: 'libaccounts-glib'
)

# PkgConfig creation
pkg.generate(
    ag_library,
    filebase: 'libaccounts-glib',
    name: 'libaccounts-glib',
    description: 'A low-level library for managing accounts settings.',
    version: meson.project_version(),
    requires: [glib_dep, gio_dep, gio_unix_dep, gobject_dep],
    subdirs: ['', 'libaccounts-glib'],
    variables: [
        'applicationfilesdir=${prefix}/share/' + application_files_dir,
        'servicefilesdir=${prefix}/share/' + service_files_dir,
        'servicetypefilesdir=${prefix}/share/' + service_type_files_dir,
        'providerfilesdir=${prefix}/share/' + provider_files_dir,
    ]
)

# Creation of the dependency to use it in tools and tests
accounts_glib_dep = declare_dependency(
    link_with: ag_library,
    dependencies: accounts_glib_library_deps,
    include_directories: [ include_directories('.'), root_dir ]
)

# GObject introspection
gir_targets = gnome.generate_gir(ag_library,
    sources: public_headers + c_files,
    nsversion: api_version,
    namespace: 'Accounts',
    identifier_prefix: 'Ag',
    symbol_prefix: 'ag',
    includes: ['GObject-2.0', 'Gio-2.0'],
    dependencies: accounts_glib_library_deps,
    header: 'libaccounts-glib.h',
    extra_args: ['-DACCOUNTS_GLIB_COMPILATION=1'],
    install: true
)

# Vala .vapi generation
gnome.generate_vapi('libaccounts-glib',
    sources: gir_targets[0],
    packages: ['glib-2.0', 'gio-unix-2.0'],
    install: true
)

install_data('libaccounts-glib.deps',
    install_dir: join_paths(get_option('datadir'), 'vala', 'vapi')
)

install_data('com.google.code.AccountsSSO.Accounts.Manager.xml',
    install_dir: join_paths(get_option('datadir'), 'dbus-1', 'interfaces')
)

subdir('pygobject')


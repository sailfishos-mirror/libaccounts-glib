glib_prefix = dependency('glib-2.0').get_pkgconfig_variable('prefix')
glib_docpath = join_paths(glib_prefix, 'share', 'gtk-doc', 'html')
docpath = join_paths(get_option('prefix'), get_option('datadir'), 'gtk-doc', 'html')

doc_configuration = configuration_data()
doc_configuration.set('VERSION', full_version)

configure_file(input : 'version.xml.in',
    output : 'version.xml',
    configuration : doc_configuration
)

private_headers = [
    'ag-debug.h',
    'ag-internals.h',
    'ag-util.h'
]

gnome.gtkdoc('libaccounts-glib',
    main_xml: 'libaccounts-glib-docs.xml',
    src_dir: [join_paths(meson.source_root (), 'libaccounts-glib')],
    dependencies : [glib_dep, gobject_dep, accounts_glib_dep],
    content_files: [
        'ag-backup.xml',
        'ag-tool.xml',
        'application-file-format.xml',
        'gettext-xml-files.xml',
        'libaccounts-compiling.xml',
        'libaccounts-glossary.xml',
        'libaccounts-overview.xml',
        'libaccounts-running.xml',
        'provider-file-format.xml',
        'service-file-format.xml',
        'service-type-file-format.xml',
        'validating-xml-files.xml'
    ],
    fixxref_args: [
        '--html-dir=' + docpath,
        '--extra-dir=' + join_paths(glib_docpath, 'glib'),
        '--extra-dir=' + join_paths(glib_docpath, 'gobject'),
        '--extra-dir=' + join_paths(glib_docpath, 'gio')
    ],
    scan_args: ['--rebuild-types','--ignore-headers=' + ' '.join(private_headers)],
    install: true
)

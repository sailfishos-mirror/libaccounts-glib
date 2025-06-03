project('libaccounts-glib', 'c', version: '1.27', license: 'LGPL2.1', meson_version: '>=0.56.0')

version_parts = meson.project_version().split('.')
api_version = '@0@.0'.format(version_parts[0])
so_version = '0'

gnome = import('gnome')
pkg = import('pkgconfig')

glib_minimal_version = '2.44'
glib_dep = dependency('glib-2.0', version : '>=' + glib_minimal_version)
gio_dep = dependency('gio-2.0', version : '>=' + glib_minimal_version)
gio_unix_dep = dependency('gio-unix-2.0', version : '>=' + glib_minimal_version)
gobject_dep = dependency('gobject-2.0', version : '>=' + glib_minimal_version)
libxml_dep = dependency('libxml-2.0')
sqlite_dep = dependency('sqlite3', version : '>=3.7.0')

glib_minimal_version_parts = glib_minimal_version.split('.')
glib_major_req = glib_minimal_version_parts[0]
glib_minor_req = glib_minimal_version_parts[1]

application_files_dir = 'accounts/applications'
provider_files_dir = 'accounts/providers'
service_files_dir = 'accounts/services'
service_type_files_dir = 'accounts/service_types'
database_dir = 'libaccounts-glib'

add_project_arguments(
    '-DGLIB_VERSION_MIN_REQUIRED=GLIB_VERSION_@0@_@1@'.format(glib_major_req, glib_minor_req),
    '-DGLIB_VERSION_MAX_ALLOWED=GLIB_VERSION_@0@_@1@'.format(glib_major_req, glib_minor_req),
    '-DAPPLICATION_FILES_DIR="'+ application_files_dir + '"',
    '-DPROVIDER_FILES_DIR="'+ provider_files_dir + '"',
    '-DSERVICE_FILES_DIR="'+ service_files_dir + '"',
    '-DSERVICE_TYPE_FILES_DIR="'+ service_type_files_dir + '"',
    '-DDATABASE_DIR="'+ database_dir + '"',
language : 'c')

accounts_glib_library_deps = [glib_dep, gio_dep, gio_unix_dep, gobject_dep, libxml_dep, sqlite_dep]

xmllint = find_program('xmllint', required: false)

root_dir = include_directories('.')

subdir('libaccounts-glib')
subdir('tools')
subdir('data')
if get_option('docs')
  subdir('docs')
endif
if get_option('tests')
  subdir('tests')
endif

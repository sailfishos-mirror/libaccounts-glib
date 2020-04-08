project('libaccounts-glib', 'c', version: '1.24', license: 'LGPL2.1', meson_version: '>=0.48.0')

version_parts = meson.project_version().split('.')
api_version = '@0@.0'.format(version_parts[0])
so_version = '0'

gnome = import('gnome')
pkg = import('pkgconfig')

glib_minimal_version = '2.26'
glib_dep = dependency('glib-2.0', version : '>=' + glib_minimal_version)
gio_dep = dependency('gio-2.0', version : '>=' + glib_minimal_version)
gio_unix_dep = dependency('gio-unix-2.0', version : '>=' + glib_minimal_version)
gobject_dep = dependency('gobject-2.0', version : '>=' + glib_minimal_version)
libxml_dep = dependency('libxml-2.0')
sqlite_dep = dependency('sqlite3', version : '>=3.7.0')

application_files_dir = 'accounts/applications'
provider_files_dir = 'accounts/providers'
service_files_dir = 'accounts/services'
service_type_files_dir = 'accounts/service_types'
database_dir = 'libaccounts-glib'

add_project_arguments('-DAPPLICATION_FILES_DIR="'+ application_files_dir + '"', language : 'c')
add_project_arguments('-DPROVIDER_FILES_DIR="'+ provider_files_dir + '"', language : 'c')
add_project_arguments('-DSERVICE_FILES_DIR="'+ service_files_dir + '"', language : 'c')
add_project_arguments('-DSERVICE_TYPE_FILES_DIR="'+ service_type_files_dir + '"', language : 'c')
add_project_arguments('-DDATABASE_DIR="'+ database_dir + '"', language : 'c')

accounts_glib_library_deps = [glib_dep, gio_dep, gio_unix_dep, gobject_dep, libxml_dep, sqlite_dep]

xmllint = find_program('xmllint', required: false)

root_dir = include_directories('.')

subdir('libaccounts-glib')
subdir('tools')
subdir('data')
subdir('docs')
subdir('tests')

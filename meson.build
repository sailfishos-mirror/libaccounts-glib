project('libaccounts-glib', 'c')

version_major = 1
version_minor = 23
full_version = '@0@.@1@'.format(version_major, version_minor)
api_version = '@0@.0'.format(version_major)

gnome = import('gnome')

glib_dep = dependency('glib-2.0', version : '>=2.26')
gio_dep = dependency('gio-2.0', version : '>=2.26')
gio_unix_dep = dependency('gio-unix-2.0')
gobject_dep = dependency('gobject-2.0', version : '>=2.35.1')
libxml_dep = dependency('libxml-2.0')
sqlite_dep = dependency('sqlite3', version : '>=3.7.0')

application_files_dir = 'accounts/applications'
provider_files_dir = 'accounts/providers'
service_files_dir = 'accounts/services'
service_type_files_dir = 'accounts/service_types'
database_dir = 'libaccounts-glib'

add_global_arguments('-DAPPLICATION_FILES_DIR="'+ application_files_dir + '"', language : 'c')
add_global_arguments('-DPROVIDER_FILES_DIR="'+ provider_files_dir + '"', language : 'c')
add_global_arguments('-DSERVICE_FILES_DIR="'+ service_files_dir + '"', language : 'c')
add_global_arguments('-DSERVICE_TYPE_FILES_DIR="'+ service_type_files_dir + '"', language : 'c')
add_global_arguments('-DDATABASE_DIR="'+ database_dir + '"', language : 'c')

accounts_glib_library_deps = [glib_dep, gio_dep, gio_unix_dep, gobject_dep, libxml_dep, sqlite_dep]

xmllint = find_program('xmllint', required: false)

root_dir = include_directories('.')

subdir('libaccounts-glib')
subdir('tools')
subdir('data')
subdir('docs')
subdir('tests')

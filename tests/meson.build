check_dep = dependency('check')

test_manager = gnome.gdbus_codegen('test-manager',
    join_paths(meson.source_root (), 'libaccounts-glib', 'com.google.code.AccountsSSO.Accounts.Manager.xml'),
    namespace: 'Test',
    interface_prefix: 'com.google.code.AccountsSSO.Accounts.',
    object_manager: true
)

test_process = executable('test-process',
    'test-process.c',
    dependencies: accounts_glib_dep
)

accounts_glib_testsuite = executable('accounts-glib-testsuite',
    'check_ag.c',
    test_manager,
    include_directories: root_dir,
    dependencies: [accounts_glib_dep, check_dep]
)

test_data_dir = join_paths (meson.current_source_dir (), 'data')
test_environment = environment()
test_environment.set ('AG_APPLICATIONS', join_paths (test_data_dir, 'accounts', 'applications'))
test_environment.set ('AG_SERVICES', join_paths (test_data_dir, 'accounts', 'services'))
test_environment.set ('AG_SERVICE_TYPES', join_paths (test_data_dir, 'accounts', 'service-types'))
test_environment.set ('AG_PROVIDERS', join_paths (test_data_dir, 'accounts', 'providers'))
test_environment.set ('ACCOUNTS', '/tmp')
test_environment.set ('AG_DEBUG', 'all')
test_environment.set ('G_MESSAGES_DEBUG', 'all')
test_environment.set ('G_DEBUG', 'fatal-criticals')
test_environment.set ('G_SLICE', 'always-malloc')
test_environment.set ('XDG_DATA_HOME', test_data_dir)
test_environment.append ('PATH', meson.current_build_dir ())


dbus_test_runner = find_program('dbus-test-runner', required: false)
if dbus_test_runner.found()
    test('accounts-glib-testsuite',
        dbus_test_runner,
        args: ['-m','180','-t',join_paths (meson.current_build_dir (), 'accounts-glib-testsuite')],
        env: test_environment,
        timeout: 180
    )
else
    test('accounts-glib-testsuite',
        accounts_glib_testsuite,
        env: test_environment,
        timeout: 180
    )
endif

if xmllint.found ()
    xml_files = [
        ['accounts-application.dtd', 'applications', ['Gallery.application','Mailer.application']],
        ['accounts-service.dtd', 'services', ['MyService.service','MyService2.service','OtherService.service']],
        ['accounts-provider.dtd', 'providers', ['MyProvider.provider']],
        ['accounts-service-type.dtd', 'service-types', ['e-mail.service-type']]
    ]
    foreach xml_file : xml_files
        dtd_path = join_paths (meson.source_root(), 'data', xml_file[0])
        foreach target_file : xml_file[2]
            xml_path = join_paths (meson.current_source_dir(), 'data', 'accounts', xml_file[1], target_file)
            test('xmllint-@0@-@1@'.format(xml_file[1], target_file),
                xmllint,
                args: ['--noout', '--dtdvalid', dtd_path, xml_path]
            )
        endforeach
    endforeach
endif

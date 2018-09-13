dtd_files = [
    'accounts-application.dtd',
    'accounts-provider.dtd',
    'accounts-service.dtd',
    'accounts-service-type.dtd'
]

its_files = [
    'accounts-application.loc',
    'accounts-application.its',
    'accounts-provider.loc',
    'accounts-provider.its',
    'accounts-service.loc',
    'accounts-service.its',
    'accounts-service-type.loc',
    'accounts-service-type.its'
]

install_data(dtd_files,
    install_dir: join_paths(get_option('datadir'), 'xml', 'accounts', 'schema', 'dtd')
)

install_data(its_files,
    install_dir: join_paths(get_option('datadir'), 'gettext', 'its')
)

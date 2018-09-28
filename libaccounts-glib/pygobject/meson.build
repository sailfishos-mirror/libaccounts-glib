python3 = import('python3')
python_exec = python3.find_python()
python_exec_result = run_command(python_exec, ['-c', 'import gi; from os.path import abspath; print(abspath(gi._overridesdir))'])

if python_exec_result.returncode() != 0
    error('Failed to retreive the python GObject override directory')
endif

install_data('Accounts.py',
    install_dir: join_paths(python_exec_result.stdout().strip())
)

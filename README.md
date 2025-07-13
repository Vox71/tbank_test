sudo apt update

sudo apt install -y clang llvm libbpf-dev libelf-dev

clang -o monitor monitor.c -lbpf -lelf

sudo ./monitor --block /etc/passwd --block /other/file

sudo ./monitor_open.bt

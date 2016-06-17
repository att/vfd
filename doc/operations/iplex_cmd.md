#Iplex Commandline Interface

Iplex is the command line tool which is used to communicate with VFd.
Commands such as add a VF, delete a VF, show status are supported.
The following describes the command line options and parameters for iplex.

```
python iplex.py --help
 iplex
        Usage:
        iplex {add | delete} <port-id> [--loglevel=<value>] [--debug]
        iplex show {<port-id>|all} [--loglevel=<value>] [--debug]
        iplex -h | --help
        iplex --version
        Options:
                -h, --help      show this help message and exit
                --version       show version and exit
                --debug         show debugging output
        --loglevel=<value>  Default logvalue [default: 0]python iplex.py --help
 iplex
        Usage:
        iplex {add | delete} <port-id> [--loglevel=<value>] [--debug]
        iplex show {<port-id>|all} [--loglevel=<value>] [--debug]
        iplex -h | --help
        iplex --version
        Options:
                -h, --help      show this help message and exit
                --version       show version and exit
                --debug         show debugging output
        --loglevel=<value>  Default logvalue [default: 0]
```

#VFd Main Configuration File


The main configuration file for VFd by default resides in /etc/vfd/vfd.cfg and 
is expected to contain a single json object with the following format.  


```
{   
    "log_dir":             "/var/log/vfd",
    "log_keep":            30,
    "log_level":           1,
    "init_log_level":      2,
    "config_dir":          "/var/lib/vfd/config",
    "fifo":                "/var/lib/vfd/request",
    "cpu_mask":            "0x00",
    "dpdk_log_level":      2,
    "dpdk_init_log_level": 8,
    "default_mtu":         9000,
    "pciids": [ 
        { 
          "id": "0000:07:00.0", 
          "mtu": 1500, 
          "enable_loopback": true 
         },
         { 
           "id":  "0000:07:00.1", 
           "mtu": 9000 
         }
    ]
}
```

## Main json fields

    `log_dir` 			is the directory where log files are to be placed
    `log_keep`			is the number of days that VFd log files are to be kept
    `log_level`			is the verbosity level for log messages after initialisation is complete
    `init_log_level`	is the verbosity level for log messages during initialisation
    `config_dir`		is the directory where configuration files are expected to be placed
    `fifo`				is the path of the fifo that VFd will create for iplex to use to make requests
    `cpu_mask`			is the default cpu mask which is used when doing EAL initialisation
    `dpdk_log_level`	is the verbose level requested of the DPDK library after initialisation
    `dpdk_init_log_level`	is the verbose level requested of the DPDK library during initialisation
    `default_mtu`			is the MTU value used for any pci defined with out a value
    `pciids` 			is an array of pci information 'objects'


### Pci object fields

	`id`				is the address of the pci device
	`mtu`				is the specific MTU which should be used (overrides default in main fields)
	`enable_loopback`	if true causes the bridging value to be set for the PF allowing VM to VM packet flow across the NIC



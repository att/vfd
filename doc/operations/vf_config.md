
#VF Configuration File Layout


A VF configuration file is expected to be written by the virtualisation manager (e.g. openstack)
into the config directory listed in the main VFd configuration file in the /etc/vdd directory.
The file is a single json 'object' which identifies the VF (by pci address and interface number) and
supplies the configuration information. 

The following is the expected syntax of the file:



```
{
    "name":             "name/uuid",
    "pciid":            "0000:07:00.1",
    "vfid":             0,
    "strip_stag":       {true|false},
    "allow_bcast":      {true|false},
    "allow_mcast":      {true|false},
    "allow_un_ucast":   {true|false},
    "link_status":      "{on|off|auto}",
    "stop_cb":          "/path/command parms",
    "start_cb":         "/path/command parms",
    "vlans":            [ 10, 11, 12, 33 ],
    "macs":             [ "aa:bb:cc:dd:ee:f0", "11:22:33:44:55:66", "ff:ff:ff:ff:ff:ff" ]
}
```

## Field Descriptions

`name`		A string which identifies the VF. VFd does not use the name; it is only for the convenience of the creator.

`pciid`		The address of the PF on which the VF is allocated

`vfid`		The id (0-31) of the VF that is being configured

`strip_stag` A boolean value that when true will cause the NIC to remove the VLAN ID from packets before they are given to the VF.  
	When true, this option implies that VLAN ID is inserted as packets are sent (there is no separate insert option).

`allow_bcast` A boolean value which when true allows broadcast messages to be received by the VF from the wire

`allow_mcast` A boolean value which when true allows multicast messages to be received by the VF from the wire

`allow_un_ucast` A boolean value which when true allows unicast messages to MACs not belonging to the VF to be received by the VF from the wire, i.e., promiscous mode

`link_status` Sets one of three possible modes: on (status always reported to the VF), off (status never reported), auto (NIC decides when to report)

`stop_cb`	The command line of a command that VFd will execute just prior to VFd shutdown

`start_cb`  The command line of a command that VFd will execute immediately following initialisation (before any new ipelx requests are processed)

`vlans`  An array of VLAN IDs which are used as a filter for the VF; only packets with these IDs will be passed by the NIC.  If the 
	list contains more than one value, then `strip_stag` *must* be false or VFd will toss and error and refuse to add the VF.

`macs`   An array of mac addresses which are used to filter outgoing packets to the wire. I.e., this acts as an anti-spoof filter. The MAC address of the VF is always an implied member. So, this array may be empty (e.g. []) to indicate the current MAC address of the VF is the only address which should be used as a filter.

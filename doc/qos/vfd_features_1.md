VFd 2.0 features
================

Dynamic update of VF parameters
-------------------------------

VFd will support a `iplex update <vf_conf_file>` command to allow in-place updating of VF parameters. VFd will determine if the changes can be made non-destructively, i.e., without reseting the VF, and if possible, make them transparently to the attached VNF. This feature will mainly be useful for changing mirroring and QoS parameters after initial deployment. 


Bandwidth QoS
-------------

The VFd bandwidth qos feature allows an administrator to configure the NIC with multiple traffic classes (4 to 8), henceforth named `tc0, tc1, ..., tc8` and to allocate priority and bandwidth to these classes. TC numbering is in priority order, with the higher number representing higher priority. The traffic classes are exposed to VFs in the form of multiple RX/TX queues - one per TC. Bandwidth assigned to each TC is shared across all the VF pools in the system. Additionally, each VF can be assigned a different min and max bandwidth within each TC.
 
TX pipeline: the TX pipeline will support priority and bandwidth allocation to each traffic class, and
bandwidth allocation to each VF pool using a combination of PF and VF configuration below. The TC of outgoing packets will be inferred from the TX queue the packet is sent to. *802.1q PCP markings will not be considered in the TX TC selection decision*. However, VFd will configure the NIC to insert the PCP values provided by the VNF into the outgoing frame. Therefore, it is the VNF's responsibility to ensure that the correct PCP marking (see RX per-PF configuration section below) is provided in the frame descriptor when during packet transmission.

RX pipeline: the RX pipeline will use a PCP --> TC mapping table (see per PF configuration below) to determine what TC to map each incoming packet to based on the 802.1q header PCP values marked on the packet. This mapping will be a system-wide administrative setting. The packet will be sent to the appropriate RX queue based on it's TC (there's one RX queue per VF for each TC). *The NIC shall not provide any prioritization or bandwidth control for RX traffic beyond putting packets on the right TC queues.* The VNF associated attached to the VF is responsible for reading packets in priority order in order to provide any RX prioritization/policing. 

### Tenant view from a VM

	                     +-----------------------------------+
                         |                VM                 |
						 |  VNF read RX Qs in priority order |
						 +---|   |---|   |---|   |---|   |---+
                             |   |   |   |   |   |   |   |
						      TC0     TC1     TC2     TC3
                         TX: min_bw  min_bw  min_bw  min_bw     | TX priority and bandwith
						     max_bw  max_bw  max_bw  max_bw     | based on TX Q pkt is sent to
                               ^     ^        ^      ^
                                \     \      /      /
							     \     \    /      /
							 Select RX TC queue based on PCP
                                          ^
                                          |
										  |
									  RX packet


### System view (TC class configuration)

       VF0_TC0  VF1_TC0 ...    VF0_TC1 VF1_TC1 ...     VF0_TC1 VF1_TC1 ...     VF0_TC1 VF1_TC1 ...
	+---| Q |---| Q |---+   +---| Q |---| Q |---+   +---| Q |---| Q |---+   +---| Q |---| Q |---+
        |   |   |   |           |   |   |   |           |   |   |   |           |   |   |   |
       max_bw   max_bw          max_bw  max_bw          max_bw  max_bw          max_bw  max_bw
             TC0                     TC1                     TC2                     TC3
           priority                priority                priority                priority
            min_bw                  min_bw                  min_bw                  min_bw


### Per-PF configuration

The per PF configuration in the main VFd configuration file allows the configuration of system-wide administrative settings for traffic class prioritization and the allocation of bandwidth to different traffic classes. The bandwidth allocated to each TC can then be apportioned across different VFs using the per-VF configuration settings below.

The configuration follows the following format. The design principle is to expose as many DCB settings as possible directly to the administrator to allow flexible policy choice for the system administration. An example of how the settings can be used to create a particular policy is shown below.

__[TODO: KJ:] The actual granularity with which these percentages are specified may be limited based on credit allocation. Needs to be worked out.__

    # Allow definition of multiple bandwidth groups. Each BWG can be allocated
	# a fixed amount of bandwidth share. If a BWG doesn't use it's share, other
	# BWGs can use it.
    priority_bwg:              # BWG name can be specified by administrator
	    classes: tc3
	    min_bw: 1%
    private_bwg:
	    classes: tc1,tc2       # tc1 and tc2 can reuse each others unused bandwidth
	    min_bw: 69%       
    default_bwg:
		classes: tc0
		min_bw: 30%

    # Each traffic class can now be configured independently
    tc3: signaling                    # TC name can be specified by administrator
		link_strict_priority: disable # (default: disable)
	    bwg_strict_priority: enable   # (default: disable)
		max_bw: 0.01%                 # (default: 100%)
	tc2: realtime
		link_strict_priority: disable
		bwg_strict_priority: disable
		min_bw: 39%                   # (default: 100/num_tcs)
		max_bw: 100%
	tc1: vpn
		link_strict_priority: disable
		bwg_strict_priority: disable
		min_bw: 30%
	tc0: internet
		link_strict_priority: disable
		bwg_strict_priority: disable
		min_bw: 30%

    # RX priority map (two PCP values are allocated to each TC to allow In-contract/out-of-contract marking
    priority_map: {0:tc0, 1:tc0, 2:tc1, 3:tc1, 4:tc2, 5:tc2, 6:tc3, 7:tc3}

Each TC must be assigned to a bandwidth group. Each bandwidth group is given a share of the link (`min_bw`). If a BWG doesn't use its share, then other BWGs can. If the sum of BWG `min_bw` is less than 100%, then they will be allocated the excess bandwidth proportionally. There is a no `max_bw` parameter for a BWG. 

Each TC can be configured separately and be given a `min_bw` within its BWG. The sum of all `min_bw` of TCs belonging to a BWG must total the `min_bw` of the BWG. Otherwise, the TCs will be allocated bandwidth in proportionally to their weights within the group.

A TC having `link_strict_priority` (LSP) enabled will be given strict priority for the link *except* for traffic belonging to higher number TCs. Therefore, to have a globally strict priority TC, it should be enabled for the highest numbered traffic class. For such TCs, the `min_bw` field is superfluous (they can consume the entire link bandwidth not already consumed by a higher priority class), but the `max_bw` will still be honored on a per VF basis using the VM rate limiter. In other words, no single VF can transmit more than `max_bw` in a TC configured using LSP. Note that if an LSP class is configured without any `max_bw`, then lower priority classes cannot be guaranteed to receive their share of min bandwidth.

A TC with `bwg_strict_priority` (GSP) is similar to LSP, but its scope is within a BWG. I.e., it can consume all the bandwidth not already consumed by higher priority TCs within its bandwidth group within the current round. However, a GSP TC will not consume more than its BWG's min_bw under contention conditions. So, similar to LSP, `min_bw` is not applicable for a GSP TC, but the `min_bw` of its BWG will still apply. The `max_bw` is again a per VF limit implemented using a per queue rate limiter. 

The priority map determines which TC each PCP value maps to for RX traffic. 

__[KJ TODO:] should we simplify by eliminating BWG entirely, and only allowing per TC configuration with strict_priority, min_bw, and max_bw fields?__

### Per VF Configuration

Per-VF QoS configuration can be provided within the VF's config file, and will determine how bandwidth is allocated to a particular VF compared to the other VFs on the host. It is expected that these values will be provided by tenants, but may go through an admission control layer to ensure that VNF demands do not exceed either the capacities of the PF, or the bandwidths allocated to each TC. To ensure that a VF's configuration can be specified independently of other VFs on a PF, the requirements are specified in terms of raw bandwidth (Gb/s or Mb/s) or as a percentage of the link linespeed (etc. 10% of 10G) rather than the weights used by the PF's Weighted Fair Queuing Scheduler. 

The configuration format is as follows:

    # All fields are optional. Values can be specified as raw bw (Mbps,Gbps) or as a percentage of
    # linespeed. If a field is omitted, the default values specified will be used.

    tc0_min_bw: 10Kbps          (Default value: min_bw_allocated_to_tc/num_max_vfs)
	tc0_max_bw: 2Gbps           (Default value: 100% or max_bw for TC, whichever is lower)

    tc1_min_bw: 0%
    tc1_max_bw: 0%

    tc2_min_bw: 1Gbps
    tc2_max_bw: 50%

    tc3_min_bw: 30%
    tc3_max_bw: 10Gbps

The min bandwidth indicates the bandwidth the VF will receive under congestion conditions on the NIC. It's value is upper bounded by the `min_bw` allocated to the corresponding TC. If VFd determines that there is not enough bandwidth to satisfy the `min_bw` request, it will raise an exception. If VFd is configured in the `strict` mode (as specified in the global VFd config file), then the VF creation or update request will fail. If VFd is configured in a `relaxed` mode, then the request will succeed, but the status of VFd will changed to `oversubscribed`. This status can be queried using the `iplex status` command described below. The oversubscription factor will be reported in the status. E.g., if 40% more min_bandwidth has been allocated than is available, then the link allocation will be reported as 140%. In such an oversubscribed scenario, each tenant may get proportionally less bandwidth than they asked for in the worst case. It is expected that there will be a higher level admission control layer (e.g., Valet/Tegu) that controls VM and VF allocation to control oversubscription.

The max bandwidth indicates the bandwidth received by the VF in the worst case, whether other VFs are using the link or not. This is implemented using a strict rate control per TX queue. It is expected that the max_bw parameter will usually be used by system administrators to throttle VFs that are causing congestion or incast issues elsewhere in the network (e.g., at a shared router). Therefore, the default is to disable the rate limiter by providing a default `max_bw` of 100%. There is no admission control applied to max bandwidth.


Port Mirroring
--------------

Port mirroring of all ingress/egress packets to/from a VF/VLAN to a mirror VF will be supported. More details forthcoming, including whether we can add filters based on IP 5-tuples.

Network Measurement
-------------------

Real-time network statistics for each VF configured using VFd will be provided through the `iplex status` command. These will include RX/TX/Drop/Error counts for the VF for each TC. In addition, VFd will also report the allocation status of the PF (i.e., level of oversubscription). Details still to be fleshed out.


State Persistence
-----------------

TBD. Currently, a VFd restart causes network drivers in the guest VMs to loose connectivity because the VF DMA register configurations are lost. VFd may save the contents of DMA registers on exit and reload them on restart to support restarting or upgrading VFd transparently to the guest VMs. The long term feasibility of this feature is still being debated because different NICs may have large differences in what needs to be loaded/stored.



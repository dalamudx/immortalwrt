# VEP1400 → RB5009 routing with FRR (ImmortalWrt 25.12.2)

The VEP1400 x86/64 image selects `mihomo` v1.19.32 (based on
[douglarek/mihomo-openwrt](https://github.com/douglarek/mihomo-openwrt),
pinned to `68f9c94`), `frr`, `frr-zebra`, `frr-watchfrr`, `frr-staticd`,
`frr-bgpd`, `frr-ospfd`, `frr-pythontools`, `kmod-tun`,
`kmod-netlink-diag` and `kmod-inet-diag`.

Mihomo's UCI section is disabled by default. FRR's packaged
`/etc/frr/daemons` has `bgpd=no` and `ospfd=no`. The image does not
configure an eBGP peer, OSPF neighbor or proxy endpoint. Enable those
only after validating the RouterOS configuration and TUN behavior.

## Data flow and trade-offs

`https://git.aoodc.com/dev/nchnroutes/releases/download/latest/routes4.conf`
currently contains 13,409 BIRD static routes. Strictly equivalent
aggregation leaves 13,380 prefixes; BGP **does not reduce the number
of prefixes stored on the RB5009UG**. An initial BGP session or a
reconnect transfers all prefixes; monitor RB5009 memory and forwarding
performance during a staged rollout.

    routes4.conf (BIRD syntax)
        → validate and translate to FRR `ip route PREFIX tun0 tag 4242`
        → staticd and zebra install selected routes on VEP1400
        → bgpd redistributes only tagged routes via a route-map
        → RB5009 learns next hop 10.10.10.254 over eBGP
        → RB5009 clients send matching traffic to VEP1400 / mihomo tun0

The published `via "tun0"` is a local interface reference. The RB5009
must learn the VEP1400 management IP as next hop, not the string `tun0`.
Keep the route generator and RouterOS import policy restricted to this
published set: do not advertise the default route, local/management
prefixes, or other static routes. Check that the remote proxy server
and DNS remain reachable without going through the TUN.

FRR accepts an ordinary static configuration file (unlike BIRD it does
not directly parse `route ... via "tun0";`). Convert and validate the
published data, compose the full intended `/etc/frr/frr.conf` including
unchanged base/router/route-map sections, and apply it using the packaged
`frr-reload` tool (`frr-pythontools`). `vtysh -f` alone is not a
full-state reconciliation: deleted file entries need corresponding
removals. On successful reload, bgpd normally sends incremental
announcements and withdrawals while the session stays established. For
13,409 routes, measure `frr-reload` time and avoid frequent whole-table
changes.

`frr-ospfd` is available for the requested OSPF capability. Do not
redistribute this whole table into OSPF: it would generate thousands of
external LSAs. If OSPF is used for a smaller internal topology, keep
its export policy separate from the large eBGP feed.

## Before activation

1. Confirm the RB5009's existing BGP peers/ASNs and choose unused ASNs
   for this peering. Restrict TCP 179 to the two routers; import from
   this peer only, reject default/private/management prefixes, and set
   a prefix limit with safe headroom if RouterOS supports it.
2. Configure mihomo's `tun0` with a stable name. Its upstream sample
   enables `auto-route` and `auto-redirect`; disable global routing
   changes if FRR is to install only selected routes. Test that proxy
   egress, DNS, management and the return path work before advertising.
3. Validate the download over TLS, verify each IPv4 prefix and the
   entire expected file size/count, and reject truncated or malformed
   updates. Convert only valid `route PREFIX via "tun0";` entries.
   Build a complete candidate FRR configuration in the same filesystem.
   Run `/usr/sbin/frr-reload --test CANDIDATE` to inspect the delta,
   then `/etc/init.d/frr reload CANDIDATE` to apply it while the daemons
   stay running. Only after a successful reload, atomically replace
   `/etc/frr/frr.conf` with the candidate for the next boot. Keep a
   previous copy for rollback. The dry-run reports a delta; it does
   not prove that the proxy or RouterOS forwarding works.
4. Observe `vtysh -c 'show ip bgp summary'`, `vtysh -c 'show ip route
   static'`, RouterOS peer status and imported prefix count. Validate
   actual forwarding for a few destinations, then increase the route
   count. A live `tun0` alone does not prove the proxy is healthy:
   withdraw the advertisements on proxy failure to prevent blackholes.

A small illustrative FRR policy (adjust RouterOS IP, ASNs, tag, ACLs,
route-map details and static route syntax after checking FRR 10.4.1):

```frr
ip route 1.224.0.0/11 tun0 tag 4242
!
route-map ONLY-NCHN permit 10
 match tag 4242
!
router bgp 64512
 bgp router-id 10.10.10.254
 neighbor 10.10.10.1 remote-as 64513
 address-family ipv4 unicast
  redistribute static route-map ONLY-NCHN
 exit-address-family
```

This example is **not installed or enabled** in the image. The packaged
`bgpd=no` must be changed only after the route policy is verified and
TUN/proxy availability is monitored. Start with a handful of prefixes,
then test load and failure recovery on the RB5009 before full import.

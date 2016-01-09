TODO
====

* Simplify command line arguments
  - Use -p, in in.[t]ftpd mode, to denote -p PORT
  - Add -s LEVEL to setlogmask(), or let -d control this?
* Setup signed .deb repository on deb.troglobit.com
* Port to *BSD (Free/Net/Open) -- requires kqueue support in libuEv
* Add TFTP retransmit support and inactivity timer, see
  http://tools.ietf.org/html/rfc2349
* Add support for IPv6
* Update Coverity Scan model to skip intended constructs
* Add uftp client, with .netrc support
  - See netrc(5) for details of format.
  - Build small CLI library using editline.


# ServiceManager

Open TeraDesk Options → Network options, or D:/APPS/SERVICES/SERVICES.APP
(Services on a fresh desktop). The menu brings an existing ServiceManager
window forward instead of starting another instance. ServiceManager remains
a separate application and continues to work with other desktops.

Networking: enable/disable Ethernet, set the hostname, choose DHCP or static
IPv4, and enter address, subnet mask, optional gateway and optional DNS server.
DHCP greys out the static fields and shows the current en0 IP address; it refreshes
roughly every two seconds. Static values are retained when switching modes.
Sharing: enable the background COM3 UART receiver (115200 baud), enable FTP,
and choose a dedicated FTP username and password. FTP and UART provide C:/D:
file maintenance. Use trusted local connections; FTP is unencrypted.

Click fields to edit, or use Tab/Shift-Tab. Passwords are masked. Enter matching
new passwords of 8-128 characters; leave both blank to retain the current hash.
Invalid lengths and mismatched entries show a dialog before any save or service
restart. The app retains the masked input and focuses the field to correct.
The app never stores a plaintext password or passes it on the command line.
An account name already owned by another user is rejected. Root login remains
locked; the dedicated FTP account has full-write maintenance access.

Save and apply persists settings and applies them in a background worker;
network changes restart Ethernet and FTP; FTP account/service changes restart
FTP. These actions disconnect existing FTP sessions. UART-only changes leave
Ethernet and FTP running. The GUI
remains usable and closing it does not stop enabled services. DHCP can take time
to acquire an address; successful apply means the services were started, not that
a lease or cable link has been confirmed. Reload discards unsaved edits.

Settings: /etc/a2560me-services.conf; password hash: /etc/passwd.
Startup: D:/MINT/NET/NETUP.SH, FTPUP.SH and SERVICES.SH --boot.
Errors: /var/log/services-apply.log, dhclient.log, inetd-ftp.log and uartrecv.log.
Only the optional UART receiver is added by the SERVICES boot hook; network and
FTP keep their normal startup hooks. With no settings file, existing DHCP and
transfer-account behavior is retained. UART is off until enabled.

Preserve the service configuration, password database and disabled markers when
updating drives. No per-machine configuration is shipped in generic releases.
Do not run a second UARTRECV manually while the background receiver is enabled.
The receiver has an exclusive lock to prevent two instances consuming one UART.
Use the existing workstation scripts/uart-upload.py sender with COM3's lab
transport; no UART protocol change is required.

Hardware validation (2026-10-27): switched from DHCP to static IPv4 through
ServiceManager and tested Cloudflare (1.1.1.1) and Google (8.8.8.8) separately.
Both passed interface/routing checks, gateway ping, and a fresh native curl
hostname lookup and HTTP 200 download, with DHCP stopped and only the selected
DNS server configured. No machine-specific network profile is shipped.

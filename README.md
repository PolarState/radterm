#README
RaDtErM!! ^.^
Serial Terminal for Macs!

currently requires root privilege (probably not for a good reason)

## Options
* -b --baud <num>      specify the baud rate, defaults to 9600
* -p --port <path>     specify the port. If empty, tries to guess based on your platform
* -t                   every new line shows time in ms.
* -i --pidfile <path>  specify the pidfile location
* -g --logfile <path>  specify the logfile location
* -v --verbose         enable verbose output

## Example Usage:
sudo ./radterm -t -b 15200 -p /dev/tty.*
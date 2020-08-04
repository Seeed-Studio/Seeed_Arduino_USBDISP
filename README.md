# Seeed Arduino USB Display

USB Display firmware for Wio Terminal


## WIKI
Get more detail about Wio Terminal as USB Display from: 
[WIKI](https://wiki.seeedstudio.com/Wio-Terminal-HMI)


## Get Started
Download the Seeed_Arduino_USBDISP library to your local.
There are two examples, NullFunctional and USBDisplayAndMouseControl:

1. If you want higher screen refresh rate on Wio Terminal, upload NullFunctional to Wio Terminal.

2. If you want Wio Terminal to also act as a USB Mouse, upload USBDisplayAndMouseControl to Wio Terminal.


## Note
The USBDISP().begin(bool reverse, bool usermode) function has two parameters.<br>
In our examples, the default setting is USBDISP().begin(true)<br>
If you want to parse drawing function in usermode-sdk or python-demo(seeed-linux-usbdisp),<br>
you need to set it as USBDISP().begin(true, true)<br>


## Contributing
Contributing to this software is warmly welcomed. You can do this basically by<br>
[forking](https://help.github.com/articles/fork-a-repo), committing modifications and then [pulling requests](https://help.github.com/articles/using-pull-requests) (follow the links above<br>
for operating guide). Adding change log and your contact into file header is encouraged.<br>
Thanks for your contribution.

Seeed Studio is an open hardware facilitation company based in Shenzhen, China. <br>
Benefiting from local manufacture power and convenient global logistic system, <br>
we integrate resources to serve new era of innovation. Seeed also works with <br>
global distributors and partners to push open hardware movement.<br>

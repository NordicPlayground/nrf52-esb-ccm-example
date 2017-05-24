nrf-esb-ccm-example
===================
This example implements a simple encryption library based on the CCM module. 

The example is based around the ESB PTX and PRX example, and is set up to transmit an encrypted data stream from the PTX to the PRX. 

The encryption is enabled by the PTX sending a random 16 byte key and a random 8 byte IV to the PRX, which is used to initialize the CCM module and encrypt the following packets. 

Both the PTX and PRX boards will output status updated over the UART log, which can be watched in a terminal on the PC:    
Baudrate: 115200      
Hardware flow control off, 1 stop bit, no parity

The encrypted data will include the state of button 1, 2 and 3 in the PTX which will be displayed on LED 1, 2 and 3 on the PRX. 

Pressing button 4 on the PTX will send a new pairing request packet, with a new random key and IV. Useful if the PRX has reset, or if the encrypted link is broken for some other reason. 

The log output on the PRX side will show the incoming encrypted payload as well as the decrypted data. 

Requirements
------------
- nRF5 SDK version 13.0.0
- 2x nRF52 DK (PCA10040)
- Keil uVision v5

TODO
----
- Implement encrypted key exchange
- Update makefile and IAR project

Note
----

The project may need modifications to work with other versions or other boards. 

To compile it, clone the repository in the *[SDK]/examples/peripheral/* folder.

About this project
------------------
This application is one of several applications that has been built by the support team at Nordic Semiconductor, as a demo of some particular feature or use case. It has not necessarily been thoroughly tested, so there might be unknown issues. It is hence provided as-is, without any warranty. 

However, in the hope that it still may be useful also for others than the ones we initially wrote it for, we've chosen to distribute it here on GitHub. 

The application is built to be used with the official nRF5 SDK, that can be downloaded from developer.nordicsemi.com

Please post any questions about this project on: 
[https://devzone.nordicsemi.com](https://devzone.nordicsemi.com)
<!-- Please do not change this html logo with link -->
<a href="https://www.microchip.com" rel="nofollow"><img src="images/microchip.png" alt="MCHP" width="300"/></a>

# TCP/IP Lite solutions using ETHxxJ60

This repository provides MPLAB X IDE projects that can work out of the box. The solutions that are included in the repository include functionality for UDP, TCP Server and TCP Client Demos. Note that the TCP/IP Lite stack needs to be serviced every 1 second and the timer callback function needs to be set to 1 second.

---

## Related Documentation

More details can be found at the following links:
- [Microchip Ethernet Controllers](https://www.microchip.com/design-centers/ethernet/ethernet-devices/products/ethernet-controllers)
- [PIC18F67J60](https://www.microchip.com/wwwproducts/en/PIC18F67J60)
- [IoT PoE Development Board](https://www.microchip.com/design-centers/ethernet/ethernet-of-everything/get-started)


## Software Used

- MPLAB® X IDE v5.40 or later [(microchip.com/mplab/mplab-x-ide)](http://www.microchip.com/mplab/mplab-x-ide)
- MPLAB® XC8 v2.20 or later [(microchip.com/mplab/compilers)](http://www.microchip.com/mplab/compilers)
- TCP/IP Lite Stack v3.0.0
- Ethernet Drivers Library v3.0.0
- TCPIP Demo GUI v2.0
- [Wireshark Tool](https://www.wireshark.org/)

## Hardware Used

- PIC18 PoE Main Board [(dm160230)](https://www.microchip.com/developmenttools/ProductDetails/dm160230)
- [PIC18F67J60](https://www.microchip.com/wwwproducts/en/PIC18F67J60)

---

## UDP Solution

1. Open MPLABX IDE.
<br><img src="images/mplabIcon.png" alt="mplabIcon" width="75"/>
2. From the downloaded projects, open ethxxj60-udp-solution.X.
3.	Open Windows Command Prompt application on your PC. Type “ipconfig” to get the IP address of your PC.
<br><img src="images/ethxj60/udpSolution/ipconfig.png">
4.	Sending UDP Packets: In udp_demo.c, under the function UDP_DEMO_Send ():
    - Modify the Destination IP address with PC’s IP address as noted in Step 3.
    - Destination Port (anything in the range of dynamic ports)
<br><img src="images/ethxj60/udpSolution/destinationPort.png">
5.	Receiving UDP Packets: In Source Files\MCC generated files\ udpv4_port_handler_table.c, add the following code:
    - In UDP_CallBackTable, add the following code to perform UDP Receive:
<br><img src="images/ethxj60/udpSolution/udpReceive.png">
6.	Click on Program Device to program the code to the device.
7.	Launch Wireshark. From the Capture menu, click Options.
<br>Select an Interface from the list to which your board and PC are connected, click Start for capturing packets.
<br>e.g.: Local Area Network
8. In Wireshark, notice the DHCP packets handshake to get the device IP address.
<br><img src="images/ethxj60/udpSolution/wiresharkDHCPCapture.png">
9.	In Wireshark, double click on “Offer” packet. Expand “Dynamic Host Configuration Protocol” to get the device IP address.
<br><img src="images/ethxj60/udpSolution/DHCPPacket.png">
10. Open the Java application TCPIP_Demo.exe. Go to the UDP tab and assign the same port number as ‘DestPort’ (65531). Click on ‘Listen’ button. Click “Allow Access” if warning occurs. Assign the IP Address of your board which is found from step 9(192.168.0.37). Click on ‘Claim’ button.
<br><img src="images/ethxj60/udpSolution/udpDemoGUI.png">	
11.	In Wireshark, set the filter field as bootp||udp.port==65531.
12.	In Demo GUI, type data(e.g.: “PIC8F67J60”)inside Send Data box and press the Send button and observe the Wireshark capture.
<br><img src="images/ethxj60/udpSolution/udpWiresharkSend.png">
13.	On the IoT PoE Main Board, press the Switch S1 and observe the Wireshark capture.
<br><img src="images/ethxj60/udpSolution/udpWiresharkReceive.png">

---

## TCP Client Solution

1. Open MPLABX IDE.
<br><img src="images/mplabIcon.png" alt="mplabIcon" width="75"/>
2. From the downloaded projects, open ethxxj60-tcp-client-solution.X.
3.	Click on Program Device to program the code to the device.
4. Launch Wireshark. From the Capture menu, click Options.
<br>Select an Interface from the list to which your board and PC are connected, click Start for capturing packets.
<br>e.g.: Local Area Network
5. In Wireshark, set the filter field as bootp||tcp.port==65534.
6.	Open the Java application TCPIP_Demo.exe. Go to the TCP Server Demo tab and assign the port number as ‘65534’. Click on ‘Listen’ button. The status of the TCP Connection is printed inside the STATUS text box.
<br><img src="images/ethxj60/tcpClientSolution/tcpClientSolutionGUI.png">
<br><img src="images/ethxj60/tcpClientSolution/tcpClientWiresharkPacket.png">
7.	After the connection is established:
    - Type text inside the Send text box and click on ‘Send’ button. The text sent is displayed inside the Sent/Received Data box.
<br><img src="images/ethxj60/tcpClientSolution/tcpClientSend.png">
<br><img src="images/ethxj60/tcpClientSolution/tcpClientWiresharkSend.png">
8.	Push ‘Disconnect’ button, to close the TCP Connection. A client disconnected message will appear on the STATUS text box.  

---

## TCP Server Solution

1. Open MPLABX IDE.
<br><img src="images/mplabIcon.png" alt="mplabIcon" width="75"/>
2. From the downloaded projects, open ethxxj60-tcp-server-solution.X.
3.	Click on Program Device to program the code to the device.
4. Launch Wireshark. From the Capture menu, click Options.
<br>Select an Interface from the list to which your board and PC are connected, click Start for capturing packets.
<br>e.g.: Local Area Network
5. In Wireshark, notice the DHCP packets handshake to get the device IP address.
<br><img src="images/ethxj60/tcpServerSolution/wiresharkDHCPCapture.png">
6.	In Wireshark, double click on “Offer” packet. Expand “Dynamic Host Configuration Protocol” to get the device IP address.
<br><img src="images/ethxj60/tcpServerSolution/DHCPPacket.png">
7.	Open the Java application TCPIP_Demo.exe. Go to the TCP Client Demo tab, assign the device IP address as Server IP address in the GUI. Assign the port number as ‘7’. Click on ‘Connect’ button. The status of the TCP Connection is printed inside the STATUS text box.
<br><img src="images/ethxj60/tcpServerSolution/tcpServerDemoGUI.png">
8.	After the connection is established:
    - Type text inside the Send text box and click on ‘Send’ button. The text sent is echoed and displayed inside the Sent/Received Data box.
<br><img src="images/ethxj60/tcpServerSolution/tcpServerSend.png">
    - Also in Wireshark, observe the TCP packets by setting the filter “tcp.port == 7”
    <br><img src="images/ethxj60/tcpServerSolution/tcpServerWiresharkSend.png">
9.	Push ‘Disconnect’ button, to close the TCP Connection. TCP server closing the connection message will appear on the STATUS text box.

---
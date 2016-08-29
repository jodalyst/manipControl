# manipControl
Simple header file to control Sutter MP-285 (MCP-2000, etc...) micromanipulators with MATLAB. Works at least as far back as MATLAB 2007 up through MATLAB 2016b

This code must be compiled for MATLAB on your own machine. To do this, drag the file into your MATLAB path.  Download the appropriate D2xx drivers from here: http://www.ftdichip.com/Drivers/D2XX.htm  (in particular you'll need the ftd2xx.lib file)

then run:

mex manipControl.cpp -v ftd2xx.lib

or something similar and then it should build (depending on your MATLAB version you may need a -l command preceding the ftd2xx.lib 

Once installed you have the following commands at your disposal:

Commands:
manipControl('numDevices'): return the number of FTD2XX devices connected to the computer.
manipControl('deviceName',deviceNumber): return the name of the FTD2XX device number <deviceNumber>.
manipControl('deviceSerial',deviceNumber): return the serial number of the FTD2XX device number <deviceNumber>
manipControl('initialize'): Initialize all the manipulators connected to the computer.  Returns a vector of the initialized device numbers.
manipControl('uninitialize'): Releases control of all the manipulators connected to the computer.
manipControl('changePosition',deviceNumber,driveNumber,X,Y,Z): Change the x,y,z position of the particular device and drive.
manipControl('getPosition',deviceNumber,driveNumber): Obtain the x,y,z position of the particular device and drive.

A note on what a "drive" and what a "device" is.  A device is the number of separate USB connected devices.  A "drive" is a  manipulator.  If these are individual MP-285 devices, then you have one drive per device. (indexed at 1).

If you have an MCP-2000, you have one device with two drives (so you'll always specify the same device, but do separate drive numbers to access/communicate with each manipulator. 

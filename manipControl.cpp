#include <stdio.h>
#include <string.h>
#include <math.h>
#include "mex.h"
#include <windows.h>
#include "ftd2xx.h"

//Original code written by Chris Rohde for Lambda SC Shutter (DATE)
//This code was rewritten by Joe Steinmeyer for use with MPC-2000/ROE-200N (4/24/2009)

// Constants
#define FUNC_NAME "ManipControl"	//The name of the function (as seen by Matlab)
#define FUNC_VER 1.10				//The current version of the manipulator control software
#define maxManips 16				//The maximum number of manipulators that can be controlled using this program

// Function prototypes
FT_STATUS numberOfDevices(DWORD*);
FT_STATUS numberOfManips(DWORD*);
FT_STATUS deviceDescription(DWORD, char*);
FT_STATUS deviceSerial(DWORD, char*);
int isManip(DWORD);
void getCommands();
FT_STATUS getHandle(DWORD, FT_HANDLE*);
FT_STATUS where(FT_HANDLE,int,int*,int*,int*);
FT_STATUS move(FT_HANDLE,int,int,int,int);
FT_STATUS driveChange(FT_HANDLE,int);


void mexFunction(int nlhs, mxArray *plhs[], int nrhs, const mxArray *prhs[])
{

	DWORD numDevs;									// Return value for querying the number of devices attached
	DWORD numManips;								// Return value for querying the number of manipulator controllers attached
	DWORD devNum;									// Value to input the device we're interested in accessing
    DWORD driveNum;                                  //Value to input the drive we're interested (Used exclusively for MPC-2000/ROE-200N)	

	mxArray *mxCommandStr;								// Command string: Matlab data
	int cmdStrLen;										// Command string: length
	char *commandStr;									// Command string: converted C character array	
	double *outVal;										// pointer to single value to return
	double *outArray;									// pointer to array to return
    int x_des;
    int y_des;
    int z_des;
	
	//Configuration parameters
	static int initialized = 0;							// whether the manips have been initialized	
	static int numHandles = 0;							// number of entries in manipHandles[]
	static FT_HANDLE manipHandles[maxManips];		    // Array containing handles
	static int deviceNumbers[maxManips];				// Array containing the device number corresponding to the handle in manipHandles[]	

	//ensure that we have at least one input parameter, and if so, get the first parameter (the command string)
	if (nrhs < 1) {
		mexPrintf("Error: %s requires at least one parameter (the command string)\n",FUNC_NAME);
		return;
	}

	//Get the command string
	mxCommandStr = (mxArray*)prhs[0];
	cmdStrLen = (int)mxGetN(mxCommandStr)+1;
	commandStr = (char*)mxMalloc(cmdStrLen*sizeof(char));
	mxGetString(mxCommandStr,commandStr,cmdStrLen);

	// The following 5 commands don't require the manip 
	//Command: Get Devices (no more parameters)
	if (strcmp(commandStr,"numDevices") == 0) {	
		numberOfDevices(&numDevs);
		plhs[0] = mxCreateDoubleMatrix(1,1,mxREAL);
		outVal = mxGetPr(plhs[0]);
		outVal[0] = numDevs;
	}
	//Command: Get the number of devices that are manipulators
	else if (strcmp(commandStr,"numManips") == 0) {	
		numberOfManips(&numManips);
		plhs[0] = mxCreateDoubleMatrix(1,1,mxREAL);
		outVal = mxGetPr(plhs[0]);
		outVal[0] = numManips;
	}
	//Command: Get device name (device number)
	else if (strcmp(commandStr,"deviceName") == 0) {	
		//We require another parameter, so ensure it's been specified		
		if (nrhs != 2) {
			mexPrintf("Error. Using 'deviceName' requires exactly one other input parameter (the device number)\n");			
		} else if (initialized == 1) {
			mexPrintf("Error. 'deviceName' cannot be accessed once a device has been initialized\n");
			char *devName;
			mwSize buflen = 64;		
			devName=(char*)mxCalloc(buflen, sizeof(char));
			strcpy(devName,"error");
			plhs[0] = mxCreateString(devName);
		}
		else {
			//Get the other parameter (should be the device number)
			devNum = (DWORD)*mxGetPr(prhs[1]);
			// allocate memory for output string
			char *devName;
			mwSize buflen = 64;		
			devName=(char*)mxCalloc(buflen, sizeof(char));
			// get device name 
			deviceDescription(devNum, devName);
			// return the device name
			plhs[0] = mxCreateString(devName);
		}
	}
	//Command: Get serial number(device number)
	else if (strcmp(commandStr,"deviceSerial") == 0) {	
		//We require another parameter, so ensure it's been specified		
		if (nrhs != 2) {
			mexPrintf("Error. Using 'deviceSerial' requires exactly one other input parameter (the device number)\n");			
		}
		else if (initialized == 1) {
			mexPrintf("Error. 'deviceSerial' cannot be accessed once a device has been initialized\n");
			char *devName;
			mwSize buflen = 64;		
			devName=(char*)mxCalloc(buflen, sizeof(char));
			strcpy(devName,"error");
			plhs[0] = mxCreateString(devName);
		}
		else {
			//Get the other parameter (should be the device number)
			devNum = (DWORD)*mxGetPr(prhs[1]);
			// allocate memory for output string
			char *devName;
			mwSize buflen = 64;		
			devName=(char*)mxCalloc(buflen, sizeof(char));
			// get device name 
			deviceSerial(devNum, devName);
			// return the device name
			plhs[0] = mxCreateString(devName);
		}
	}
	//Command: help (no more parameters). List all possible commands
	else if (strcmp(commandStr,"help") == 0) {	
		getCommands();
	}
	//##########################################################################
	//Command: initializeAllManips (no more parameters). Returns device numbers of MPC2000s
	//##########################################################################
	else if (strcmp(commandStr,"initialize") == 0) {
		//Don't run this routine if we're already initialized
		if (initialized == 1) {
			plhs[0] = mxCreateDoubleMatrix(1,1,mxREAL);
			outArray = mxGetPr(plhs[0]);
			outArray[0] = 0;
			mexPrintf("Already initialized.  Uninitialize first if you want to re-run this routine\n");
		}
		else {			
			DWORD numDevs;
			DWORD numManips = 0;

			numberOfDevices(&numDevs);

			//get handles to the devices that are manipulators 
			for (DWORD i=0;i<numDevs;i++) {
				if (isManip(i) != 0) {
					getHandle(i, &manipHandles[numManips]);
					deviceNumbers[numManips] = i;
					mexPrintf("%u is a manipulator\n",numManips);
					numManips++;
				}
			}
			numHandles = numManips;			
			initialized = 1;
			mexPrintf("Initialized %u manipulators\n", numHandles);
			// now return the device numbers of the manipulators
			plhs[0] = mxCreateDoubleMatrix(numHandles,1,mxREAL);
			outArray = mxGetPr(plhs[0]);
			for (int i=0;i<numHandles;i++)
				outArray[i] = deviceNumbers[i];
		}
	}
	//##########################################################################
	//Command: uninitialize (no more parameters)
	//##########################################################################
	else if (strcmp(commandStr, "uninitialize") == 0) {
		//Don't run this routine if we're not initialized
		if (initialized == 0) {
			mexPrintf("Not initialized.\n");
		}
		else {
			for (DWORD i=0; i<numHandles; i++) {
				FT_Close(manipHandles[i]);
			}
			mexPrintf("Uninitialized %u manipulators\n", numHandles);
			numHandles = 0;
			initialized = 0;
		}
	}
	//Command: getDevicePosition (device number, drive number)
    //this command will return the values of the x
	else if (strcmp(commandStr, "getPosition") == 0) {
        //Don't run this routine if we're not initialized
         if (initialized == 0) {
             mexPrintf("Not initialized.\n");
        }
         //We require two other parameters, so ensure it's been specified
         else if (nrhs != 3) {
              mexPrintf("Error. Using 'getPosition' requires exactly two other input parameters (the device number and the drive number, respectively)\n");
         }
         else {
              int exists = 0;
              //Get the Device number
              devNum = (DWORD)*mxGetPr(prhs[1]);
              //Get the Drive Number
              int driveNum;
              driveNum = (DWORD) *mxGetPr(prhs[2]);
              
              //create and initialize the input values...only kind of needed
              int x;
              x = 0;
              int y;
              y = 0;
              int z;
              z = 0;
              
              where(manipHandles[devNum],driveNum,&x,&y,&z);
              
              plhs[0] = mxCreateDoubleMatrix(1,3,mxREAL);
              outVal = mxGetPr(plhs[0]);
              outVal[0] = x;
              outVal[1] = y;
              outVal[2] = z;   
         }
    }
    //Command changePosition (device number, drive number, x,y,z position)
    //The x, y, and z values are in STEPS!!!! NOT MICRONS...so they can only be from 0 to 400e3!
    else if (strcmp(commandStr, "changePosition") == 0) {
       //Don't run this routine if  we're not initialized
        if (initialized == 0) {
              mexPrintf("Not initialized.\n");
         }
         //We require five other parameters, so ensure it's been specified
         else if (nrhs != 6) {
              mexPrintf("Error. Using 'changePosition' requires exactly 5 other input parameters (device number, drive number, x coord, y coord, z coord, IN THAT ORDER)\n");
         }
         else {
         //Get the device number
         devNum = (DWORD) *mxGetPr(prhs[1]);
         
         driveNum = (DWORD) *mxGetPr(prhs[2]); //we'll just assume devNum and driveNum are in range...yes this is sloppy...fuck you.
         
         x_des = (int) *mxGetPr(prhs[3]);
         y_des = (int) *mxGetPr(prhs[4]);
         z_des = (int) *mxGetPr(prhs[5]);
         
         if (x_des < 0 || x_des > 400e3) {
             mexPrintf("Error.  x value for 'changeposition' is out of valid range!  0 <= x <=400e3.\n");
         }
         if (y_des < 0 || y_des > 400e3) {
             mexPrintf("Error.  y value for 'changeposition' is out of valid range!  0 <= y <=400e3.\n");
         }
         if (z_des < 0 || z_des > 400e3) {
             mexPrintf("Error.  z value for 'changeposition' is out of valid range!  0 <= z <=400e3.\n");
         }
         
         
         move(manipHandles[devNum],driveNum,x_des,y_des,z_des);
         
         }
    }
	else
		mexPrintf("Invalid command.  Type %s('help') to see command listing.\n",FUNC_NAME);

	// Finally, free the memory we used to create the command string
	mxFree(commandStr);
}

//##############################################################################
//################HOW MANY DEVICES/OF A CERTAIN KIND????########################
//##############################################################################

// Return the number of devices connected to the computer
FT_STATUS numberOfDevices(DWORD* numDevs)
{
	FT_STATUS ftStatus;	

	// Get the number of connected devices.  There should be two devices
	ftStatus = FT_ListDevices(numDevs,NULL,FT_LIST_NUMBER_ONLY);
	if (ftStatus == FT_OK) {
		// FT_ListDevices OK, number of devices connected is in numDevs		
	}
	else {
		// FT_ListDevices failed
		*numDevs = 0;
		
	}
	return ftStatus;
}

FT_STATUS numberOfManips(DWORD* numManips)
{
	DWORD numDevs;
	FT_STATUS ftStatus;

	*numManips = 0;
	ftStatus = numberOfDevices(&numDevs);
	for (DWORD i=0;i<numDevs;i++) {
		if (isManip(i) != 0) {
			(*numManips)++;
		}
	}
	return ftStatus;
}
//##############################################################################
//######################RETURNS INFO ON DEVICE##################################
//##############################################################################
// Return the description (name) of the specified device
FT_STATUS deviceDescription(DWORD deviceNumber, char* deviceName)
{
	FT_STATUS ftStatus;	
	DWORD devIndex = deviceNumber; //we need to copy the device number for some reason
	char Buffer[64]; // more than enough room!

	ftStatus = FT_ListDevices((PVOID)devIndex,Buffer,FT_LIST_BY_INDEX|FT_OPEN_BY_DESCRIPTION);
	if (ftStatus == FT_OK) {
	// FT_ListDevices OK, description is in Buffer		
		strcpy(deviceName, Buffer);
	}
	else {
		mexPrintf("%s('deviceName'): Error getting data to manipulator 1",FUNC_NAME);
	}
	return ftStatus;
}

// Return the serial number of the specified device
FT_STATUS deviceSerial(DWORD deviceNumber, char* deviceName)
{
	FT_STATUS ftStatus;	
	DWORD devIndex = deviceNumber; //we need to copy the device number for some reason
	char Buffer[64]; // more than enough room!

	ftStatus = FT_ListDevices((PVOID)devIndex,Buffer,FT_LIST_BY_INDEX|FT_OPEN_BY_SERIAL_NUMBER);
	if (ftStatus == FT_OK) {
	// FT_ListDevices OK, description is in Buffer		
		strcpy(deviceName, Buffer);
	}
	else {
		mexPrintf("%s('deviceSerial'): Error getting data to manipulator 2\n",FUNC_NAME);
	}
	return ftStatus;
}

//##############################################################################
//##################CHECKS TO SEE DEVICE TYPES##################################
//##############################################################################

// Checks to see if the device is a Manipulator [1 yes, 0 no]
int isManip(DWORD deviceNumber) 
{
	char devDesc[64];
	deviceDescription(deviceNumber,devDesc);
	// we have a manip
	if (strcmp(devDesc,"Sutter Instrument ROE-200") == 0)
    //if (strcmp(devDesc,"Sutter Instrument Lambda SC") == 0)
		return 1;
	// the device is not a Manip
	else
		return 0;
}
//##############################################################################
//##############################################################################
//##############################################################################

// Create a handle to the device that allows writing
FT_STATUS getHandle(DWORD deviceNumber, FT_HANDLE *deviceHandle)
{
	FT_STATUS ftStatus;
    //FT_HANDLE ftHandle1;

	//Get the serial number of the device
	char devSerial[128];	
	// get device name 
	deviceSerial(deviceNumber, devSerial);
	mexPrintf("Getting handle to device %u (serial %s)\n", deviceNumber, devSerial);

    //ftStatus = FT_OpenEx("SIDDUI9M", FT_OPEN_BY_SERIAL_NUMBER, &ftHandle1);
	ftStatus = FT_OpenEx(devSerial, FT_OPEN_BY_SERIAL_NUMBER, deviceHandle);
    //ftStatus = FT_Open(0,deviceHandle);
	if (ftStatus != FT_OK) {
		mexPrintf("Couldn't open manipulator %u\n", deviceNumber);
		return ftStatus;
	}
    
    //*deviceHandle = ftHandle1;  
    
    //mexPrintf("Good So Far (2)\n");
	//Set up the channel properly
	FT_SetDataCharacteristics (*deviceHandle, FT_BITS_8, FT_STOP_BITS_1,FT_PARITY_NONE);
    //mexPrintf("Set Data Characteristics\n");
	FT_SetFlowControl (*deviceHandle, FT_FLOW_NONE, 0,0);
    //mexPrintf("Set Flow Control\n");
	FT_SetTimeouts(*deviceHandle,500,500);
    //mexPrintf("Set Timeouts\n");
	FT_Purge(*deviceHandle,FT_PURGE_RX|FT_PURGE_TX);
    //mexPrintf("Purged everything\n");
	FT_SetLatencyTimer(*deviceHandle,16);
    //mexPrintf("Set Latency Timer\n");
	FT_SetBaudRate(*deviceHandle,128000);
    //mexPrintf("Set Baud Rate\n");
	FT_SetUSBParameters(*deviceHandle,64,0);
    //mexPrintf("Set USB Parameters\n");
	
	//Maybe resetting the device will help???
	//FT_ResetDevice(*deviceHandle);

	//Write 238 (0xEE) to have the controller begin accepting commands	
	#define initWriteSize 1
	char TxBuffer[initWriteSize];	
	TxBuffer[0] = 238;
	DWORD bytesWritten;

	ftStatus = FT_Write(*deviceHandle, TxBuffer, initWriteSize, &bytesWritten);
	if (ftStatus != FT_OK){
		mexPrintf("%s('getHandle'): Error writing to manipulator %u\n",FUNC_NAME, deviceNumber);
    }
    else {
        mexPrintf("%s Successfully initially written to device.\n");
    }
	return ftStatus;
}

//##############################################################################
//##############################################################################


FT_STATUS where(FT_HANDLE ftHandle, int drive, int* xout, int* yout, int* zout) 
{
	FT_STATUS ftStatus;
		
	//Begin by flushing the buffer
	FT_Purge(ftHandle, FT_PURGE_RX | FT_PURGE_TX);
    
    DWORD bytesWritten;
    
    //change the manipulator drive, if needed...
    driveChange(ftHandle, drive);
   
	//Send the command to get the status
	char BytesToWrite[1];
    BytesToWrite[0]= 0x43;
    //The 'C' command which tells the manipulator to get current position hex: 0x43
    
	ftStatus = FT_Write(ftHandle, BytesToWrite , 1, &bytesWritten);
	if (ftStatus != FT_OK) {
		mexPrintf("%s('status'): Error writing");		
		return ftStatus;
	}
    //mexPrintf("Successfully wrote %u byte(s).  Specifically we sent: %s\n", bytesWritten, BytesToWrite);

	//Now we need to read the response			
	DWORD BytesReceived;  //use for reference and debugging
	//receive the values
    
    //input arrays:
    char RxBuffer[64];
    int input[32];
    //just to be safe, set all the values to zero!  Sloppy I know.
    input[0]=0;
    input[1]=0;
    input[2]=0;
    input[3]=0;
    input[4]=0;
    input[5]=0;
    input[6]=0;
    input[7]=0;
    input[8]=0;
    input[9]=0;
    input[10]=0;
    input[11]=0;
    
    ftStatus = FT_Read(ftHandle,&input[0],1,&BytesReceived);  //First byte is some new line garbage...read it and then forget it.
    ftStatus = FT_Read(ftHandle,&input[0],1,&BytesReceived);
    ftStatus = FT_Read(ftHandle,&input[1],1,&BytesReceived);
    ftStatus = FT_Read(ftHandle,&input[2],1,&BytesReceived);
    ftStatus = FT_Read(ftHandle,&input[3],1,&BytesReceived);
    ftStatus = FT_Read(ftHandle,&input[4],1,&BytesReceived);
    ftStatus = FT_Read(ftHandle,&input[5],1,&BytesReceived);
    ftStatus = FT_Read(ftHandle,&input[6],1,&BytesReceived);
    ftStatus = FT_Read(ftHandle,&input[7],1,&BytesReceived);
    ftStatus = FT_Read(ftHandle,&input[8],1,&BytesReceived);
    ftStatus = FT_Read(ftHandle,&input[9],1,&BytesReceived);
    ftStatus = FT_Read(ftHandle,&input[10],1,&BytesReceived);
    ftStatus = FT_Read(ftHandle,&input[11],1,&BytesReceived);
    //Read out whatever other garbage is sitting in the output buffer.
    ftStatus = FT_Read(ftHandle,&RxBuffer[0],14,&BytesReceived);  
    
    /*mexPrintf("0:%d 1: %d 2: %d 3: %d\n", input[0], input[1], input[2], input[3]);
    mexPrintf("0:%d 1: %d 2: %d 3: %d\n", input[4], input[5], input[6], input[7]);
    mexPrintf("0:%d 1: %d 2: %d 3: %d\n", input[8], input[9], input[10], input[11]);
    */
	//ftStatus = FT_Read(ftHandle,&RxBuffer[0],14,&BytesReceived);
	if (ftStatus == FT_OK) {
		//mexPrintf("Successfully read %u bytes.  Response: %s\n", BytesReceived, RxBuffer);
		//Bytes 0 through 3 are the first long (x) value IN REVERSE SIGNIFICANT BYTE ORDER, 
        //Bytes 4 through 7 are the second long (y) value IN REVERSE SIGNIFICANT BYTE ORDER,
        //Bytes 8 through 11 are the third long (z) value IN REVERSE SIGNIFICANT BYTE ORDER,
        //Byte 12 should be a carriage return  JDS (5/05/2009)
        
        //The above comment should be noted that the output stream from the manipulator consists of at at least 13 bytes
        //The first is some new line (binary: ...00010) and then we have the twelve bytes in ascending significant order.
        //There's probably a carriage return at the end of this but I really don't care at this point.  I will flush/delete
        //the buffer anyways so its no big deal.
		
        //Each drive moves in 62.5 nm steps, so take the data, weight it appropriately,
        //then assign it to the proper pointer.  The output is in steps...NOT MICRONS! OR NANOMETERS!
        //So *xout, *yout, and *zout have a range of 0 to 400e3 steps, which corresponds to 0 to 25 mm
        
        //Take each input and weight it according to its significance and then assign to the pointers supplied:
        
        *xout = (input[0]+256*input[1]+256*256*input[2] + 256*256*256*input[3]);
        
        *yout = (input[4]+256*input[5]+256*256*input[6] + 256*256*256*input[7]);
    
        *zout = (input[8]+256*input[9]+256*256*input[10] + 256*256*256*input[11]);
         
        return ftStatus;
    }
	else {
		mexPrintf("%s('status'): Error reading response\n",FUNC_NAME);
		return ftStatus;
	}		
}

FT_STATUS move(FT_HANDLE ftHandle, int drive, int x, int y, int z)
{
    FT_STATUS ftStatus;
		
	//Begin by flushing the buffer
	FT_Purge(ftHandle, FT_PURGE_RX | FT_PURGE_TX);
    
    DWORD bytesWritten;
    char RxBuffer[12];
    //Change the manipulator drive (if needed...)
    driveChange(ftHandle, drive);
    
    //Send the command to get the status
	char input[13];
    //The 'M' command which tells the manipulator to move, damnit. hex: 0x4D
    input[0] = 0x4D;
    
    input[1] = char(x);
    input[2] = char((x >> 8));
    input[3] = char((x >> 16));
    input[4] = char((x >> 24));
    
    input[5] = char(y);
    input[6] = char((y >> 8));
    input[7] = char((y >> 16));
    input[8] = char((y >> 24));
    
    input[9] = char(z);
    input[10] = char((z >> 8));
    input[11] = char((z >> 16));
    input[12] = char((z >> 24));
    
	ftStatus = FT_Write(ftHandle, input , 13, &bytesWritten);
    //Read out the carriage return from the manip
    ftStatus = FT_Read(ftHandle,&RxBuffer[0],3,&bytesWritten);
	if (ftStatus != FT_OK) {
		mexPrintf("%s('status'): Error writing");		
		return ftStatus;
	}
    else{
    //mexPrintf("Successfully wrote %u byte(s).  Specifically we sent: %s\n", bytesWritten, input);
    return ftStatus;
    }
    return ftStatus;
   
}

FT_STATUS driveChange(FT_HANDLE ftHandle, int drive)
{
    FT_STATUS ftStatus;
		
	//Begin by flushing the buffer
	FT_Purge(ftHandle, FT_PURGE_RX);
    
    DWORD bytesWritten;
   
    //create char array to hold commands for drive change
    char driveChange[2];
    driveChange[0] = 0x49;
    
    //determine which drive to change to
    if (drive == 1){
        driveChange[1] = 0x01;
    }
    else if (drive == 2){
        driveChange[1] = 0x02;
    }
    else {
        mexPrintf("%s('status'): Error reading response.  Drive value is incorrect in FT_STATUS_where\n",FUNC_NAME);
		return ftStatus;
    }
    
    //####################
    DWORD BytesReceived;
	//receive the values
    char RxBuffer[64];
    //####################
    
    //Change the drive...
    ftStatus = FT_Write(ftHandle, driveChange , 2, &bytesWritten);
	if (ftStatus != FT_OK) {
		mexPrintf("%s('status'): Error writing");		
		return ftStatus;
	}
    //Read out the carriage return from the manip
    ftStatus = FT_Read(ftHandle,&RxBuffer[0],3,&BytesReceived);
    //mexPrintf("Successfully read %u bytes.  Response: %s\n", BytesReceived, RxBuffer);
    
    if (ftStatus == FT_OK) {
        //mexPrintf("Drive Change written. Successfully wrote %u byte(s).  Specifically we sent: %s\n", bytesWritten, driveChange);
    }
    return ftStatus;
}

//##############################################################################
//#################FOR MANIPULATORS#############################################
//##############################################################################


void getCommands() {
	mexPrintf("%s version %0.2f\n",FUNC_NAME,FUNC_VER);
	mexPrintf("=================================\n");
	mexPrintf("\tAvailable Commands:\n");
	mexPrintf("=================================\n");
	mexPrintf("%s('numDevices'): return the number of FTD2XX devices connected to the computer.\n",FUNC_NAME);
	mexPrintf("%s('deviceName',deviceNumber): return the name of the FTD2XX device number <deviceNumber>.\n",FUNC_NAME);
	mexPrintf("%s('deviceSerial',deviceNumber): return the serial number of the FTD2XX device number <deviceNumber>.\n",FUNC_NAME);
	mexPrintf("%s('initialize'): Initialize all the manipulators connected to the computer.  Returns a vector of the initialized device numbers.\n",FUNC_NAME);
	mexPrintf("%s('uninitialize'): Releases control of all the manipulators connected to the computer.\n",FUNC_NAME);
    mexPrintf("%s('changePosition',deviceNumber,driveNumber,X,Y,Z): Change the x,y,z position of the particular device and drive.\n",FUNC_NAME);
    mexPrintf("%s('getPosition',deviceNumber,driveNumber): Obtain the x,y,z position of the particular device and drive.\n",FUNC_NAME);
} // void getCommands()
